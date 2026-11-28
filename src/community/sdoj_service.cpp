#include "community/sdoj_service.h"
#include "community/replay_format.h"
#include <algorithm>
#include <charconv>
#include <map>
#include <set>
#include <stdexcept>

namespace sdoj {
using namespace sdoj::xam;
using Json = nlohmann::json;
namespace {
void Check(bool ok) {
  if (!ok)
    throw std::runtime_error("Invalid SDOJ community data");
}
// Xbox stat properties in POST /scores columns. Type 2 uses signed decimal strings
// to preserve all 64 bits in JSON. Type 1 uses JSON integers.
constexpr std::pair<uint32_t, uint32_t> kTypes[] = {
    {0x20000004, 2},  // Points, also sent as the top-level score.
    {0x20000002, 2},  // Packed Xbox FILETIME, with the original word order.
    {0x10000007, 1},  // Character * 16 + shot type.
    {0x10000006, 1},  // Stage, native replay offset 56.
    {0x10000005, 1},  // Replay offset 60, returned to the ranking screen in column 5.
    {0x10000001, 1},  // Replay offset 76, returned to the ranking screen in column 6.
    {0x10008011, 1}   // Optional recording size in bytes, or zero before upload.
};
uint32_t Type(uint32_t id) {
  for (const auto& [key, type] : kTypes)
    if (id == key)
      return type;
  return 0;
}
int64_t Number(const Json& value, uint32_t type) {
  if (type == 1) {
    Check(value.is_number_integer());
    return value.get<int64_t>();
  }
  Check(value.is_string());
  const auto& text = value.get_ref<const std::string&>();
  int64_t result = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
  Check(!text.empty() && text.size() <= 20 && parsed.ec == std::errc{} &&
        parsed.ptr == text.data() + text.size());
  return result;
}
std::map<uint32_t, int64_t> Properties(const Json& properties) {
  Check(properties.is_array() && properties.size() >= 6 && properties.size() <= 7);
  std::map<uint32_t, int64_t> result;
  for (const auto& item : properties) {
    Check(item.at("id").is_number_integer() && item.at("type").is_number_integer());
    const auto id64 = item.at("id").get<int64_t>();
    Check(id64 >= 0 && id64 <= UINT32_MAX);
    const auto id = uint32_t(id64), type = Type(id);
    Check(type && item.at("type") == type);
    auto value = Number(item.at("value"), type);
    Check(result.emplace(id, value).second);
    if (type == 1)
      Check(value >= 0 && value <= INT32_MAX);
  }
  for (const auto& [id, type] : kTypes)
    if (id != 0x10008011)
      Check(result.contains(id));
  Check(result.at(0x20000004) >= 0);
  const auto packed = result.at(0x10000007);
  Check(packed <= 0x32 && (packed & 15) <= 2);
  for (auto id : {0x10000001, 0x10000005, 0x10000006})
    Check(result.at(id) <= 0xffff);
  NativeFiletime(result.at(0x20000002));
  if (result.contains(0x10008011)) {
    const auto size = result.at(0x10008011);
    Check(!size || (size > kReplayHeaderBytes && size <= kReplayMaxBytes));
  }
  return result;
}
bool View(uint32_t view) { return ViewCharacterType(view) != UINT32_MAX; }
uint32_t ColumnType(uint16_t id) { return id == 1 ? 2 : 1; }
std::string RunKey(const Json& properties) {
  const auto p = Properties(properties);
  return std::to_string(p.at(0x20000004)) + "/" + std::to_string(p.at(0x20000002)) + "/" +
         std::to_string(p.at(0x10000007));
}
void MatchReplay(const Json& properties, std::span<const uint8_t> record) {
  const auto p = Properties(properties);
  const auto info = InspectReplay(record);
  Check(p.at(0x20000004) == info.score && p.at(0x20000002) == info.timestamp &&
        p.at(0x10000007) == info.character_type && p.at(0x10000006) == info.stage &&
        p.at(0x10000005) == info.continues && p.at(0x10000001) == info.option);
  if (p.contains(0x10008011))
    Check(!p.at(0x10008011) || p.at(0x10008011) == int64_t(record.size()));
}
cave::community::NativeSchema Schema() {
  return {.game = "sdoj",
          .replay_namespace = "sdoj-replay",
          .replay_format = kReplayFormat,
          .replay_header = kReplayHeaderBytes,
          .replay_max = kReplayMaxBytes,
          .valid_view = View,
          .encode_score = EncodeCommunityScore,
          .decode_columns = [](uint32_t, const Json& p,
                               uint32_t bytes) { return DecodeCommunityColumns(p, bytes); },
          .column_type = ColumnType,
          .native_name = cave::community::NativeLegacyName,
          .run_key = RunKey,
          .match_replay = [](uint32_t, const Json& p,
                             std::span<const uint8_t> record) { MatchReplay(p, record); },
          .storage_size_property = 0x10008011};
}
}  // namespace
Json EncodeCommunityScore(const CommunityWrite& view) {
  Check(View(view.view));
  Json columns = Json::array();
  for (const auto& col : view.columns) {
    Check(Type(col.id) == col.type && col.type);
    Json value = col.type == 2 ? Json(std::to_string(col.integer)) : Json(col.integer);
    columns.push_back({{"id", col.id}, {"type", col.type}, {"value", std::move(value)}});
  }
  const auto props = Properties(columns);
  Check(props.at(0x10000007) == ViewCharacterType(view.view));
  return {{"view", view.view},
          {"rules", "standard"},
          {"score", std::to_string(props.at(0x20000004))},
          {"columns", std::move(columns)}};
}
std::vector<CommunityColumn> DecodeCommunityColumns(const Json& properties, uint32_t replay_bytes) {
  const auto props = Properties(properties);
  std::vector<CommunityColumn> result;
  Check(!replay_bytes || (replay_bytes > kReplayHeaderBytes && replay_bytes <= kReplayMaxBytes));
  // Writer properties and reader column IDs are different namespaces.
  // The native reader at 8212CB40..8212CBCC restores replay-header offsets
  // +52/+56/+60/+76 from columns 2/4/5/6, and +32 from column 1.
  // Score is row.rating. Returning the packed FILETIME as column 2 makes
  // its high word a character index and crashes the replay-preview renderer.
  result.push_back({1, 2, props.at(0x20000002), 0});
  result.push_back({2, 1, props.at(0x10000007), 0});
  result.push_back({4, 1, props.at(0x10000006), 0});
  result.push_back({5, 1, props.at(0x10000005), 0});
  result.push_back({6, 1, props.at(0x10000001), 0});
  result.push_back({0xfffa, 1, replay_bytes, 0});
  return result;
}
SdojCommunity::SdojCommunity(std::string endpoint, std::string nickname, std::string build, bool ranked)
    : NativeService(Schema(), std::move(endpoint), std::move(nickname), std::move(build)), ranked_(ranked) {}

bool SdojCommunity::AllowsPrivilege(uint32_t privilege) const {
  constexpr uint32_t kUserCreatedContent = 247;
  constexpr uint32_t kCommunications = 252;
  return privilege == kUserCreatedContent || privilege == kCommunications;
}

void SdojCommunity::Write(std::vector<CommunityWrite> views, WriteCompletion complete) {
  if (!ranked_) {
    complete(5);
    return;
  }
  NativeService::Write(std::move(views), std::move(complete));
}

void SdojCommunity::Replay(CommunityReplay request, ReplayCompletion complete) {
  if (!ranked_ && request.upload) {
    complete({5, {}});
    return;
  }
  NativeService::Replay(std::move(request), std::move(complete));
}
}  // namespace sdoj
