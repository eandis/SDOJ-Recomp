#include "community/session.h"
#include "community/sdoj_service.h"
#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/system/kernel_state.h>

REXCVAR_DECLARE(bool, debug);
REXCVAR_DEFINE_BOOL(community_enabled, true, "SDOJ", "Enable personal leaderboards and replay sharing")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);
REXCVAR_DEFINE_STRING(player_name, "Player", "SDOJ", "Name shown on community scoreboards")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);
REXCVAR_DEFINE_STRING(community_endpoint, "https://xiaolu.life/cave/api/v1", "SDOJ",
                      "Community leaderboard server URL")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

namespace sdoj {
void CommunitySession::Start() {
  if (!REXCVAR_GET(community_enabled))
    return;
  const auto endpoint = REXCVAR_GET(community_endpoint);
  if (endpoint.empty())
    return;
  try {
    const auto name = cave::community::NormalizeNickname(REXCVAR_GET(player_name));
    if (!cave::community::ValidNickname(name))
      throw std::invalid_argument("Invalid player name");
    // Muteki and other original debug tools are enabled for an entire launch.
    // Retain online reads in that mode and reject score and replay writes.
    const std::string build =
        SDOJ_RELEASE_VERSION[0] ? "SDOJ-Recomp " SDOJ_RELEASE_VERSION : "SDOJ-Recomp development";
    auto service = std::make_shared<SdojCommunity>(endpoint, name, build, !REXCVAR_GET(debug));
    sdoj::xam::SetCommunityService(service);
    service_ = std::move(service);
    REXLOG_INFO("Community leaderboards enabled for SaiDaiOuJou");
  } catch (const std::exception& error) {
    REXLOG_WARN("Community unavailable: {}", error.what());
  }
}

void CommunitySession::Stop(rex::system::KernelState* kernel) {
  if (!service_)
    return;
  sdoj::xam::SetCommunityService({});
  sdoj::xam::CancelCommunityIo(kernel);
  service_->Stop();
  service_.reset();
}
}  // namespace sdoj
