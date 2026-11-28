#include "community/replay_format.h"
#include <array>
#include <bit>
#include <stdexcept>

namespace sdoj {
namespace {
void Check(bool ok) {
  if (!ok)
    throw std::runtime_error("Invalid SDOJ replay");
}
uint32_t Be32(std::span<const uint8_t> data, size_t at) {
  return uint32_t(data[at]) << 24 | uint32_t(data[at + 1]) << 16 | uint32_t(data[at + 2]) << 8 | data[at + 3];
}
uint64_t Be64(std::span<const uint8_t> data, size_t at) {
  return uint64_t(Be32(data, at)) << 32 | Be32(data, at + 4);
}
}  // namespace
uint32_t ViewCharacterType(uint32_t view) {
  constexpr std::array<std::array<uint32_t, 3>, 4> arcade{
      {{2, 7, 11}, {3, 9, 12}, {4, 10, 13}, {85, 86, 87}}},
      novice{{{14, 17, 20}, {15, 18, 21}, {16, 19, 22}, {88, 89, 90}}},
      ver15{{{35, 38, 41}, {36, 39, 42}, {37, 40, 43}, {91, 92, 93}}};
  if (view >= 23 && view <= 25)
    return 48 + view - 23;
  for (const auto* mode : {&arcade, &novice, &ver15})
    for (uint32_t character = 0; character < 4; ++character)
      for (uint32_t dress = 0; dress < 3; ++dress)
        if ((*mode)[character][dress] == view)
          return (character << 4) | dress;
  return UINT32_MAX;
}
uint64_t NativeFiletime(int64_t packed) {
  const auto bits = std::bit_cast<uint64_t>(packed), value = (bits << 32) | (bits >> 32);
  Check(value > 0 && value <= INT64_MAX);
  return value;
}
ReplayInfo InspectReplay(std::span<const uint8_t> data) {
  Check(data.size() > kReplayHeaderBytes && data.size() <= kReplayMaxBytes && data.size() % 2 == 0);
  Check(Be32(data, 0) == 0x01304250 && Be32(data, 92) == data.size());
  uint32_t checksum = 0;
  for (size_t at = 0; at < data.size(); at += 2)
    if (at != 8)
      checksum += (uint32_t(data[at]) << 8 | data[at + 1]) * uint32_t(at / 2 + 1);
  Check((checksum & 0xffff) == (uint32_t(data[8]) << 8 | data[9]));
  ReplayInfo info{std::bit_cast<int64_t>(Be64(data, 24)),
                  std::bit_cast<int64_t>(Be64(data, 32)),
                  Be32(data, 52),
                  Be32(data, 56),
                  Be32(data, 60),
                  Be32(data, 76)};
  Check(info.score >= 0 && (info.character_type >> 4) <= 3 && (info.character_type & 15) <= 2 &&
        info.stage <= 0xffff && info.continues <= 0xffff && info.option <= 0xffff);
  NativeFiletime(info.timestamp);
  return info;
}
}  // namespace sdoj
