#pragma once
#include "community/replay_encoding.h"

namespace sdoj {
inline constexpr uint32_t kReplayHeaderBytes = 304, kReplayMaxBytes = 8 * 1024 * 1024;
inline constexpr auto kReplayFormat = "sdoj-disc-native-v1";
struct ReplayInfo {
  int64_t score, timestamp;
  uint32_t character_type, stage, continues, option;
};
ReplayInfo InspectReplay(std::span<const uint8_t> record);
uint64_t NativeFiletime(int64_t packed);
// Returns UINT32_MAX for an unreviewed board. No G2 IDs or team assumptions.
uint32_t ViewCharacterType(uint32_t view);
}  // namespace sdoj
