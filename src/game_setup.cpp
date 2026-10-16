#include "game_setup.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

#include <rex/crypto/sha256.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "popups.h"

#include <rex/ui/windowed_app_context.h>

#include <memory>
#include <mutex>

namespace game_setup {
namespace {

namespace fs = std::filesystem;

constexpr size_t kMaxExtractorRecordSize = 4096;
constexpr size_t kExtractorOutputBufferSize = 4096;
constexpr unsigned kExtractorPollIntervalMs = 50;
constexpr unsigned kCancelGracePollCount = 10;
constexpr unsigned kMaxStagingAttempts = 100;

struct File {
  const char* name;
  const char* hash;
  size_t offset = 0;
  size_t length = 0;
};

constexpr std::array disc_base{
    File{"default.xex", "709EB9FD2E4446E5754461616693B3BBB8B78FEF5AA613FFCDE6D643DA0133F5"},
    File{"CA022100.bin", "A8454BB24A0C0F0A0E5C89C8885041DEE6411E2C688874A5E2A57966174939BE"},
    File{"CA022110.bin", "4C08B646140E4887AC2036208195E99DFDB4AE32FC7D1571D589B6657791BC75"}};

constexpr std::array god_base{
    File{"default.xex", "41AB091576813248712A387B273398E5D700454505E83F6F1FEB7DE2C1027707"},
    File{"CA022100.bin", "EC610171AE966D7B37D3E44FF40DD89EF1086DC88633C01EB390824154659769"},
    File{"CA022110.bin", "D0340C943EC0FCEEB65892B7F3451CA6B687D2282984FEAAC3D8D043BEEB2569"}};

constexpr std::array disc_patches{
    File{"CA022100.binp", "830EE751922483115F735F442A23B0AB203468DE17E509DA6ED08B25BE25DC4F", 53248, 126976},
    File{"CA022110.binp", "B0FD71B07A0DFAE8E834A08A2EF7BD064B44492A7B50854B2C9245AB48266C00", 180224, 118784},
    File{"default.xexp", "79C6958470749931040BA2B68AED268A64CDAEED03F788EEAFFDABD45BA7EBBB", 299008, 299008}};

constexpr std::array god_patches{
    File{"CA022100.binp", "09C8A87064A2096438418DF80725C089CCC09A610CDB4805ED6993B822CB604E"},
    File{"CA022110.binp", "0A722663AFB4AC7F391A6A81DD1D4708D52B0C3749B614697C766A7084DCB747"},
    File{"default.xexp", "BC61B8AD072E8F62A56C6D32712EEE5B44F34B72AA4225C3C6569DD7F25DB374"}};

constexpr std::string_view disc_tu_hash =
    "0A893048C761BA6CF8ED14637F326726BC616B5183CA1EE52DE02FC12B46B215";
constexpr uintmax_t disc_tu_size = 598016;

void require(bool ok, std::string_view message) {
  if (!ok) {
    throw std::runtime_error(std::string(message));
  }
}

bool hash_equals(std::string_view actual, std::string_view expected) {
  if (actual.size() != expected.size()) {
    return false;
  }
  for (size_t i = 0; i < actual.size(); ++i) {
    const auto a = static_cast<unsigned char>(actual[i]);
    const auto b = static_cast<unsigned char>(expected[i]);
    if (std::toupper(a) != std::toupper(b)) {
      return false;
    }
  }
  return true;
}

bool matches(const fs::path& directory, std::span<const File> files) {
  for (const auto& file : files) {
    const auto path = directory / file.name;
    if (!fs::is_regular_file(path) ||
        !hash_equals(rex::crypto::sha256_file(path), file.hash)) {
      return false;
    }
  }
  return true;
}

enum class Dump { kMissing, kDisc, kGod };

Dump dump_kind(const fs::path& directory) {
  if (matches(directory, disc_base)) {
    return Dump::kDisc;
  }
  if (matches(directory, god_base)) {
    return Dump::kGod;
  }
  return Dump::kMissing;
}

uint64_t process_id() {
#ifdef _WIN32
  return GetCurrentProcessId();
#else
  return static_cast<uint64_t>(getpid());
#endif
}

fs::path staging_directory(const fs::path& game_data_path) {
  auto parent = game_data_path.parent_path();
  if (parent.empty()) {
    parent = ".";
  }
  auto name = game_data_path.filename().string();
  if (name.empty()) {
    name = "game_data";
  }
  for (unsigned attempt = 0; attempt < kMaxStagingAttempts; ++attempt) {
    const auto path = parent / (name + ".extracting-" +
                                std::to_string(process_id()) + "-" +
                                std::to_string(attempt));
    if (fs::create_directory(path)) {
      return path;
    }
  }
  throw std::runtime_error("Could not create an extraction staging directory.");
}

void status(std::string_view message) {
#ifdef _WIN32
  std::string line = "SDOJ setup: ";
  line.append(message);
  line.push_back('\n');
  OutputDebugStringA(line.c_str());
#else
  std::fprintf(stderr, "SDOJ setup: %.*s\n", static_cast<int>(message.size()), message.data());
#endif
}

void check_active(const std::atomic<bool>* active) {
  if (active != nullptr && !active->load()) {
    throw std::runtime_error("Setup was canceled.");
  }
}

void report_progress(const ProgressCallback& progress, std::string_view message,
                     float fraction = -1.0f) {
  if (progress) {
    progress(Progress{std::string(message), fraction});
  }
}

void report_phase(const ProgressCallback& progress, std::string_view message) {
  status(message);
  report_progress(progress, message);
}

void report_extractor_record(const ProgressCallback& progress,
                             std::string_view record) {
  while (!record.empty() && (record.front() == ' ' || record.front() == '\t')) {
    record.remove_prefix(1);
  }
  while (!record.empty() && (record.back() == ' ' || record.back() == '\t')) {
    record.remove_suffix(1);
  }
  if (record.empty()) {
    return;
  }

  float fraction = -1.0f;
  bool per_file = false;
  const size_t bracket = record.rfind('[');
  if (bracket != std::string_view::npos && bracket > 0 && record.back() == ']') {
    const auto percent = record.substr(bracket + 1, record.size() - bracket - 2);
    if (percent.size() >= 2 && percent.back() == '%') {
      const auto digits = percent.substr(0, percent.size() - 1);
      unsigned value = 0;
      const auto result =
          std::from_chars(digits.data(), digits.data() + digits.size(), value);
      if (result.ec == std::errc{} &&
          result.ptr == digits.data() + digits.size() && value <= 100) {
        record = record.substr(0, bracket);
        while (!record.empty() && (record.back() == ' ' || record.back() == '\t')) {
          record.remove_suffix(1);
        }
        fraction = static_cast<float>(value) / 100.0f;
        per_file = true;
      }
    }
  }

  std::string message(record);
  if (per_file) {
    message += " (per-file)";
  }
  report_progress(progress, message, fraction);
}

class ExtractorOutput {
 public:
  explicit ExtractorOutput(const ProgressCallback& progress)
      : progress_(progress) {
    record_.reserve(kMaxExtractorRecordSize);
  }

  void consume(const char* bytes, size_t size) {
    for (size_t i = 0; i < size; ++i) {
      const char c = bytes[i];
      if (c == '\r' || c == '\n') {
        finish_record();
      } else if (!truncated_) {
        if (record_.size() < kMaxExtractorRecordSize) {
          record_.push_back(c);
        } else {
          truncated_ = true;
        }
      }
    }
  }

  void finish() { finish_record(); }

 private:
  void finish_record() {
    if (!truncated_) {
      report_extractor_record(progress_, record_);
    }
    record_.clear();
    truncated_ = false;
  }

  const ProgressCallback& progress_;
  std::string record_;
  bool truncated_ = false;
};

#ifdef _WIN32
class ScopedHandle {
 public:
  explicit ScopedHandle(HANDLE handle = INVALID_HANDLE_VALUE) : handle_(handle) {}
  ~ScopedHandle() {
    if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
      CloseHandle(handle_);
    }
  }
  ScopedHandle(const ScopedHandle&) = delete;
  ScopedHandle& operator=(const ScopedHandle&) = delete;
  HANDLE get() const { return handle_; }
  void close() {
    if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
      CloseHandle(handle_);
      handle_ = INVALID_HANDLE_VALUE;
    }
  }

 private:
  HANDLE handle_;
};

std::wstring quote(std::wstring_view argument) {
  std::wstring result = L"\"";
  size_t slashes = 0;
  for (wchar_t c : argument) {
    if (c == L'\\') {
      ++slashes;
      continue;
    }
    result.append(slashes * (c == L'\"' ? 2 : 1), L'\\');
    if (c == L'\"') {
      result += L'\\';
    }
    result += c;
    slashes = 0;
  }
  result.append(slashes * 2, L'\\');
  return result + L'\"';
}

int run_extractor(const fs::path& executable, const fs::path& root,
                  const fs::path& destination, const fs::path& iso,
                  const ProgressCallback& progress,
                  const std::atomic<bool>* active) {
  check_active(active);
  ExtractorOutput records(progress);
  std::array<char, kExtractorOutputBufferSize> buffer{};
  SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
  HANDLE raw_read = INVALID_HANDLE_VALUE;
  HANDLE raw_write = INVALID_HANDLE_VALUE;
  require(CreatePipe(&raw_read, &raw_write, &security, 0),
          "Could not capture extract-xiso output.");
  ScopedHandle output_read(raw_read);
  ScopedHandle output_write(raw_write);
  require(SetHandleInformation(output_read.get(), HANDLE_FLAG_INHERIT, 0),
          "Could not prepare extract-xiso output capture.");
  ScopedHandle input(CreateFileW(L"NUL", GENERIC_READ,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
  require(input.get() != INVALID_HANDLE_VALUE,
          "Could not prepare extract-xiso input.");
  std::wstring command = quote(executable.wstring()) + L" -x -s -d " +
                         quote(destination.wstring()) + L" " + quote(iso.wstring());
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = input.get();
  startup.hStdOutput = output_write.get();
  startup.hStdError = output_write.get();
  PROCESS_INFORMATION process{};
  require(CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
                         CREATE_NO_WINDOW, nullptr, root.c_str(), &startup, &process),
          "Could not start extract-xiso.");
  CloseHandle(process.hThread);
  ScopedHandle process_handle(process.hProcess);
  output_write.close();

  bool process_exited = false;
  bool output_closed = false;
  DWORD code = 1;
  try {
    while (!process_exited || !output_closed) {
      check_active(active);
      if (!process_exited) {
        const DWORD waited = WaitForSingleObject(process_handle.get(), 0);
        if (waited == WAIT_OBJECT_0) {
          require(GetExitCodeProcess(process_handle.get(), &code),
                  "Could not wait for extract-xiso.");
          process_exited = true;
        } else {
          require(waited == WAIT_TIMEOUT, "Could not wait for extract-xiso.");
        }
      }

      if (output_closed) {
        if (!process_exited) {
          const DWORD waited = WaitForSingleObject(
              process_handle.get(), static_cast<DWORD>(kExtractorPollIntervalMs));
          if (waited == WAIT_OBJECT_0) {
            require(GetExitCodeProcess(process_handle.get(), &code),
                    "Could not wait for extract-xiso.");
            process_exited = true;
          } else {
            require(waited == WAIT_TIMEOUT, "Could not wait for extract-xiso.");
          }
        }
        continue;
      }

      DWORD available = 0;
      if (!PeekNamedPipe(output_read.get(), nullptr, 0, nullptr, &available, nullptr)) {
        if (GetLastError() == ERROR_BROKEN_PIPE) {
          output_closed = true;
          records.finish();
          continue;
        }
        throw std::runtime_error("Could not read extract-xiso output.");
      }
      if (available > 0) {
        DWORD read = 0;
        const DWORD request = std::min<DWORD>(available, static_cast<DWORD>(buffer.size()));
        if (!ReadFile(output_read.get(), buffer.data(), request, &read, nullptr)) {
          if (GetLastError() == ERROR_BROKEN_PIPE) {
            output_closed = true;
            records.finish();
            continue;
          }
          throw std::runtime_error("Could not read extract-xiso output.");
        }
        records.consume(buffer.data(), read);
      } else if (!process_exited) {
        const DWORD waited = WaitForSingleObject(
            process_handle.get(), static_cast<DWORD>(kExtractorPollIntervalMs));
        if (waited == WAIT_OBJECT_0) {
          require(GetExitCodeProcess(process_handle.get(), &code),
                  "Could not wait for extract-xiso.");
          process_exited = true;
        } else {
          require(waited == WAIT_TIMEOUT, "Could not wait for extract-xiso.");
        }
      } else {
        Sleep(kExtractorPollIntervalMs);
      }
    }
  } catch (...) {
    if (!process_exited) {
      TerminateProcess(process_handle.get(), 1);
      WaitForSingleObject(process_handle.get(), INFINITE);
    }
    throw;
  }
  return static_cast<int>(code);
}

void replace_file(const fs::path& from, const fs::path& to) {
  require(MoveFileExW(from.c_str(), to.c_str(),
                      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH),
          "Could not install a TU1 patch file.");
}
#else
class ScopedFd {
 public:
  explicit ScopedFd(int fd = -1) : fd_(fd) {}
  ~ScopedFd() {
    if (fd_ >= 0) {
      ::close(fd_);
    }
  }
  ScopedFd(const ScopedFd&) = delete;
  ScopedFd& operator=(const ScopedFd&) = delete;
  int get() const { return fd_; }
  void close() {
    if (fd_ >= 0) {
      ::close(fd_);
      fd_ = -1;
    }
  }

 private:
  int fd_;
};

void terminate_and_reap(pid_t child) noexcept {
  kill(child, SIGTERM);
  for (unsigned attempt = 0; attempt < kCancelGracePollCount; ++attempt) {
    int child_status = 0;
    const pid_t result = waitpid(child, &child_status, WNOHANG);
    if (result == child || (result < 0 && errno == ECHILD)) {
      return;
    }
    if (result < 0 && errno != EINTR) {
      break;
    }
    std::this_thread::sleep_for(
        std::chrono::milliseconds(kExtractorPollIntervalMs));
  }
  kill(child, SIGKILL);
  int child_status = 0;
  while (waitpid(child, &child_status, 0) < 0 && errno == EINTR) {
  }
}

int run_extractor(const fs::path& executable, const fs::path& root,
                  const fs::path& destination, const fs::path& iso,
                  const ProgressCallback& progress,
                  const std::atomic<bool>* active) {
  check_active(active);
  fs::permissions(executable,
                  fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                  fs::perm_options::add);
  ExtractorOutput records(progress);
  std::array<char, kExtractorOutputBufferSize> buffer{};

  int descriptors[2]{};
  require(pipe(descriptors) == 0, "Could not capture extract-xiso output.");
  ScopedFd output_read(descriptors[0]);
  ScopedFd output_write(descriptors[1]);
  const int flags = fcntl(output_read.get(), F_GETFL, 0);
  require(flags >= 0 && fcntl(output_read.get(), F_SETFL, flags | O_NONBLOCK) == 0,
          "Could not prepare extract-xiso output capture.");

  const std::string executable_string = executable.string();
  const std::string destination_string = destination.string();
  const std::string iso_string = iso.string();
  const pid_t child = fork();
  require(child >= 0, "Could not start extract-xiso.");
  if (child == 0) {
    output_read.close();
    if (dup2(output_write.get(), STDOUT_FILENO) < 0 ||
        dup2(output_write.get(), STDERR_FILENO) < 0) {
      _exit(127);
    }
    output_write.close();
    if (chdir(root.c_str()) != 0) {
      _exit(127);
    }
    execl(executable_string.c_str(), executable_string.c_str(), "-x", "-s", "-d",
          destination_string.c_str(), iso_string.c_str(), static_cast<char*>(nullptr));
    _exit(127);
  }
  output_write.close();

  int child_status = 0;
  bool process_exited = false;
  bool output_closed = false;
  try {
    while (!process_exited || !output_closed) {
      check_active(active);
      if (!process_exited) {
        const pid_t waited = waitpid(child, &child_status, WNOHANG);
        if (waited == child) {
          process_exited = true;
        } else if (waited < 0 && errno != EINTR) {
          throw std::runtime_error("Could not wait for extract-xiso.");
        }
      }

      if (output_closed) {
        if (!process_exited) {
          std::this_thread::sleep_for(
              std::chrono::milliseconds(kExtractorPollIntervalMs));
        }
        continue;
      }

      pollfd descriptor{output_read.get(), static_cast<short>(POLLIN | POLLHUP), 0};
      const int ready = poll(&descriptor, 1, static_cast<int>(kExtractorPollIntervalMs));
      if (ready < 0) {
        if (errno == EINTR) {
          continue;
        }
        throw std::runtime_error("Could not read extract-xiso output.");
      }
      if (ready == 0) {
        continue;
      }
      if ((descriptor.revents & (POLLERR | POLLNVAL)) != 0) {
        throw std::runtime_error("Could not read extract-xiso output.");
      }
      if ((descriptor.revents & (POLLIN | POLLHUP)) != 0) {
        while (true) {
          check_active(active);
          const ssize_t bytes = read(output_read.get(), buffer.data(), buffer.size());
          if (bytes > 0) {
            records.consume(buffer.data(), static_cast<size_t>(bytes));
            continue;
          }
          if (bytes == 0) {
            output_closed = true;
            records.finish();
            break;
          }
          if (errno == EINTR) {
            continue;
          }
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
          }
          throw std::runtime_error("Could not read extract-xiso output.");
        }
      }
    }
  } catch (...) {
    if (!process_exited) {
      terminate_and_reap(child);
    }
    throw;
  }
  if (WIFEXITED(child_status)) {
    return WEXITSTATUS(child_status);
  }
  if (WIFSIGNALED(child_status)) {
    return 128 + WTERMSIG(child_status);
  }
  return 1;
}

void replace_file(const fs::path& from, const fs::path& to) {
  std::error_code error;
  fs::rename(from, to, error);
  require(!error, "Could not install a TU1 patch file.");
}
#endif

fs::path extractor_path(const fs::path& release_root) {
#ifdef _WIN32
  return release_root / "tools/extract-xiso/extract-xiso.exe";
#else
  return release_root / "tools/extract-xiso/extract-xiso";
#endif
}

void ensure_parent_directory(const fs::path& path) {
  const auto parent = path.parent_path();
  if (!parent.empty()) {
    fs::create_directories(parent);
  }
}

void prepare_impl(const fs::path& release_root, const fs::path& game_data_path,
                  const ProgressCallback& progress,
                  const std::atomic<bool>* active) {
  check_active(active);
  require(!release_root.empty(), "Release folder was not provided.");
  require(!game_data_path.empty(), "Game data folder was not provided.");

  report_phase(progress, "Verifying game data...");
  auto kind = dump_kind(game_data_path);

  if (kind == Dump::kMissing) {
    require(!fs::exists(game_data_path) ||
                (fs::is_directory(game_data_path) && fs::is_empty(game_data_path)),
            "game_data is incomplete or from an unsupported dump. Existing files were left untouched.");

    std::vector<fs::path> isos;
    for (const auto& entry : fs::directory_iterator(release_root)) {
      auto extension = entry.path().extension().string();
      std::transform(extension.begin(), extension.end(), extension.begin(),
                     [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      if (entry.is_regular_file() && extension == ".iso" && entry.file_size() > 0) {
        isos.push_back(entry.path());
      }
    }
    require(isos.size() == 1,
            "Put exactly one supported SDOJ ISO next to the game executable.");

    const auto extractor = extractor_path(release_root);
    require(fs::is_regular_file(extractor),
            "extract-xiso is missing. Re-extract the release and try again.");

    check_active(active);
    ensure_parent_directory(game_data_path);
    check_active(active);
    const auto staging = staging_directory(game_data_path);
    report_phase(progress, "Extracting ISO...");
    require(run_extractor(extractor, release_root, staging, isos.front(), progress, active) == 0,
            "ISO extraction failed. The temporary extraction was retained for diagnosis.");

    check_active(active);
    report_phase(progress, "Verifying extracted files...");
    kind = dump_kind(staging);
    require(kind != Dump::kMissing,
            "That ISO does not match a supported SDOJ dump. Extracted files were retained.");

    check_active(active);
    if (fs::exists(game_data_path)) {
      require(fs::is_empty(game_data_path) && fs::remove(game_data_path),
              "Could not replace the empty game_data directory.");
    }
    fs::rename(staging, game_data_path);
  }

  const auto patches = kind == Dump::kDisc
                           ? std::span<const File>(disc_patches)
                           : std::span<const File>(god_patches);
  report_phase(progress, "Verifying title update...");
  check_active(active);

  if (!matches(game_data_path, patches)) {
    std::vector<std::vector<char>> disc_payloads;
    fs::path title_update;

    if (kind == Dump::kGod) {
      require(matches(release_root, god_patches),
              "Put verified GoD TU1 CA022100.binp, CA022110.binp and default.xexp next to the game executable.");
    } else {
      for (const auto& entry : fs::directory_iterator(release_root)) {
        if (entry.is_regular_file() && entry.file_size() == disc_tu_size &&
            hash_equals(rex::crypto::sha256_file(entry.path()), disc_tu_hash)) {
          title_update = entry.path();
          break;
        }
      }
      require(!title_update.empty(),
              "Put the supported disc TU1 (TU_11LK1V7...) file next to the game executable.");

      std::ifstream input(title_update, std::ios::binary);
      require(input.is_open(), "Could not read the TU1 file.");
      for (const auto& file : patches) {
        std::vector<char> payload(file.length);
        input.clear();
        input.seekg(static_cast<std::streamoff>(file.offset));
        input.read(payload.data(), static_cast<std::streamsize>(payload.size()));
        require(static_cast<size_t>(input.gcount()) == payload.size(),
                "Could not read a TU1 payload.");
        require(hash_equals(rex::crypto::sha256(std::string_view(payload.data(), payload.size())),
                            file.hash),
                "TU1 payload verification failed.");
        disc_payloads.push_back(std::move(payload));
      }
    }

    report_phase(progress, "Installing title update...");
    check_active(active);
    ensure_parent_directory(game_data_path);
    check_active(active);
    const auto staging = staging_directory(game_data_path);
    if (kind == Dump::kGod) {
      for (const auto& file : patches) {
        check_active(active);
        fs::copy_file(release_root / file.name, staging / file.name);
      }
    } else {
      for (size_t i = 0; i < patches.size(); ++i) {
        check_active(active);
        std::ofstream output(staging / patches[i].name, std::ios::binary);
        output.write(disc_payloads[i].data(),
                     static_cast<std::streamsize>(disc_payloads[i].size()));
        output.close();
        require(!output.fail(), "Could not write a TU1 patch file.");
      }
    }

    check_active(active);
    require(matches(staging, patches), "Staged TU1 patches failed verification.");
    check_active(active);
    for (const auto& file : patches) {
      replace_file(staging / file.name, game_data_path / file.name);
    }
    require(fs::is_empty(staging) && fs::remove(staging),
            "Could not remove the empty TU1 staging directory.");
    check_active(active);
    require(matches(game_data_path, patches), "Installed TU1 patches failed verification.");
  }
  check_active(active);
  report_phase(progress, "Setup complete.");
}

}

bool IsPrepared(const std::filesystem::path& game_data_path) noexcept {
  try {
    const auto kind = dump_kind(game_data_path);
    if (kind == Dump::kDisc) {
      return matches(game_data_path, disc_patches);
    }
    if (kind == Dump::kGod) {
      return matches(game_data_path, god_patches);
    }
  } catch (...) {
  }
  return false;
}

bool Prepare(const std::filesystem::path& release_root,
             const std::filesystem::path& game_data_path,
             std::string& error,
             const ProgressCallback& progress,
             const std::atomic<bool>* active) noexcept {
  try {
    prepare_impl(release_root, game_data_path, progress, active);
    return true;
  } catch (const std::exception& exception) {
    error = exception.what();
  } catch (...) {
    error = "Unknown setup failure.";
  }
  return false;
}

void ReportError(const std::string& error) {
#ifdef _WIN32
  const std::string message = "The game could not start because setup failed:\n\n" + error;
  OutputDebugStringA(message.c_str());
  std::fprintf(stderr, "SDOJ setup failed: %s\n", error.c_str());
  MessageBoxA(nullptr, message.c_str(), "SDOJ setup failed",
              MB_OK | MB_ICONERROR | MB_SETFOREGROUND | MB_TASKMODAL);
#else
  std::fprintf(stderr, "SDOJ setup failed: %s\n", error.c_str());
#endif
}

namespace {

struct SetupState {
  std::atomic<bool> active{true};
  std::atomic<bool> result_delivered{false};
  rex::ui::WindowedAppContext* context = nullptr;
  std::mutex progress_mutex;
  Progress progress{"Verifying game data...", -1.0f};
  std::unique_ptr<popups::Progress> dialog;
  std::function<void()> success;
  std::function<void(const std::string&)> failure;
};

void CompleteOnUI(const std::shared_ptr<SetupState>& state, bool prepared,
                  std::string error) {
  if (!state || !state->active.load(std::memory_order_acquire) ||
      state->result_delivered.exchange(true, std::memory_order_acq_rel)) {
    return;
  }

  state->dialog.reset();
  if (prepared) {
    auto success = std::move(state->success);
    state->success = nullptr;
    if (success) {
      try {
        success();
      } catch (...) {
      }
    }
  } else if (state->failure) {
    auto failure = std::move(state->failure);
    state->failure = nullptr;
    try {
      failure(error);
    } catch (...) {
    }
  } else {
    ReportError(error);
    if (state->context) {
      state->context->QuitFromUIThread();
    }
  }
}

void QueueResult(const std::shared_ptr<SetupState>& state, bool prepared,
                 std::string error) {
  if (!state || !state->active.load(std::memory_order_acquire) || !state->context) {
    return;
  }
  const std::weak_ptr<SetupState> weak_state = state;
  try {
    state->context->CallInUIThreadDeferred(
        [weak_state, prepared, error = std::move(error)]() mutable {
          const auto current = weak_state.lock();
          if (!current || !current->active.load(std::memory_order_acquire)) {
            return;
          }
          CompleteOnUI(current, prepared, std::move(error));
        });
  } catch (...) {
  }
}

}

struct Setup::Data {
  std::shared_ptr<SetupState> state;
  std::thread worker;
};

Setup::Setup() : data_(std::make_unique<Data>()) {}

Setup::~Setup() { Stop(); }

void Setup::Start(rex::ui::WindowedAppContext& context,
                  rex::ui::ImGuiDrawer* drawer,
                  const std::filesystem::path& release_root,
                  const std::filesystem::path& game_data_path,
                  std::function<void()> success,
                  std::function<void(const std::string&)> failure) {
  if (data_->state || data_->worker.joinable()) {
    return;
  }

  std::shared_ptr<SetupState> state;
  try {
    state = std::make_shared<SetupState>();
  } catch (...) {
    constexpr char kStartError[] = "Could not start game data setup.";
    if (failure) {
      failure(kStartError);
    } else {
      ReportError(kStartError);
      context.QuitFromUIThread();
    }
    return;
  }
  state->context = &context;
  state->success = std::move(success);
  state->failure = std::move(failure);
  data_->state = state;

  if (drawer) {
    try {
      const std::weak_ptr<SetupState> weak_state = state;
      state->dialog = std::make_unique<popups::Progress>(drawer, [weak_state] {
        Progress progress;
        if (const auto current = weak_state.lock()) {
          std::lock_guard lock(current->progress_mutex);
          progress = current->progress;
        }
        return progress;
      });
    } catch (...) {
    }
  }

  try {
    data_->worker = std::thread(
        [state, release_root, game_data_path] {
          std::string error;
          bool prepared = false;
          try {
            const std::weak_ptr<SetupState> weak_state = state;
            prepared = Prepare(
                release_root, game_data_path, error,
                [weak_state](const Progress& progress) {
                  const auto current = weak_state.lock();
                  if (!current ||
                      !current->active.load(std::memory_order_acquire)) {
                    return;
                  }
                  std::lock_guard lock(current->progress_mutex);
                  current->progress = progress;
                },
                &state->active);
          } catch (...) {
            error = "Unknown setup failure.";
          }
          QueueResult(state, prepared, std::move(error));
        });
  } catch (...) {
    QueueResult(state, false, "Could not start game data setup.");
  }
}

void Setup::Stop() {
  const auto state = data_->state;
  if (state) {
    state->active.store(false, std::memory_order_release);
  }
  if (data_->worker.joinable()) {
    data_->worker.join();
  }
  if (state) {
    state->dialog.reset();
    state->context = nullptr;
  }
  data_->state.reset();
}

}
