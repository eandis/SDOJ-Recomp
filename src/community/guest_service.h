#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rex::system {
class KernelState;
class XEnumerator;
}  // namespace rex::system

namespace sdoj::xam {
using rex::system::KernelState;
using rex::system::XEnumerator;

struct CommunityColumn {
  uint32_t id = 0;
  uint32_t type = 1;
  int64_t integer = 0;
  double real = 0;
};
struct CommunitySpec {
  uint32_t view = 0;
  std::vector<uint16_t> columns;
};
struct CommunityRow {
  uint64_t xuid = 0;  // Native ABI token, never a community account identity.
  uint32_t rank = 0;
  int64_t rating = 0;
  std::string nickname;
  std::vector<CommunityColumn> columns;
  std::string native_tag;  // Optional title-specific <=15-byte legacy display encoding.
};
struct CommunityView {
  uint32_t view = 0;
  uint32_t total = 0;
  std::vector<CommunityRow> rows;
};
struct CommunityRead {
  uint32_t type = 1;
  uint64_t pivot = 1;
  uint32_t count = 20;
  std::vector<uint64_t> xuids;
  std::vector<CommunitySpec> specs;
};
struct CommunityWrite {
  uint32_t view = 0;
  std::vector<CommunityColumn> columns;
};
struct CommunityResult {
  uint32_t error = 0;
  std::vector<CommunityView> views;
};
struct CommunityReplay {
  uint32_t view = 0;
  uint64_t xuid = 0;
  bool upload = false;
  uint32_t capacity = 0;
  std::vector<uint8_t> record;
};
struct CommunityReplayResult {
  uint32_t error = 0;
  std::vector<uint8_t> record;
};
struct CommunityRank {
  uint32_t view;
  int64_t rating;
};
struct CommunityRankResult {
  uint32_t error = 0;
  std::vector<uint32_t> ranks;
};

class CommunityService {
 public:
  using ReadCompletion = std::function<void(CommunityResult)>;
  using WriteCompletion = std::function<void(uint32_t)>;
  using ReplayCompletion = std::function<void(CommunityReplayResult)>;
  using RankCompletion = std::function<void(CommunityRankResult)>;
  virtual ~CommunityService() = default;
  virtual void Read(CommunityRead request, ReadCompletion completion) = 0;
  virtual void Write(std::vector<CommunityWrite> views, WriteCompletion completion) = 0;
  virtual void Replay(CommunityReplay request, ReplayCompletion completion) = 0;
  virtual void Estimate(std::vector<CommunityRank> request, RankCompletion completion) = 0;
  virtual void Stop() = 0;
  virtual bool AllowsPrivilege(uint32_t) const { return false; }
  // Cached Unicode display only; must not perform network or disk IO.
  virtual std::string DisplayName(uint64_t xuid) const { return {}; }
  // Title-specific namespace for opaque replay paths.
  virtual std::string_view ReplayNamespace() const = 0;
};

// Install before guest launch. Detach before cancelling native I/O and stopping
// the provider, while the kernel is still available for guest completions.
void SetCommunityService(std::shared_ptr<CommunityService> service);
std::shared_ptr<CommunityService> GetCommunityService();
void CancelCommunityIo(KernelState* kernel);
bool CancelCommunityIo(KernelState* kernel, uint32_t overlapped);

uint32_t CreateCommunityStatsEnumerator(KernelState* kernel, uint32_t title, uint32_t type, uint64_t pivot,
                                        uint32_t count, uint32_t spec_count, uint32_t specs,
                                        uint32_t buffer_size, uint32_t handle);
std::optional<uint32_t> EnumerateCommunityStats(KernelState* kernel, XEnumerator* enumerator, uint32_t buffer,
                                                uint32_t buffer_size, uint32_t overlapped,
                                                uint32_t* items_returned = nullptr);
std::optional<uint32_t> DispatchCommunityMessage(KernelState* kernel, uint32_t app, uint32_t message,
                                                 uint32_t buffer, uint32_t length, uint32_t overlapped);

// Pure serializer, also used by host tests. Guest pointers refer to the caller's
// destination, never a temporary SystemHeap allocation.
bool SerializeCommunityStats(const CommunityResult& result, std::span<const CommunitySpec> specs,
                             uint32_t guest_address, std::span<uint8_t> bytes);
uint32_t CommunityStatsBufferSize(std::span<const CommunitySpec> specs, uint32_t rows);

// Opaque title-specific XStorage paths: never host files or arbitrary URLs.
std::u16string CommunityReplayPath(uint32_t view, uint64_t xuid);
bool ParseCommunityReplayPath(std::u16string_view path, uint32_t& view, uint64_t& xuid);
constexpr bool CommunityPageAccessAllowed(uint32_t access, bool write) {
  const uint32_t required = write ? 3 : 1;  // Read bit, plus write bit when needed.
  return (access & required) == required;
}
uint32_t BuildCommunityReplayPath(KernelState* kernel, uint32_t user, uint64_t xuid, uint32_t facility,
                                  uint32_t info, uint32_t info_size, uint32_t filename, uint32_t destination,
                                  uint32_t length);
uint32_t StartCommunityReplayIo(KernelState* kernel, bool upload, uint32_t user, uint32_t path,
                                uint32_t capacity, uint32_t buffer, uint32_t result_size, uint32_t result,
                                uint32_t overlapped);
uint32_t StartCommunityRankIo(KernelState* kernel, uint32_t count, uint32_t requests, uint32_t capacity,
                              uint32_t result, uint32_t overlapped);
}  // namespace sdoj::xam
