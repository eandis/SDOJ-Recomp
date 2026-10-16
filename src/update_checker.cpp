#include "update_checker.h"
#include "popups.h"

#include <rex/cvar.h>
#include <rex/ui/imgui_drawer.h>
#include <rex/ui/windowed_app_context.h>

#include <nlohmann/json.hpp>
#include <toml++/toml.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>
#elif defined(__linux__)
#include <curl/curl.h>
#include <gio/gio.h>
#endif

REXCVAR_DECLARE(bool, check_for_updates);

#ifndef SDOJ_RELEASE_VERSION
#define SDOJ_RELEASE_VERSION ""
#endif

namespace update_checker {
namespace {

constexpr std::chrono::milliseconds kRequestTimeout{5000};
constexpr std::chrono::milliseconds kConnectTimeout{2000};

#ifdef _WIN32
constexpr wchar_t kApiHostWide[] = L"api.github.com";
constexpr wchar_t kApiPathWide[] = L"/repos/eandis/SDOJ-Recomp/releases/latest";
#endif

constexpr std::size_t kMaximumBodyBytes = 128 * 1024;
constexpr std::int64_t kCheckIntervalSeconds = 24 * 60 * 60;
constexpr std::string_view kReleasePagePrefix =
    "https://github.com/eandis/SDOJ-Recomp/releases/";

struct VersionValue {
  std::array<std::uint32_t, 4> parts{};
  std::size_t count = 0;
};

struct ReleaseRecord {
  std::string tag_name;
  std::string name;
  std::string html_url;
  bool draft = false;
  bool prerelease = false;
  std::string changelog;
};

struct LatestRelease {
  std::string version;
  std::string url;
  std::string changelog;
};

std::optional<VersionValue> ParseVersion(std::string_view text) {
  if (text.size() > 1 && text.front() == 'v') {
    text.remove_prefix(1);
  }
  if (text.empty()) {
    return std::nullopt;
  }

  VersionValue result;
  std::size_t start = 0;
  while (start < text.size()) {
    if (result.count == result.parts.size()) {
      return std::nullopt;
    }
    const std::size_t end = text.find('.', start);
    const std::size_t part_end = end == std::string_view::npos ? text.size() : end;
    if (part_end == start) {
      return std::nullopt;
    }
    const std::string_view part = text.substr(start, part_end - start);
    if (!std::all_of(part.begin(), part.end(), [](char value) {
          return value >= '0' && value <= '9';
        })) {
      return std::nullopt;
    }
    std::uint32_t number = 0;
    const auto [last, error] = std::from_chars(part.data(), part.data() + part.size(), number);
    if (error != std::errc() || last != part.data() + part.size()) {
      return std::nullopt;
    }
    result.parts[result.count++] = number;
    if (end == std::string_view::npos) {
      start = text.size();
    } else {
      start = end + 1;
      if (start == text.size()) {
        return std::nullopt;
      }
    }
  }
  if (result.count < 2) {
    return std::nullopt;
  }
  return result;
}

int CompareVersions(const VersionValue& left, const VersionValue& right) {
  for (std::size_t i = 0; i < std::max(left.count, right.count); ++i) {
    const std::uint32_t left_part = i < left.count ? left.parts[i] : 0;
    const std::uint32_t right_part = i < right.count ? right.parts[i] : 0;
    if (left_part < right_part) return -1;
    if (left_part > right_part) return 1;
  }
  return 0;
}

bool IsTrustedReleaseUrl(std::string_view url) {
  if (!url.starts_with(kReleasePagePrefix) || url.size() == kReleasePagePrefix.size()) {
    return false;
  }
  return std::none_of(url.begin(), url.end(), [](unsigned char value) {
    return value < 0x20 || value == 0x7f;
  });
}

bool BodyWithinLimit(std::size_t current_size, std::size_t incoming_size) {
  return current_size <= kMaximumBodyBytes &&
         incoming_size <= kMaximumBodyBytes - current_size;
}

std::optional<LatestRelease> SelectLatestRelease(const ReleaseRecord& record) {
  if (record.draft || record.prerelease || !IsTrustedReleaseUrl(record.html_url)) {
    return std::nullopt;
  }
  const auto tag_version = ParseVersion(record.tag_name);
  if (tag_version) {
    return LatestRelease{record.tag_name, record.html_url, record.changelog};
  }
  std::string_view name = record.name;
  const auto is_space = [](unsigned char value) {
    return value == ' ' || value == '\t' || value == '\n' || value == '\r' ||
           value == '\f' || value == '\v';
  };
  while (!name.empty() && is_space(static_cast<unsigned char>(name.front()))) {
    name.remove_prefix(1);
  }
  while (!name.empty() && is_space(static_cast<unsigned char>(name.back()))) {
    name.remove_suffix(1);
  }
  if (!ParseVersion(name)) {
    return std::nullopt;
  }
  return LatestRelease{std::string(name), record.html_url, record.changelog};
}

std::optional<LatestRelease> ParseReleaseJson(std::string_view body) {
  if (!BodyWithinLimit(0, body.size())) {
    return std::nullopt;
  }
  const auto document = nlohmann::json::parse(body.begin(), body.end(), nullptr, false);
  if (document.is_discarded() || !document.is_object()) {
    return std::nullopt;
  }
  const auto tag = document.find("tag_name");
  const auto name = document.find("name");
  const auto url = document.find("html_url");
  const auto draft = document.find("draft");
  const auto prerelease = document.find("prerelease");
  if (tag == document.end() || !tag->is_string() || url == document.end() || !url->is_string() ||
      draft == document.end() || !draft->is_boolean() || prerelease == document.end() ||
      !prerelease->is_boolean()) {
    return std::nullopt;
  }
  std::string release_name;
  if (name != document.end() && name->is_string()) {
    release_name = name->get<std::string>();
  }
  std::string changelog;
  const auto body_field = document.find("body");
  if (body_field != document.end() && body_field->is_string()) {
    auto parsed_changelog = body_field->get<std::string>();
    if (BodyWithinLimit(0, parsed_changelog.size())) {
      changelog = std::move(parsed_changelog);
    }
  }
  return SelectLatestRelease({tag->get<std::string>(), std::move(release_name),
                              url->get<std::string>(), draft->get<bool>(),
                              prerelease->get<bool>(), std::move(changelog)});
}

bool ShouldCheckNow(bool enabled, std::string_view local_version, std::int64_t now,
                    const std::optional<std::int64_t>& last_success) {
  if (!enabled || !ParseVersion(local_version)) {
    return false;
  }
  if (!last_success || *last_success <= 0 || now < *last_success) {
    return true;
  }
  return now >= kCheckIntervalSeconds && *last_success <= now - kCheckIntervalSeconds;
}

bool ShouldPrompt(std::string_view local, std::string_view latest,
                  std::string_view skipped) {
  const auto local_version = ParseVersion(local);
  const auto latest_version = ParseVersion(latest);
  if (!local_version || !latest_version ||
      CompareVersions(*latest_version, *local_version) <= 0) {
    return false;
  }
  const auto skipped_version = ParseVersion(skipped);
  return !skipped_version || CompareVersions(*latest_version, *skipped_version) != 0;
}

struct CacheData {
  std::optional<std::int64_t> last_success;
  std::string latest_version;
  std::string latest_url;
  std::string skipped_version;
  std::string latest_changelog;
};

std::filesystem::path CachePath(const std::filesystem::path& user_data_path) {
  return user_data_path / "update_check.toml";
}

CacheData ReadCacheFile(const std::filesystem::path& path) {
  CacheData cache;
  try {
    if (!std::filesystem::exists(path)) {
      return cache;
    }
    const auto table = toml::parse_file(path.string());
    cache.last_success = table["last_success_unix"].value<std::int64_t>();
    if (const auto value = table["latest_version"].value<std::string>()) {
      cache.latest_version = *value;
    }
    if (const auto value = table["latest_url"].value<std::string>()) {
      cache.latest_url = *value;
    }
    if (const auto value = table["skipped_version"].value<std::string>()) {
      cache.skipped_version = *value;
    }
    if (const auto value = table["latest_changelog"].value<std::string>();
        value && BodyWithinLimit(0, value->size())) {
      cache.latest_changelog = *value;
    }
  } catch (...) {
  }
  return cache;
}

void WriteCacheFile(const std::filesystem::path& path, const CacheData& cache) {
  try {
    std::error_code error;
    const auto parent_path = path.parent_path();
    if (!parent_path.empty()) {
      std::filesystem::create_directories(parent_path, error);
    }
    if (error) {
      return;
    }

    toml::table table;
    if (cache.last_success) {
      table.insert("last_success_unix", *cache.last_success);
    }
    if (!cache.latest_version.empty()) {
      table.insert("latest_version", cache.latest_version);
    }
    if (!cache.latest_url.empty()) {
      table.insert("latest_url", cache.latest_url);
    }
    if (!cache.skipped_version.empty()) {
      table.insert("skipped_version", cache.skipped_version);
    }
    if (!cache.latest_changelog.empty() &&
        BodyWithinLimit(0, cache.latest_changelog.size())) {
      table.insert("latest_changelog", cache.latest_changelog);
    }

    auto temporary_path = path;
    temporary_path += ".tmp";
    {
      std::ofstream file(temporary_path, std::ios::binary | std::ios::trunc);
      if (!file) {
        return;
      }
      file << table;
      file.flush();
      if (!file) {
        file.close();
        std::filesystem::remove(temporary_path, error);
        return;
      }
    }

#ifdef _WIN32
    if (!MoveFileExW(temporary_path.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
      DeleteFileW(temporary_path.c_str());
    }
#else
    std::filesystem::rename(temporary_path, path, error);
    if (error) {
      std::filesystem::remove(temporary_path, error);
    }
#endif
  } catch (...) {
  }
}

struct UpdateState;

struct UpdateState {
  std::atomic<bool> active{true};
  rex::ui::WindowedAppContext* context = nullptr;
  rex::ui::ImGuiDrawer* drawer = nullptr;
  std::filesystem::path cache_path;
  std::string local_version;
  std::unique_ptr<popups::Update> dialog;
  bool before_setup = false;
  std::function<void()> completion;
  std::unique_ptr<popups::Message> notice;
  bool busy = false;
};

void CompleteFirstSetupOnUI(const std::shared_ptr<UpdateState>& state) {
  if (!state || !state->active.load(std::memory_order_acquire) || !state->before_setup) {
    return;
  }
  auto completion = std::move(state->completion);
  state->completion = nullptr;
  if (completion) {
    try {
      completion();
    } catch (...) {
    }
  }
}

bool QueueFirstSetupCompletion(const std::shared_ptr<UpdateState>& state) {
  if (!state || !state->before_setup ||
      !state->active.load(std::memory_order_acquire) || !state->context) {
    return false;
  }
  const std::weak_ptr<UpdateState> weak_state = state;
  try {
    return state->context->CallInUIThreadDeferred([weak_state] {
      const auto current = weak_state.lock();
      if (!current || !current->active.load(std::memory_order_acquire)) {
        return;
      }
      CompleteFirstSetupOnUI(current);
    });
  } catch (...) {
    return false;
  }
}

void LaunchReleasePage(const std::string& url) {
  if (!IsTrustedReleaseUrl(url)) {
    return;
  }
#ifdef _WIN32
  const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, url.data(),
                                            static_cast<int>(url.size()), nullptr, 0);
  if (required <= 0) {
    return;
  }
  std::wstring wide_url(static_cast<std::size_t>(required), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, url.data(),
                          static_cast<int>(url.size()), wide_url.data(), required) <= 0) {
    return;
  }
  const auto result = reinterpret_cast<std::intptr_t>(
      ShellExecuteW(nullptr, L"open", wide_url.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
  (void)result;
#elif defined(__linux__)
  GError* error = nullptr;
  g_app_info_launch_default_for_uri(url.c_str(), nullptr, &error);
  if (error) {
    g_error_free(error);
  }
#endif
}

void HandleDialogAction(const std::weak_ptr<UpdateState>& weak_state,
                        const LatestRelease& release, popups::UpdateAction action) {
  const auto state = weak_state.lock();
  if (!state || !state->active.load(std::memory_order_acquire) || !state->context) {
    return;
  }
  try {
    state->context->CallInUIThreadDeferred([weak_state, release, action] {
      const auto current = weak_state.lock();
      if (!current || !current->active.load(std::memory_order_acquire)) {
        return;
      }
      try {
        if (action == popups::UpdateAction::Open) {
          LaunchReleasePage(release.url);
        } else if (action == popups::UpdateAction::Skip) {
          auto cache = ReadCacheFile(current->cache_path);
          cache.skipped_version = release.version;
          WriteCacheFile(current->cache_path, cache);
        }
      } catch (...) {
      }
      current->dialog.reset();
      CompleteFirstSetupOnUI(current);
    });
  } catch (...) {
  }
}

bool QueueUpdateDialog(const std::shared_ptr<UpdateState>& state,
                       const LatestRelease& release) {
  if (!state->active.load(std::memory_order_acquire) || !state->context) {
    return false;
  }
  const std::weak_ptr<UpdateState> weak_state = state;
  try {
    return state->context->CallInUIThreadDeferred([weak_state, release] {
      const auto current = weak_state.lock();
      if (!current || !current->active.load(std::memory_order_acquire)) {
        return;
      }
      bool dialog_shown = false;
      try {
        if (current->drawer && REXCVAR_GET(check_for_updates) &&
            ShouldPrompt(current->local_version, release.version, {}) && !current->dialog) {
          const std::weak_ptr<UpdateState> action_state = current;
          current->dialog = std::make_unique<popups::Update>(
              current->drawer, release.version, release.changelog,
              [action_state, release](popups::UpdateAction action) {
                HandleDialogAction(action_state, release, action);
              });
          dialog_shown = true;
        }
      } catch (...) {
      }
      if (!dialog_shown) {
        CompleteFirstSetupOnUI(current);
      }
    });
  } catch (...) {
    return false;
  }
}

}

struct Checker::Data {
  std::shared_ptr<UpdateState> state;
  std::thread worker;
  rex::ui::WindowedAppContext* context = nullptr;
  rex::ui::ImGuiDrawer* drawer = nullptr;
  std::filesystem::path cache_path;
  std::shared_ptr<UpdateState> manual;
  std::thread manual_worker;
};

namespace {

#ifdef _WIN32
constexpr int kWinHttpResolveTimeoutMs = 1000;
constexpr int kWinHttpConnectTimeoutMs = 1500;
constexpr int kWinHttpSendTimeoutMs = 1500;
constexpr int kWinHttpReceiveTimeoutMs = 1000;

struct WinHttpHandle {
  HINTERNET value = nullptr;
  WinHttpHandle() = default;
  explicit WinHttpHandle(HINTERNET handle) : value(handle) {}
  ~WinHttpHandle() {
    if (value) {
      WinHttpCloseHandle(value);
    }
  }
  WinHttpHandle(const WinHttpHandle&) = delete;
  WinHttpHandle& operator=(const WinHttpHandle&) = delete;
};

std::optional<std::string> FetchLatestBody(const std::shared_ptr<UpdateState>& state) {
  const auto deadline = std::chrono::steady_clock::now() + kRequestTimeout;
  WinHttpHandle session(WinHttpOpen(L"SDOJ-Recomp update checker",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
  if (!session.value) {
    return std::nullopt;
  }
  if (!WinHttpSetTimeouts(session.value, kWinHttpResolveTimeoutMs,
                          kWinHttpConnectTimeoutMs, kWinHttpSendTimeoutMs,
                          kWinHttpReceiveTimeoutMs)) {
    return std::nullopt;
  }
  WinHttpHandle connection(WinHttpConnect(session.value, kApiHostWide,
                                           INTERNET_DEFAULT_HTTPS_PORT, 0));
  if (!connection.value || !state->active.load(std::memory_order_acquire) ||
      std::chrono::steady_clock::now() >= deadline) {
    return std::nullopt;
  }
  WinHttpHandle request(WinHttpOpenRequest(
      connection.value, L"GET", kApiPathWide, nullptr, WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
  if (!request.value) {
    return std::nullopt;
  }
  DWORD disable_redirects = WINHTTP_DISABLE_REDIRECTS;
  if (!WinHttpSetOption(request.value, WINHTTP_OPTION_DISABLE_FEATURE,
                        &disable_redirects, sizeof(disable_redirects))) {
    return std::nullopt;
  }
  constexpr wchar_t headers[] =
      L"Accept: application/vnd.github+json\r\n"
      L"X-GitHub-Api-Version: 2022-11-28\r\n";
  if (!state->active.load(std::memory_order_acquire) ||
      !WinHttpSendRequest(request.value, headers, static_cast<DWORD>(-1),
                          WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
      !WinHttpReceiveResponse(request.value, nullptr)) {
    return std::nullopt;
  }
  if (std::chrono::steady_clock::now() >= deadline) {
    return std::nullopt;
  }

  DWORD status_code = 0;
  DWORD status_size = sizeof(status_code);
  if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                           WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size,
                           WINHTTP_NO_HEADER_INDEX) ||
      status_code != 200) {
    return std::nullopt;
  }

  std::string body;
  while (state->active.load(std::memory_order_acquire) &&
         std::chrono::steady_clock::now() < deadline) {
    DWORD available = 0;
    if (!WinHttpQueryDataAvailable(request.value, &available)) {
      return std::nullopt;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      return std::nullopt;
    }
    if (available == 0) {
      return body;
    }
    if (!BodyWithinLimit(body.size(), available)) {
      return std::nullopt;
    }
    std::array<char, 8192> buffer{};
    const DWORD to_read = std::min<DWORD>(available, static_cast<DWORD>(buffer.size()));
    DWORD bytes_read = 0;
    if (!WinHttpReadData(request.value, buffer.data(), to_read, &bytes_read)) {
      return std::nullopt;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      return std::nullopt;
    }
    if (bytes_read == 0 || !BodyWithinLimit(body.size(), bytes_read)) {
      return std::nullopt;
    }
    body.append(buffer.data(), bytes_read);
  }
  return std::nullopt;
}
#elif defined(__linux__)
struct CurlResponse {
  std::string body;
  bool too_large = false;
};

std::size_t ReceiveCurlBody(char* data, std::size_t size, std::size_t count,
                            void* user_data) {
  auto* response = static_cast<CurlResponse*>(user_data);
  if (size != 0 && count > std::numeric_limits<std::size_t>::max() / size) {
    response->too_large = true;
    return 0;
  }
  const std::size_t bytes = size * count;
  if (!BodyWithinLimit(response->body.size(), bytes)) {
    response->too_large = true;
    return 0;
  }
  response->body.append(data, bytes);
  return bytes;
}

int CheckCurlCancellation(void* user_data, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
  const auto* state = static_cast<const std::shared_ptr<UpdateState>*>(user_data);
  return (*state)->active.load(std::memory_order_acquire) ? 0 : 1;
}

std::optional<std::string> FetchLatestBody(const std::shared_ptr<UpdateState>& state) {
  CURL* curl = curl_easy_init();
  if (!curl) {
    return std::nullopt;
  }
  CurlResponse response;
  constexpr char endpoint[] =
      "https://api.github.com/repos/eandis/SDOJ-Recomp/releases/latest";
  curl_easy_setopt(curl, CURLOPT_URL, endpoint);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "SDOJ-Recomp update checker");
  curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Accept: application/vnd.github+json");
  headers = curl_slist_append(headers, "X-GitHub-Api-Version: 2022-11-28");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
#if LIBCURL_VERSION_NUM >= 0x075500
  curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
#else
  curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
#endif
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS,
                   static_cast<long>(kConnectTimeout.count()));
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(kRequestTimeout.count()));
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ReceiveCurlBody);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, CheckCurlCancellation);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &state);

  const CURLcode result = curl_easy_perform(curl);
  long status_code = 0;
  char* effective_url = nullptr;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
  curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
  const bool exact_endpoint = effective_url &&
      std::string_view(effective_url) == endpoint;
  curl_easy_cleanup(curl);
  curl_slist_free_all(headers);
  if (result != CURLE_OK || status_code != 200 || response.too_large || !exact_endpoint ||
      !state->active.load(std::memory_order_acquire)) {
    return std::nullopt;
  }
  return response.body;
}
#else
std::optional<std::string> FetchLatestBody(const std::shared_ptr<UpdateState>&) {
  return std::nullopt;
}
#endif

std::optional<LatestRelease> FetchRelease(const std::shared_ptr<UpdateState>& state,
                                          CacheData& cache) {
  const auto body = FetchLatestBody(state);
  if (!body || !state->active.load(std::memory_order_acquire)) {
    return std::nullopt;
  }
  auto release = ParseReleaseJson(*body);
  if (release) {
    cache.last_success = static_cast<std::int64_t>(std::time(nullptr));
    cache.latest_version = release->version;
    cache.latest_url = release->url;
    cache.latest_changelog = release->changelog;
    WriteCacheFile(state->cache_path, cache);
  }
  return release;
}

bool CheckForUpdate(const std::shared_ptr<UpdateState>& state, bool enabled) {
  if (!state->active.load(std::memory_order_acquire)) {
    return false;
  }
  CacheData cache = ReadCacheFile(state->cache_path);
  auto release = SelectLatestRelease(
      {cache.latest_version, cache.latest_version, cache.latest_url, false, false,
       cache.latest_changelog});

  const std::int64_t now = static_cast<std::int64_t>(std::time(nullptr));
  if (ShouldCheckNow(enabled, state->local_version, now, cache.last_success) &&
      state->active.load(std::memory_order_acquire)) {
    if (auto fetched_release = FetchRelease(state, cache)) {
      release = std::move(fetched_release);
    }
  }
  if (state->active.load(std::memory_order_acquire) && release &&
      ShouldPrompt(state->local_version, release->version, cache.skipped_version)) {
    return QueueUpdateDialog(state, *release);
  }
  return false;
}

void ShowNotice(const std::shared_ptr<UpdateState>& state, std::string title,
                std::string text) {
  const std::weak_ptr<UpdateState> weak_state = state;
  state->notice = std::make_unique<popups::Message>(
      state->drawer, std::move(title), std::move(text), "OK", [weak_state] {
        const auto current = weak_state.lock();
        if (!current || !current->active.load(std::memory_order_acquire) ||
            !current->context) {
          return;
        }
        current->context->CallInUIThreadDeferred([weak_state] {
          if (const auto latest = weak_state.lock()) {
            latest->notice.reset();
          }
        });
      });
}

void ShowManualResult(const std::shared_ptr<UpdateState>& state,
                      const std::optional<LatestRelease>& release) {
  state->busy = false;
  state->notice.reset();
  if (!state->drawer) {
    return;
  }
  if (!release) {
    ShowNotice(state, "Update check failed",
               "Could not get the latest release from GitHub. Check your internet "
               "connection and try again.");
    return;
  }
  if (!ShouldPrompt(state->local_version, release->version, {})) {
    ShowNotice(state, "No updates",
               "You have the latest version (" + state->local_version + ").");
    return;
  }
  const std::weak_ptr<UpdateState> action_state = state;
  state->dialog = std::make_unique<popups::Update>(
      state->drawer, release->version, release->changelog,
      [action_state, release = *release](popups::UpdateAction action) {
        HandleDialogAction(action_state, release, action);
      });
}

Checker* active_checker = nullptr;

}

Checker::Checker() : data_(std::make_unique<Data>()) { active_checker = this; }

Checker::~Checker() {
  Stop();
  if (active_checker == this) {
    active_checker = nullptr;
  }
}

void Checker::Attach(rex::ui::WindowedAppContext& context, rex::ui::ImGuiDrawer* drawer,
                     const std::filesystem::path& user_data_path) {
  data_->context = &context;
  data_->drawer = drawer;
  data_->cache_path = CachePath(user_data_path);
}

void Checker::CheckNow() {
  auto& data = *data_;
  if (!data.context || !data.drawer) {
    return;
  }
  if (data.manual && (data.manual->busy || data.manual->notice || data.manual->dialog)) {
    return;
  }
  if (data.manual_worker.joinable()) {
    data.manual_worker.join();
  }

  try {
    auto state = std::make_shared<UpdateState>();
    state->context = data.context;
    state->drawer = data.drawer;
    state->cache_path = data.cache_path;
    state->local_version = SDOJ_RELEASE_VERSION;
    data.manual = state;
    if (!ParseVersion(state->local_version)) {
      ShowNotice(state, "No updates", "Development builds do not check for updates.");
      return;
    }

    state->busy = true;
    state->notice = std::make_unique<popups::Message>(
        state->drawer, "Checking for updates", "Contacting GitHub...");
    data.manual_worker = std::thread([state] {
      std::optional<LatestRelease> release;
      try {
        CacheData cache = ReadCacheFile(state->cache_path);
        release = FetchRelease(state, cache);
      } catch (...) {
      }
      if (!state->active.load(std::memory_order_acquire) || !state->context) {
        return;
      }
      const std::weak_ptr<UpdateState> weak_state = state;
      try {
        state->context->CallInUIThreadDeferred([weak_state, release] {
          const auto current = weak_state.lock();
          if (current && current->active.load(std::memory_order_acquire)) {
            ShowManualResult(current, release);
          }
        });
      } catch (...) {
      }
    });
  } catch (...) {
    if (data.manual) {
      data.manual->busy = false;
      data.manual->notice.reset();
    }
  }
}

void CheckNowFromSettings() {
  if (active_checker) {
    active_checker->CheckNow();
  }
}

void Checker::Start(rex::ui::WindowedAppContext& context, rex::ui::ImGuiDrawer* drawer,
                    const std::filesystem::path& user_data_path) {
  if (data_->state || data_->worker.joinable()) {
    return;
  }
  try {
    const auto local_version = ParseVersion(SDOJ_RELEASE_VERSION);
    const bool enabled = REXCVAR_GET(check_for_updates);
    if (!drawer || !local_version || !enabled) {
      return;
    }

    auto state = std::make_shared<UpdateState>();
    state->context = &context;
    state->drawer = drawer;
    state->cache_path = CachePath(user_data_path);
    state->local_version = SDOJ_RELEASE_VERSION;
    data_->state = state;
    data_->worker = std::thread([state, enabled] {
      try {
        CheckForUpdate(state, enabled);
      } catch (...) {
      }
    });
  } catch (...) {
    if (data_->state) {
      data_->state->active.store(false, std::memory_order_release);
    }
    data_->state.reset();
  }
}

void Checker::StartBeforeSetup(
    rex::ui::WindowedAppContext& context, rex::ui::ImGuiDrawer* drawer,
    const std::filesystem::path& user_data_path, std::function<void()> completion) {
  if (data_->state || data_->worker.joinable()) {
    return;
  }

  std::shared_ptr<UpdateState> state;
  try {
    state = std::make_shared<UpdateState>();
    state->before_setup = true;
    state->context = &context;
    state->drawer = drawer;
    state->completion = std::move(completion);
    data_->state = state;

    state->cache_path = CachePath(user_data_path);
    state->local_version = SDOJ_RELEASE_VERSION;
    const auto local_version = ParseVersion(state->local_version);
    const bool enabled = REXCVAR_GET(check_for_updates);
    if (!drawer || !local_version || !enabled) {
      QueueFirstSetupCompletion(state);
      return;
    }

    data_->worker = std::thread([state, enabled] {
      bool dialog_pending = false;
      try {
        dialog_pending = CheckForUpdate(state, enabled);
      } catch (...) {
      }
      if (!dialog_pending) {
        QueueFirstSetupCompletion(state);
      }
    });
  } catch (...) {
    if (state) {
      QueueFirstSetupCompletion(state);
    } else if (completion) {
      try {
        if (context.IsInUIThread()) {
          completion();
        } else {
          context.CallInUIThreadDeferred(std::move(completion));
        }
      } catch (...) {
      }
    }
  }
}

void Checker::Stop() {
  data_->context = nullptr;
  data_->drawer = nullptr;
  const std::array<std::shared_ptr<UpdateState>, 2> states{data_->state, data_->manual};
  for (const auto& state : states) {
    if (state) {
      state->active.store(false, std::memory_order_release);
    }
  }
  if (data_->worker.joinable()) {
    data_->worker.join();
  }
  if (data_->manual_worker.joinable()) {
    data_->manual_worker.join();
  }
  for (const auto& state : states) {
    if (state) {
      state->dialog.reset();
      state->notice.reset();
      state->drawer = nullptr;
      state->context = nullptr;
    }
  }
  data_->state.reset();
  data_->manual.reset();
}

}
