#pragma once
#include <string>
#include <string_view>

namespace cave {
std::string NormalizeDisplayNickname(std::string_view name);
// The Xbox 360 leaderboard displays at most 15 printable ASCII bytes.
std::string LegacyDisplayNickname(std::string_view canonical_name);
}  // namespace cave
