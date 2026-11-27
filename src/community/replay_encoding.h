#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace cave::community {
inline constexpr uint32_t kReplayEncodingLimit = 8 * 1024 * 1024;
std::string ReplaySha256(std::span<const uint8_t> record);
std::string ReplayBase64(std::span<const uint8_t> record);
std::vector<uint8_t> ReplayUnbase64(const std::string& text);
}  // namespace cave::community
