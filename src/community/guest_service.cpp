#include "community/guest_service.h"

#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xenumerator.h>
#include <rex/system/xevent.h>
#include <rex/system/xio.h>
#include <rex/system/xthread.h>

#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <map>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdexcept>

namespace sdoj::xam {
using namespace rex;
using namespace rex::system;
namespace {
constexpr uint32_t kAborted = 995;
constexpr uint32_t kNotSupported = 50;
std::mutex service_mutex;
std::shared_ptr<CommunityService> service;

uint32_t Read32(const uint8_t* p) { return memory::load_and_swap<uint32_t>(p); }
uint64_t Read64(const uint8_t* p) { return memory::load_and_swap<uint64_t>(p); }
void Put32(uint8_t* p, uint32_t v) { memory::store_and_swap<uint32_t>(p, v); }
void Put64(uint8_t* p, uint64_t v) { memory::store_and_swap<uint64_t>(p, v); }

bool Valid(KernelState* kernel, uint32_t address, uint32_t size, bool write = false) {
  if (!address || !size || uint64_t(address) + size > 0x100000000ull)
    return false;
  auto* heap = kernel->memory()->LookupHeap(address);
  const auto end = address + size - 1;
  if (!heap || heap != kernel->memory()->LookupHeap(end))
    return false;
  auto access = heap->QueryRangeAccess(address, end);
  return CommunityPageAccessAllowed(uint32_t(access), write);
}

bool ReadPath(KernelState* kernel, uint32_t address, std::u16string& value) {
  for (uint32_t i = 0; i < 256; ++i) {
    auto next = uint64_t(address) + i * 2;
    if (next > 0xfffffffe || !Valid(kernel, uint32_t(next), 2))
      return false;
    auto c = memory::load_and_swap<uint16_t>(kernel->memory()->TranslateVirtual(uint32_t(next)));
    if (!c)
      return !value.empty();
    value.push_back(char16_t(c));
  }
  return false;
}

bool ReadSpecs(KernelState* kernel, uint32_t address, uint32_t count, std::vector<CommunitySpec>& result) {
  if (!count || count > 16 || !Valid(kernel, address, count * 136))
    return false;
  auto* p = kernel->memory()->TranslateVirtual(address);
  for (uint32_t i = 0; i < count; ++i, p += 136) {
    auto columns = Read32(p + 4);
    if (columns > 64)
      return false;
    CommunitySpec spec{Read32(p), {}};
    for (uint32_t j = 0; j < columns; ++j)
      spec.columns.push_back(memory::load_and_swap<uint16_t>(p + 8 + j * 2));
    result.push_back(std::move(spec));
  }
  return true;
}

struct Pending;
std::mutex pending_mutex;
// Synchronous calls have no guest OVERLAPPED. Give each one a separate host
// token so concurrent calls can still be cancelled when the runtime closes.
using Key = std::pair<KernelState*, uint64_t>;
std::map<Key, std::shared_ptr<Pending>> pending;
std::atomic<uint64_t> next_sync_token{0x100000000ull};
struct Pending {
  KernelState* kernel;
  uint32_t overlapped;
  uint64_t token;
  std::mutex mutex;
  std::condition_variable wake;
  bool done = false;
  uint32_t result = 0, length = 0;

  void Complete(uint32_t error, std::function<uint32_t()> writer = {}, uint32_t items = 0) {
    {
      std::lock_guard lock(mutex);
      if (done)
        return;
      if (!error && writer)
        error = writer();
      done = true;
      result = error;
      length = items;
      // Request context was captured on the guest thread. This completion runs
      // on a bounded host worker, with no wait on the kernel dispatch thread.
      if (overlapped)
        kernel->CompleteOverlappedEx(overlapped, error ? X_ERROR_FUNCTION_FAILED : X_ERROR_SUCCESS,
                                     X_HRESULT_FROM_WIN32(error), error ? 0 : items);
    }
    wake.notify_all();
    std::lock_guard lock(pending_mutex);
    auto it = pending.find({kernel, token});
    if (it != pending.end() && it->second.get() == this)
      pending.erase(it);
  }

  uint32_t Wait(uint32_t* items = nullptr) {
    // Only the calling guest thread waits, never the kernel's shared IO
    // dispatcher. Provider work remains on its bounded HTTP worker. Timeout
    // and shutdown close this operation before a late callback can write RAM.
    std::unique_lock lock(mutex);
    if (!wake.wait_for(lock, std::chrono::seconds(12), [this] { return done; })) {
      lock.unlock();
      Complete(1460);
      lock.lock();
    }
    if (items)
      *items = result ? 0 : length;
    return result;
  }
};

std::shared_ptr<Pending> Begin(KernelState* kernel, uint32_t overlapped, const CommunityService* provider) {
  if (overlapped && !Valid(kernel, overlapped, sizeof(XAM_OVERLAPPED), true))
    return {};
  auto operation = std::make_shared<Pending>();
  operation->kernel = kernel;
  operation->overlapped = overlapped;
  operation->token = overlapped ? overlapped : next_sync_token.fetch_add(1);
  if (!overlapped) {
    std::lock_guard lock(pending_mutex);
    if (GetCommunityService().get() != provider)
      return {};
    pending.emplace(Key{kernel, operation->token}, operation);
    return operation;
  }
  std::shared_ptr<Pending> previous;
  {
    std::lock_guard lock(pending_mutex);
    auto it = pending.find({kernel, overlapped});
    if (it != pending.end())
      previous = it->second;
  }
  std::unique_lock<std::mutex> previous_lock;
  if (previous)
    previous_lock = std::unique_lock(previous->mutex);
  std::lock_guard lock(pending_mutex);
  if (GetCommunityService().get() != provider)
    return {};
  auto existing = pending.find({kernel, overlapped});
  if (existing != pending.end()) {
    if (existing->second != previous || !previous->done)
      return {};
    pending.erase(existing);
  }
  auto* ov = kernel->memory()->TranslateVirtual(overlapped);
  if (auto event = XOverlappedGetEvent(ov)) {
    auto ev = kernel->object_table()->LookupObject<XEvent>(event);
    if (!ev)
      return {};
    ev->Reset();
  }
  XOverlappedSetContext(ov, XThread::GetCurrentThreadHandle());
  XOverlappedSetResult(ov, X_ERROR_IO_PENDING);
  XOverlappedSetExtendedError(ov, X_ERROR_IO_PENDING);
  XOverlappedSetLength(ov, 0);
  pending.emplace(Key{kernel, overlapped}, operation);
  return operation;
}

class StatsEnumerator final : public XEnumerator {
 public:
  StatsEnumerator(KernelState* kernel, CommunityRead request, uint32_t capacity,
                  std::shared_ptr<CommunityService> provider)
      : XEnumerator(kernel, 1, capacity), request_(std::move(request)), provider_(std::move(provider)) {}
  uint32_t WriteItems(uint32_t, uint8_t*, uint32_t*) override {
    // Both sync and async calls are intercepted before the generic dispatcher;
    // it must never turn a provider request into shared-dispatcher work.
    return X_ERROR_BUSY;
  }
  uint32_t BeginWrite(uint32_t buffer, uint32_t capacity, uint32_t overlapped, uint32_t* items) {
    if (items)
      *items = 0;
    if ((!overlapped && !items) || capacity < item_size() ||
        !Valid(kernel_state(), buffer, uint32_t(item_size()), true))
      return X_ERROR_INVALID_PARAMETER;
    capacity = uint32_t(item_size());
    {
      std::lock_guard lock(mutex_);
      if (consumed_)
        return X_ERROR_NO_MORE_FILES;
      if (inflight_)
        return X_ERROR_BUSY;
      inflight_ = true;
    }
    auto operation = Begin(kernel_state(), overlapped, provider_.get());
    if (!operation) {
      std::lock_guard lock(mutex_);
      inflight_ = false;
      return X_ERROR_INVALID_PARAMETER;
    }
    auto self = retain_object(this);
    provider_->Read(request_, [self, operation, buffer, capacity](CommunityResult result) {
      {
        std::lock_guard lock(self->mutex_);
        self->inflight_ = false;
        self->consumed_ = true;
      }
      operation->Complete(
          result.error,
          [&]() -> uint32_t {
            auto* kernel = self->kernel_state();
            if (!Valid(kernel, buffer, capacity, true))
              return X_ERROR_INVALID_PARAMETER;
            auto* bytes = kernel->memory()->TranslateVirtual(buffer);
            if (!SerializeCommunityStats(result, self->request_.specs, buffer, {bytes, capacity}))
              return X_ERROR_INSUFFICIENT_BUFFER;
            return 0;
          },
          1);
    });
    return overlapped ? X_ERROR_IO_PENDING : operation->Wait(items);
  }

 private:
  CommunityRead request_;
  std::shared_ptr<CommunityService> provider_;
  std::mutex mutex_;
  bool inflight_ = false, consumed_ = false;
};
}  // namespace

void SetCommunityService(std::shared_ptr<CommunityService> value) {
  std::lock_guard lock(service_mutex);
  service = std::move(value);
}
std::shared_ptr<CommunityService> GetCommunityService() {
  std::lock_guard lock(service_mutex);
  return service;
}
bool CancelCommunityIo(KernelState* kernel, uint32_t overlapped) {
  if (!overlapped)
    return false;
  std::shared_ptr<Pending> operation;
  {
    std::lock_guard lock(pending_mutex);
    auto it = pending.find({kernel, overlapped});
    if (it != pending.end())
      operation = it->second;
  }
  if (!operation)
    return false;
  operation->Complete(kAborted);
  return true;
}
void CancelCommunityIo(KernelState* kernel) {
  std::vector<std::shared_ptr<Pending>> operations;
  {
    std::lock_guard lock(pending_mutex);
    for (auto& [key, value] : pending)
      if (key.first == kernel)
        operations.push_back(value);
  }
  for (auto& operation : operations)
    operation->Complete(kAborted);
}

std::u16string CommunityReplayPath(uint32_t view, uint64_t xuid) {
  auto provider = GetCommunityService();
  if (!provider)
    return {};
  const auto prefix = provider->ReplayNamespace();
  if (prefix.empty() || prefix.size() > 32 ||
      prefix.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-") != prefix.npos)
    return {};
  std::ostringstream text;
  text << prefix << '/' << std::hex << std::setfill('0') << std::setw(8) << view << '/' << std::setw(16)
       << xuid;
  auto path = text.str();
  return {path.begin(), path.end()};
}
bool ParseCommunityReplayPath(std::u16string_view path, uint32_t& view, uint64_t& xuid) {
  const auto canonical = CommunityReplayPath(0, 0);
  if (canonical.empty())
    return false;
  const auto prefix = std::u16string_view(canonical).substr(0, canonical.size() - 25);
  if (path.size() != prefix.size() + 8 + 1 + 16 || !path.starts_with(prefix) ||
      path[prefix.size() + 8] != u'/')
    return false;
  uint64_t values[2]{};
  for (size_t field = 0; field < 2; ++field) {
    auto part = path.substr(prefix.size() + (field ? 9 : 0), field ? 16 : 8);
    for (auto c : part) {
      if (!((c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'f')))
        return false;
      values[field] = (values[field] << 4) | (c <= u'9' ? c - u'0' : c - u'a' + 10);
    }
  }
  view = uint32_t(values[0]);
  xuid = values[1];
  return true;
}

uint32_t BuildCommunityReplayPath(KernelState* kernel, uint32_t user, uint64_t xuid, uint32_t facility,
                                  uint32_t info, uint32_t info_size, uint32_t filename, uint32_t destination,
                                  uint32_t length) {
  if (!GetCommunityService())
    return X_ERROR_NOT_LOGGED_ON;
  REXLOG_DEBUG(
      "Community native replay path request: user={} facility={} info_bytes={} info={:08X} filename={:08X} "
      "out={:08X} length={:08X}",
      user, facility, info_size, info, filename, destination, length);
  // This provider identifies a GameClip by view and owner, not the native
  // storage filename. Do not dereference or reflect that unused title constant:
  // it is not an input to the opaque community path or a host filesystem path.
  if ((user != 0 && user != 254) || facility != 1 || info_size != 4 || !Valid(kernel, info, 4) ||
      !Valid(kernel, length, 4, true)) {
    REXLOG_WARN("Community native replay path rejected: info_readable={} length_writable={}",
                Valid(kernel, info, 4), Valid(kernel, length, 4, true));
    return X_ERROR_INVALID_PARAMETER;
  }
  if (user == 0)
    xuid = 0xB13EBABEBABEBABEull;
  if (!xuid) {
    REXLOG_WARN("Community native replay path rejected: empty owner");
    return X_ERROR_INVALID_PARAMETER;
  }
  const auto view = Read32(kernel->memory()->TranslateVirtual(info));
  const auto path = CommunityReplayPath(view, xuid);
  if (path.empty()) {
    REXLOG_WARN("Community native replay path rejected: invalid provider namespace");
    return X_ERROR_INVALID_PARAMETER;
  }
  auto* size = kernel->memory()->TranslateVirtual(length);
  const auto capacity = Read32(size), required = uint32_t((path.size() + 1) * 2);
  REXLOG_DEBUG("Community native replay path buffer: view={} capacity={} required={}", view, capacity,
               required);
  Put32(size, required);
  if (capacity < required)
    return X_ERROR_INSUFFICIENT_BUFFER;
  if (!Valid(kernel, destination, required, true)) {
    REXLOG_WARN("Community native replay path rejected: output not writable");
    return X_ERROR_INVALID_PARAMETER;
  }
  auto* out = kernel->memory()->TranslateVirtual(destination);
  for (size_t i = 0; i < path.size(); ++i)
    memory::store_and_swap<uint16_t>(out + i * 2, uint16_t(path[i]));
  memory::store_and_swap<uint16_t>(out + path.size() * 2, 0);
  REXLOG_DEBUG("Community native replay path: view={} remote={}", view, user == 254);
  return X_ERROR_SUCCESS;
}

uint32_t StartCommunityReplayIo(KernelState* kernel, bool upload, uint32_t user, uint32_t path,
                                uint32_t capacity, uint32_t buffer, uint32_t result_size, uint32_t result,
                                uint32_t overlapped) {
  auto provider = GetCommunityService();
  if (!provider)
    return X_ERROR_NOT_LOGGED_ON;
  REXLOG_DEBUG(
      "Community native replay IO request: upload={} user={} bytes={} path={:08X} buffer={:08X} "
      "result_size={} overlapped={:08X}",
      upload, user, capacity, path, buffer, result_size, overlapped);
  CommunityReplay request;
  std::u16string name;
  if (user || !capacity || capacity > 8 * 1024 * 1024 || !ReadPath(kernel, path, name) ||
      !ParseCommunityReplayPath(name, request.view, request.xuid) ||
      !Valid(kernel, buffer, capacity, !upload) ||
      (!upload && (result_size != 20 || !Valid(kernel, result, 20, true))) ||
      (upload && request.xuid != 0xB13EBABEBABEBABEull))
    return X_ERROR_INVALID_PARAMETER;
  request.upload = upload;
  request.capacity = capacity;
  if (upload) {
    auto* data = kernel->memory()->TranslateVirtual(buffer);
    request.record.assign(data, data + capacity);
  }
  auto operation = Begin(kernel, overlapped, provider.get());
  if (!operation)
    return X_ERROR_INVALID_PARAMETER;
  const auto owner = request.xuid;
  REXLOG_INFO("Community native replay IO: upload={} view={} bytes={}", upload, request.view, capacity);
  const auto view = request.view;
  provider->Replay(std::move(request), [kernel, operation, upload, capacity, buffer, result, owner,
                                        view](CommunityReplayResult reply) {
    const auto bytes = uint32_t(reply.record.size());
    REXLOG_INFO("Community native replay completion: upload={} view={} error={} bytes={}", upload, view,
                reply.error, bytes);
    operation->Complete(
        reply.error,
        [&]() -> uint32_t {
          if (upload)
            return 0;
          if (!bytes || bytes > capacity)
            return X_ERROR_INSUFFICIENT_BUFFER;
          if (!Valid(kernel, buffer, capacity, true) || !Valid(kernel, result, 20, true))
            return X_ERROR_INVALID_PARAMETER;
          std::memcpy(kernel->memory()->TranslateVirtual(buffer), reply.record.data(), bytes);
          auto* out = kernel->memory()->TranslateVirtual(result);
          // Packed native X_STORAGE_DOWNLOAD_TO_MEMORY_RESULTS: size, XUID, FILETIME.
          // The original replay carries its own recording time; zero means unknown.
          std::memset(out, 0, 20);
          Put32(out, bytes);
          Put64(out + 4, owner);
          return 0;
        },
        upload ? 0 : bytes);
  });
  return overlapped ? X_ERROR_IO_PENDING : operation->Wait();
}

uint32_t CommunityStatsBufferSize(std::span<const CommunitySpec> specs, uint32_t rows) {
  if (specs.empty() || specs.size() > 16 || rows > 100)
    return 0;
  uint32_t size = 8 + uint32_t(specs.size()) * 16;
  for (const auto& spec : specs) {
    if (spec.columns.size() > 64)
      return 0;
    size += rows * (48 + uint32_t(spec.columns.size()) * 24);
  }
  return size;
}

uint32_t StartCommunityRankIo(KernelState* kernel, uint32_t count, uint32_t requests, uint32_t capacity,
                              uint32_t result, uint32_t overlapped) {
  auto provider = GetCommunityService();
  if (!provider)
    return X_ERROR_NOT_LOGGED_ON;
  if (!count || count > 16 || capacity < 8 + count * 4 || !Valid(kernel, requests, count * 12) ||
      !Valid(kernel, result, 8 + count * 4, true))
    return X_ERROR_INVALID_PARAMETER;
  std::vector<CommunityRank> ranks;
  auto* bytes = kernel->memory()->TranslateVirtual(requests);
  for (uint32_t i = 0; i < count; ++i, bytes += 12) {
    auto rating = int64_t(Read64(bytes + 4));
    if (rating < 0)
      return X_ERROR_INVALID_PARAMETER;
    ranks.push_back({Read32(bytes), rating});
  }
  auto operation = Begin(kernel, overlapped, provider.get());
  if (!operation)
    return X_ERROR_INVALID_PARAMETER;
  provider->Estimate(std::move(ranks), [kernel, operation, count, result](CommunityRankResult reply) {
    operation->Complete(reply.error, [&]() -> uint32_t {
      if (reply.ranks.size() != count || !Valid(kernel, result, 8 + count * 4, true))
        return X_ERROR_INVALID_PARAMETER;
      auto* out = kernel->memory()->TranslateVirtual(result);
      Put32(out, count);
      Put32(out + 4, result + 8);
      for (uint32_t i = 0; i < count; ++i)
        Put32(out + 8 + i * 4, reply.ranks[i]);
      return 0;
    });
  });
  return overlapped ? X_ERROR_IO_PENDING : operation->Wait();
}

bool SerializeCommunityStats(const CommunityResult& result, std::span<const CommunitySpec> specs,
                             uint32_t guest_address, std::span<uint8_t> bytes) {
  if (result.error || specs.empty() || specs.size() > 16 || bytes.size() < 8 + specs.size() * 16 ||
      uint64_t(guest_address) + bytes.size() > 0x100000000ull)
    return false;
  std::fill(bytes.begin(), bytes.end(), 0);
  size_t cursor = 8 + specs.size() * 16;
  Put32(bytes.data(), uint32_t(specs.size()));
  Put32(bytes.data() + 4, guest_address + 8);
  for (size_t i = 0; i < specs.size(); ++i) {
    const auto& spec = specs[i];
    if (spec.columns.size() > 64)
      return false;
    auto found = std::find_if(result.views.begin(), result.views.end(),
                              [&](const auto& v) { return v.view == spec.view; });
    auto* view = bytes.data() + 8 + i * 16;
    Put32(view, spec.view);
    if (found == result.views.end())
      continue;
    const auto& data = *found;
    if (data.rows.size() > 100 || cursor + data.rows.size() * 48 > bytes.size())
      return false;
    Put32(view + 4, data.total);
    Put32(view + 8, uint32_t(data.rows.size()));
    if (!data.rows.empty())
      Put32(view + 12, guest_address + uint32_t(cursor));
    auto rows_offset = cursor;
    cursor += data.rows.size() * 48;
    for (size_t j = 0; j < data.rows.size(); ++j) {
      const auto& row = data.rows[j];
      auto* p = bytes.data() + rows_offset + j * 48;
      Put64(p, row.xuid);
      Put32(p + 8, row.rank);
      Put64(p + 16, uint64_t(row.rating));
      const auto& tag = row.native_tag.empty() ? row.nickname : row.native_tag;
      if (!row.native_tag.empty() && tag.size() > 15)
        return false;
      size_t name_size = std::min(size_t(15), tag.size());
      if (row.native_tag.empty() && name_size < tag.size())
        while (name_size && (uint8_t(tag[name_size]) & 0xc0) == 0x80)
          --name_size;
      std::memcpy(p + 24, tag.data(), name_size);
      uint32_t columns = 0;
      for (auto id : spec.columns)
        if (id != 0x8000 && id != 0x8001 && id != 0x8002)
          ++columns;
      if (cursor + columns * 24 > bytes.size())
        return false;
      Put32(p + 40, columns);
      if (columns)
        Put32(p + 44, guest_address + uint32_t(cursor));
      for (auto id : spec.columns) {
        if (id == 0x8000 || id == 0x8001 || id == 0x8002)
          continue;
        auto* c = bytes.data() + cursor;
        cursor += 24;
        memory::store_and_swap<uint16_t>(c, id);
        auto value = std::find_if(row.columns.begin(), row.columns.end(),
                                  [id](const auto& col) { return (col.id & 0xffff) == id; });
        if (value == row.columns.end()) {
          c[8] = 1;
          continue;
        }
        c[8] = uint8_t(value->type);
        switch (value->type) {
          case 1:
            Put32(c + 16, uint32_t(value->integer));
            break;
          case 2:
          case 7:
            Put64(c + 16, uint64_t(value->integer));
            break;
          case 3:
            Put64(c + 16, std::bit_cast<uint64_t>(value->real));
            break;
          case 5:
            Put32(c + 16, std::bit_cast<uint32_t>(float(value->real)));
            break;
          default:
            return false;
        }
      }
    }
  }
  return true;
}

uint32_t CreateCommunityStatsEnumerator(KernelState* kernel, uint32_t title, uint32_t type, uint64_t pivot,
                                        uint32_t count, uint32_t spec_count, uint32_t specs,
                                        uint32_t buffer_size, uint32_t handle) {
  REXLOG_DEBUG(
      "Community native enumerate request: title={:08X} type={} rows={} views={} specs={:08X} "
      "size_out={:08X} handle_out={:08X}",
      title, type, count, spec_count, specs, buffer_size, handle);
  if (!Valid(kernel, buffer_size, 4, true) || !Valid(kernel, handle, 4, true))
    return X_ERROR_INVALID_PARAMETER;
  auto* size_out = kernel->memory()->TranslateVirtual(buffer_size);
  auto* handle_out = kernel->memory()->TranslateVirtual(handle);
  Put32(size_out, 0);
  Put32(handle_out, 0);
  auto provider = GetCommunityService();
  if (!provider)
    return X_ERROR_NOT_LOGGED_ON;
  CommunityRead request{type, pivot, count, {}, {}};
  if ((title && title != kernel->title_id()) || type > 3 || !count || count > 100 ||
      !ReadSpecs(kernel, specs, spec_count, request.specs))
    return X_ERROR_INVALID_PARAMETER;
  const auto capacity = CommunityStatsBufferSize(request.specs, count);
  auto enumerator = make_object<StatsEnumerator>(kernel, std::move(request), capacity, provider);
  auto status = enumerator->Initialize(0, 0xfb, 0xb0023, 0xb0024, 0);
  if (status) {
    enumerator->Delete();
    return X_ERROR_FUNCTION_FAILED;
  }
  Put32(size_out, capacity);
  Put32(handle_out, enumerator->handle());
  REXLOG_INFO("Community native enumerate create: type={} rows={} views={} bytes={}", type, count, spec_count,
              capacity);
  return X_ERROR_SUCCESS;
}

std::optional<uint32_t> EnumerateCommunityStats(KernelState* kernel, XEnumerator* enumerator, uint32_t buffer,
                                                uint32_t capacity, uint32_t overlapped,
                                                uint32_t* items_returned) {
  auto* stats = dynamic_cast<StatsEnumerator*>(enumerator);
  if (!stats)
    return {};
  return stats->BeginWrite(buffer, capacity, overlapped, items_returned);
}

std::optional<uint32_t> DispatchCommunityMessage(KernelState* kernel, uint32_t app, uint32_t message,
                                                 uint32_t buffer, uint32_t length, uint32_t overlapped) {
  // Xbox stats messages for app 0xfb: 0xb0021 reads named players (GET /leaderboard),
  // 0xb0025 writes stats (POST /scores or a local replay-size acknowledgement),
  // and 0xb0026 flushes completed writes locally.
  if (app != 0xfb || (message != 0xb0021 && message != 0xb0025 && message != 0xb0026))
    return {};
  auto provider = GetCommunityService();
  if (!provider)
    return {};
  if (message == 0xb0026) {
    // Individual writes complete only after the host provider has accepted them.
    if (!overlapped)
      return X_E_SUCCESS;
    auto operation = Begin(kernel, overlapped, provider.get());
    if (!operation)
      return X_E_INVALIDARG;
    operation->Complete(0);
    return X_ERROR_IO_PENDING;
  }
  if (!Valid(kernel, buffer, message == 0xb0021 ? 28 : 24) ||
      (length && length != (message == 0xb0021 ? 28 : 24)))
    return X_E_INVALIDARG;
  auto* bytes = kernel->memory()->TranslateVirtual(buffer);
  if (message == 0xb0025) {
    auto count = Read32(bytes + 16), address = Read32(bytes + 20);
    if (!count || count > 16 || !Valid(kernel, address, count * 12))
      return X_E_INVALIDARG;
    std::vector<CommunityWrite> views;
    auto* data = kernel->memory()->TranslateVirtual(address);
    for (uint32_t i = 0; i < count; ++i, data += 12) {
      CommunityWrite view{Read32(data), {}};
      auto props = Read32(data + 4), ptr = Read32(data + 8);
      if (props > 64 || (props && !Valid(kernel, ptr, props * 24)))
        return X_E_INVALIDARG;
      auto* property = kernel->memory()->TranslateVirtual(ptr);
      for (uint32_t j = 0; j < props; ++j, property += 24) {
        CommunityColumn col{Read32(property), property[8], 0, 0};
        switch (col.type) {
          case 1:
            col.integer = int32_t(Read32(property + 16));
            break;
          case 2:
          case 7:
            col.integer = int64_t(Read64(property + 16));
            break;
          case 3:
            col.real = std::bit_cast<double>(Read64(property + 16));
            break;
          case 5:
            col.real = std::bit_cast<float>(Read32(property + 16));
            break;
          default:
            REXLOG_WARN("Community native unsupported property: id={:08X} type={}", col.id, col.type);
            return X_HRESULT_FROM_WIN32(kNotSupported);
        }
        view.columns.push_back(col);
      }
      views.push_back(std::move(view));
    }
    auto operation = Begin(kernel, overlapped, provider.get());
    if (!operation)
      return X_E_INVALIDARG;
    provider->Write(std::move(views), [operation](uint32_t error) { operation->Complete(error); });
    if (overlapped)
      return X_ERROR_IO_PENDING;
    const auto error = operation->Wait();
    return X_HRESULT_FROM_WIN32(error);
  }
  CommunityRead request;
  request.count = Read32(bytes + 4);
  auto xuids = Read32(bytes + 8), specs = Read32(bytes + 16), capacity = Read32(bytes + 20),
       destination = Read32(bytes + 24);
  if ((Read32(bytes) && Read32(bytes) != kernel->title_id()) || !request.count || request.count > 100 ||
      !Valid(kernel, xuids, request.count * 8) ||
      !ReadSpecs(kernel, specs, Read32(bytes + 12), request.specs) || capacity > 4 * 1024 * 1024 ||
      capacity < CommunityStatsBufferSize(request.specs, request.count) ||
      !Valid(kernel, destination, capacity, true))
    return X_E_INVALIDARG;
  for (uint32_t i = 0; i < request.count; ++i)
    request.xuids.push_back(Read64(kernel->memory()->TranslateVirtual(xuids + i * 8)));
  auto operation = Begin(kernel, overlapped, provider.get());
  if (!operation)
    return X_E_INVALIDARG;
  provider->Read(request, [kernel, operation, request, destination, capacity](CommunityResult result) {
    operation->Complete(result.error, [&]() -> uint32_t {
      if (!Valid(kernel, destination, capacity, true))
        return X_ERROR_INVALID_PARAMETER;
      auto* out = kernel->memory()->TranslateVirtual(destination);
      return SerializeCommunityStats(result, request.specs, destination, {out, capacity})
                 ? 0
                 : X_ERROR_INSUFFICIENT_BUFFER;
    });
  });
  if (overlapped)
    return X_ERROR_IO_PENDING;
  const auto error = operation->Wait();
  return X_HRESULT_FROM_WIN32(error);
}
}  // namespace sdoj::xam
