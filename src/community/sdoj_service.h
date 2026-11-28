#pragma once
#include "community/native_service.h"

namespace sdoj {
nlohmann::json EncodeCommunityScore(const sdoj::xam::CommunityWrite&);
std::vector<sdoj::xam::CommunityColumn> DecodeCommunityColumns(const nlohmann::json&,
                                                               uint32_t replay_bytes = 0);
class SdojCommunity final : public cave::community::NativeService {
 public:
  SdojCommunity(std::string endpoint, std::string nickname, std::string build, bool ranked);
  bool AllowsPrivilege(uint32_t privilege) const override;
  void Write(std::vector<sdoj::xam::CommunityWrite>, WriteCompletion) override;
  void Replay(sdoj::xam::CommunityReplay, ReplayCompletion) override;

 private:
  const bool ranked_;
};
}  // namespace sdoj
