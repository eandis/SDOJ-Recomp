#pragma once
#include "community/http_client.h"
#include "community/guest_service.h"
#include <chrono>
#include <map>

namespace cave::community {
inline constexpr uint64_t kNativeCommunityLocalXuid = 0xB13EBABEBABEBABEull;
std::string NativeLegacyName(std::string_view nickname);
struct NativeSchema {
  std::string_view game, replay_namespace, replay_format;
  uint32_t replay_header = 0, replay_max = 0;
  bool (*valid_view)(uint32_t) = nullptr;
  nlohmann::json (*encode_score)(const sdoj::xam::CommunityWrite&) = nullptr;
  std::vector<sdoj::xam::CommunityColumn> (*decode_columns)(uint32_t, const nlohmann::json&,
                                                            uint32_t) = nullptr;
  uint32_t (*column_type)(uint16_t) = nullptr;
  std::string (*native_name)(std::string_view) = nullptr;
  std::string (*run_key)(const nlohmann::json&) = nullptr;
  void (*match_replay)(uint32_t, const nlohmann::json&, std::span<const uint8_t>) = nullptr;
  uint32_t storage_size_property = 0;
};

// One bounded worker and native token/reference cache. Title adapters supply
// view/property/replay semantics; transport never invents another game's rows.
class NativeService : public sdoj::xam::CommunityService {
 public:
  NativeService(NativeSchema schema, std::string endpoint, std::string nickname, std::string build);
  ~NativeService() override;
  void Read(sdoj::xam::CommunityRead, ReadCompletion) override;
  void Write(std::vector<sdoj::xam::CommunityWrite>, WriteCompletion) override;
  void Estimate(std::vector<sdoj::xam::CommunityRank>, RankCompletion) override;
  void Replay(sdoj::xam::CommunityReplay, ReplayCompletion) override;
  void Stop() override;
  std::string DisplayName(uint64_t xuid) const override;
  std::string_view ReplayNamespace() const override { return schema_.replay_namespace; }

 private:
  using Clock = std::chrono::steady_clock;
  struct Task {
    Clock::time_point deadline;
    std::function<void(Clock::time_point)> work;
    std::function<void(uint32_t)> fail;
  };
  void Enqueue(Task task);
  void Run();
  HttpResult Http(std::string path, nlohmann::json body, Clock::time_point deadline);
  sdoj::xam::CommunityResult ReadNow(const sdoj::xam::CommunityRead&, Clock::time_point);
  uint32_t WriteNow(const std::vector<sdoj::xam::CommunityWrite>&, Clock::time_point);
  sdoj::xam::CommunityRankResult EstimateNow(const std::vector<sdoj::xam::CommunityRank>&, Clock::time_point);
  sdoj::xam::CommunityReplayResult ReplayNow(const sdoj::xam::CommunityReplay&, Clock::time_point);
  struct ScoreReference {
    std::string id, replay, hash;
    uint32_t bytes = 0;
    nlohmann::json properties;
  };
  std::string upload_run_;
  std::map<uint32_t, std::string> submission_ids_;
  std::map<uint32_t, ScoreReference> uploads_;
  std::map<std::pair<uint32_t, uint64_t>, ScoreReference> downloads_;
  uint64_t NativeToken(const std::string& nickname);
  const NativeSchema schema_;
  const std::string nickname_, build_;
  HttpClient client_;
  std::mutex mutex_, stop_mutex_;
  mutable std::mutex names_mutex_;
  std::condition_variable wake_;
  std::deque<Task> tasks_;
  std::map<uint64_t, std::string> native_names_;
  uint64_t next_token_ = 0xB000000000000001ull;
  std::atomic<bool> stopping_{false};
  std::thread worker_;
};
}  // namespace cave::community
