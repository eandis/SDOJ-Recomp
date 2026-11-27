// CommunityService worker pattern adapted from Mushihimesama-Futari-PC
// 7dced5979076329db71b1296c81944c83fb72b48, community_runtime.cpp.
// Shared score-before-replay transport; native schemas are supplied by adapters.
#include "community/native_service.h"
#include "community/replay_encoding.h"
#include "common/nickname.h"
#include <rex/logging.h>
#include <rex/system/xtypes.h>
#include <random>
#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
#include <set>
#include <stdexcept>

namespace cave::community {
using rex::X_RESULT;
using namespace sdoj::xam;
using Json = nlohmann::json;
namespace {
constexpr uint32_t kCancelled = 995, kInvalid = 13, kOffline = 1231, kTimeout = 1460;
void Require(bool value) {
  if (!value)
    throw std::runtime_error("Invalid native community data");
}
int64_t Decimal(const Json& value) {
  Require(value.is_string());
  const auto& text = value.get_ref<const std::string&>();
  int64_t result{};
  auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
  Require(!text.empty() && text.size() <= 20 && parsed.ec == std::errc{} &&
          parsed.ptr == text.data() + text.size());
  return result;
}
uint32_t Unsigned(const Json& value, uint32_t cap = UINT32_MAX) {
  Require(value.is_number_integer());
  auto result = value.get<int64_t>();
  Require(result >= 0 && uint64_t(result) <= cap);
  return uint32_t(result);
}
std::string Nickname(std::string_view value) {
  auto result = cave::community::NormalizeNickname(value);
  Require(cave::community::ValidNickname(result));
  return result;
}
std::string Build(std::string value) {
  Require(!value.empty() && value.size() <= 96);
  return value;
}
std::string GameEndpoint(std::string endpoint, std::string_view game) {
  // All routes below are relative to the F4 server's /games/sdoj root.
  // The default root is https://xiaolu.life/cave/api/v1/games/sdoj.
  Require(cave::community::ValidEndpoint(endpoint));
  while (endpoint.ends_with('/'))
    endpoint.pop_back();
  const auto route = "/games/" + std::string(game);
  return endpoint.ends_with(route) ? endpoint : endpoint + route;
}
std::string Encode(std::string_view value) {
  constexpr char hex[] = "0123456789ABCDEF";
  std::string out;
  for (unsigned char c : value) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
        c == '.' || c == '~')
      out += char(c);
    else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 15];
    }
  }
  return out;
}
std::string Hex(std::span<const unsigned char> bytes) {
  constexpr char digits[] = "0123456789abcdef";
  std::string result;
  for (auto byte : bytes) {
    result += digits[byte >> 4];
    result += digits[byte & 15];
  }
  return result;
}
std::string Reference() {
  unsigned char bytes[16];
  std::random_device random;
  std::uniform_int_distribution<unsigned> octet(0, 255);
  for (auto& byte : bytes)
    byte = static_cast<unsigned char>(octet(random));
  return Hex(bytes);
}
bool ValidReference(const Json& value) {
  if (!value.is_string())
    return false;
  const auto& text = value.get_ref<const std::string&>();
  return text.size() == 32 && text.find_first_not_of("0123456789abcdef") == text.npos;
}
bool ValidHash(std::string_view value) {
  return value.size() == 64 && value.find_first_not_of("0123456789abcdef") == value.npos;
}
uint32_t Error(const cave::community::HttpResult& result) {
  if (result)
    return 0;
  if (result.error == X_ERROR_CANCELLED || result.error == kCancelled)
    return kCancelled;
  return result.error ? result.error : result.status ? kInvalid : kOffline;
}
NativeSchema ValidateSchema(NativeSchema schema) {
  Require(!schema.game.empty() && schema.game.size() <= 32 &&
          schema.game.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-") == schema.game.npos &&
          !schema.replay_namespace.empty() && !schema.replay_format.empty() && schema.replay_header > 0 &&
          schema.replay_max > schema.replay_header && schema.replay_max <= kReplayEncodingLimit &&
          schema.valid_view && schema.encode_score && schema.decode_columns && schema.column_type &&
          schema.native_name && schema.run_key && schema.match_replay);
  return schema;
}
}  // namespace

std::string NativeLegacyName(std::string_view name) { return cave::LegacyDisplayNickname(name); }
NativeService::NativeService(NativeSchema schema, std::string endpoint, std::string nickname,
                             std::string build)
    : schema_(ValidateSchema(schema)),
      nickname_(Nickname(nickname)),
      build_(Build(std::move(build))),
      client_(GameEndpoint(std::move(endpoint), schema_.game)) {
  native_names_[kNativeCommunityLocalXuid] = nickname_;
  worker_ = std::thread([this] { Run(); });
}
NativeService::~NativeService() { Stop(); }
void NativeService::Stop() {
  std::lock_guard stop(stop_mutex_);
  stopping_ = true;
  std::deque<Task> abandoned;
  {
    std::lock_guard lock(mutex_);
    abandoned.swap(tasks_);
  }
  client_.Stop();
  wake_.notify_all();
  for (auto& task : abandoned)
    task.fail(kCancelled);
  if (worker_.joinable())
    worker_.join();
}
void NativeService::Enqueue(Task task) {
  uint32_t error = 0;
  {
    std::lock_guard lock(mutex_);
    if (stopping_)
      error = kCancelled;
    else if (tasks_.size() >= 8)
      error = X_ERROR_BUSY;
    else
      tasks_.push_back(std::move(task));
  }
  if (error)
    task.fail(error);
  else
    wake_.notify_one();
}
void NativeService::Run() {
  for (;;) {
    Task task;
    {
      std::unique_lock lock(mutex_);
      wake_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
      if (stopping_)
        return;
      task = std::move(tasks_.front());
      tasks_.pop_front();
    }
    if (Clock::now() >= task.deadline) {
      task.fail(kTimeout);
      continue;
    }
    try {
      task.work(task.deadline);
    } catch (const std::exception&) {
      REXLOG_WARN("Native community request rejected: invalid native or server data");
      task.fail(kInvalid);
    }
  }
}
cave::community::HttpResult NativeService::Http(std::string path, Json body, Clock::time_point deadline) {
  if (stopping_)
    return {0, kCancelled, {}};
  auto request = client_.Request(std::move(path), std::move(body));
  if (request.wait_until(deadline) != std::future_status::ready)
    return {0, kTimeout, {}};
  auto result = request.get();
  if (result && result.body.contains("protocol") && result.body.at("protocol") != 1)
    return {result.status, kInvalid, {}};
  return result;
}
std::string NativeService::DisplayName(uint64_t xuid) const {
  std::lock_guard lock(names_mutex_);
  auto found = native_names_.find(xuid);
  return found == native_names_.end() ? std::string() : found->second;
}
uint64_t NativeService::NativeToken(const std::string& nickname) {
  std::lock_guard lock(names_mutex_);
  for (const auto& [token, name] : native_names_)
    if (name == nickname)
      return token;
  Require(native_names_.size() < 4096);
  const auto token = next_token_++;
  native_names_[token] = nickname;
  return token;
}
void NativeService::Read(CommunityRead request, ReadCompletion completion) {
  Enqueue({Clock::now() + std::chrono::seconds(10),
           [this, request = std::move(request), completion](auto deadline) {
             completion(ReadNow(request, deadline));
           },
           [completion](uint32_t error) { completion({error, {}}); }});
}
CommunityResult NativeService::ReadNow(const CommunityRead& request, Clock::time_point deadline) {
  Require(!request.specs.empty() && request.specs.size() <= 16 && request.count >= 1 &&
          request.count <= 100 && request.xuids.size() <= 100);
  CommunityResult result;
  for (const auto& spec : request.specs) {
    Require(schema_.valid_view(spec.view) && spec.columns.size() <= 64 && request.type <= 1);
    // Browse rankings: GET /leaderboard?view=<board>&rules=standard&count=<page size>
    // Add one selector: start=<1-based rank>, around=<nickname>, or name=<nickname>
    // repeated for each requested player. Xbox user IDs are resolved to names locally.
    // Example: view=23&rules=standard&count=10&start=1 reads the first ten Xbox 360 Shot ranks.
    auto path = "/leaderboard?view=" + std::to_string(spec.view) +
                "&rules=standard&count=" + std::to_string(request.count);
    if (!request.xuids.empty()) {
      size_t known = 0;
      for (auto xuid : request.xuids) {
        auto name = DisplayName(xuid);
        if (!name.empty()) {
          path += "&name=" + Encode(name);
          ++known;
        }
      }
      if (!known) {
        result.views.push_back({spec.view, 0, {}});
        continue;
      }
    } else if (request.type == 0) {
      auto name = DisplayName(request.pivot);
      if (name.empty()) {
        result.views.push_back({spec.view, 0, {}});
        continue;
      }
      path += "&around=" + Encode(name);
    } else
      path += "&start=" + std::to_string(std::clamp(request.pivot, uint64_t(1), uint64_t(1000000)));
    auto reply = Http(path, nullptr, deadline);
    if (auto error = Error(reply))
      return {error, {}};
    Require(Unsigned(reply.body.at("view")) == spec.view);
    // The menu uses total and each row's nickname, rank, decimal score and native columns.
    // A row with a replay_id also supplies its format, size and SHA-256 for download.
    CommunityView view{spec.view, Unsigned(reply.body.at("total"), 100000), {}};
    const auto& rows = reply.body.at("rows");
    Require(rows.is_array() && rows.size() <= (request.xuids.empty() ? request.count : request.xuids.size()));
    for (const auto& entry : rows) {
      Require(ValidReference(entry.at("id")));
      auto name = entry.at("nickname").get<std::string>();
      Require(cave::community::ValidNickname(name) && name == Nickname(name));
      CommunityRow row;
      row.nickname = name;
      row.native_tag = schema_.native_name(name);
      row.xuid = NativeToken(name);
      row.rank = Unsigned(entry.at("rank"), view.total);
      Require(row.rank > 0);
      row.rating = Decimal(entry.at("score"));
      Require(row.rating >= 0);
      ScoreReference reference;
      reference.id = entry.at("id").get<std::string>();
      reference.properties = entry.at("columns");
      if (!entry.at("replay_id").is_null()) {
        Require(ValidReference(entry.at("replay_id")) &&
                entry.at("replay_format") == std::string(schema_.replay_format));
        reference.replay = entry.at("replay_id").get<std::string>();
        reference.hash = entry.at("replay_sha256").get<std::string>();
        reference.bytes = Unsigned(entry.at("replay_size"), schema_.replay_max);
        Require(ValidHash(reference.hash) && reference.bytes > schema_.replay_header);
      }
      row.columns = schema_.decode_columns(spec.view, reference.properties, reference.bytes);
      downloads_[{spec.view, row.xuid}] = std::move(reference);
      view.rows.push_back(std::move(row));
    }
    // Explicit-user queries need an unranked local row, not a public score.
    for (auto xuid : request.xuids) {
      auto name = DisplayName(xuid);
      if (name.empty() || std::any_of(view.rows.begin(), view.rows.end(),
                                      [xuid](const auto& row) { return row.xuid == xuid; }))
        continue;
      CommunityRow unranked{xuid, 0, 0, name, {}};
      unranked.native_tag = schema_.native_name(name);
      for (auto id : spec.columns)
        unranked.columns.push_back({id, schema_.column_type(id), 0, 0});
      view.rows.push_back(std::move(unranked));
    }
    REXLOG_INFO("Native community native read: view={} rows={} total={}", spec.view, view.rows.size(),
                view.total);
    result.views.push_back(std::move(view));
  }
  return result;
}
void NativeService::Write(std::vector<CommunityWrite> views, WriteCompletion completion) {
  Enqueue(
      {Clock::now() + std::chrono::seconds(10),
       [this, views = std::move(views), completion](auto deadline) { completion(WriteNow(views, deadline)); },
       [completion](uint32_t error) { completion(error); }});
}
uint32_t NativeService::WriteNow(const std::vector<CommunityWrite>& views, Clock::time_point deadline) try {
  Require(!views.empty() && views.size() <= 16);
  if (schema_.storage_size_property && std::all_of(views.begin(), views.end(), [this](const auto& view) {
        return view.columns.size() == 1 && view.columns[0].id == schema_.storage_size_property;
      })) {
    std::set<uint32_t> ids;
    for (const auto& view : views) {
      auto found = uploads_.find(view.view);
      const auto& column = view.columns[0];
      Require(schema_.valid_view(view.view) && ids.insert(view.view).second && column.type == 1 &&
              found != uploads_.end() && !found->second.replay.empty() &&
              column.integer == found->second.bytes);
    }
    // The game's final size-only write acknowledges the replay receipt locally.
    return 0;
  }
  std::vector<Json> submissions;
  std::set<uint32_t> ids;
  // Submit a run: POST /scores, one JSON object per board. EncodeCommunityScore supplies
  // view (board ID), rules ("standard"), score (decimal string) and columns [{id,type,value}].
  // The envelope adds protocol=1, the F4 nickname, the client build and a submission ID.
  for (const auto& view : views) {
    uploads_.erase(view.view);
    Require(ids.insert(view.view).second);
    auto body = schema_.encode_score(view);
    body.update({{"protocol", 1}, {"nickname", nickname_}, {"build", build_}});
    submissions.push_back(std::move(body));
  }
  const auto run = schema_.run_key(submissions.front().at("columns"));
  for (const auto& body : submissions)
    Require(schema_.run_key(body.at("columns")) == run);
  if (run.empty() || run != upload_run_) {
    uploads_.clear();
    submission_ids_.clear();
  }
  upload_run_ = run;
  for (auto& body : submissions) {
    const auto view = body.at("view").get<uint32_t>();
    // Reuse this run and board's 32-digit hex submission ID on retry. The response id
    // identifies the accepted score and becomes score_id when attaching its replay.
    auto& submission = submission_ids_[view];
    if (submission.empty())
      submission = Reference();
    body["submission"] = submission;
    auto reply = Http("/scores", body, deadline);
    if (auto error = Error(reply)) {
      uploads_.clear();
      return error;
    }
    Require(ValidReference(reply.body.at("id")));
    if (!run.empty())
      uploads_[view] = {.id = reply.body.at("id").get<std::string>(), .properties = body.at("columns")};
    REXLOG_INFO("Native community native score accepted: view={}", view);
  }
  return 0;
} catch (...) {
  uploads_.clear();
  throw;
}
void NativeService::Estimate(std::vector<CommunityRank> request, RankCompletion completion) {
  Enqueue({Clock::now() + std::chrono::seconds(10),
           [this, request = std::move(request), completion](auto deadline) {
             completion(EstimateNow(request, deadline));
           },
           [completion](uint32_t error) { completion({error, {}}); }});
}
CommunityRankResult NativeService::EstimateNow(const std::vector<CommunityRank>& request,
                                               Clock::time_point deadline) {
  Require(!request.empty() && request.size() <= 16);
  CommunityRankResult result;
  for (const auto& entry : request) {
    Require(schema_.valid_view(entry.view) && entry.rating >= 0);
    // Preview a score's placement: GET /estimate?view=<board>&rules=standard&score=<points>&name=<nickname>
    // The server excludes this player's previous best and returns the 1-based rank for this view.
    auto reply = Http("/estimate?view=" + std::to_string(entry.view) + "&rules=standard&score=" +
                          std::to_string(entry.rating) + "&name=" + Encode(nickname_),
                      nullptr, deadline);
    if (auto error = Error(reply))
      return {error, {}};
    Require(Unsigned(reply.body.at("view")) == entry.view);
    const auto rank = Unsigned(reply.body.at("rank"), 100001);
    Require(rank > 0);
    result.ranks.push_back(rank);
  }
  return result;
}
void NativeService::Replay(CommunityReplay request, ReplayCompletion completion) {
  Enqueue({Clock::now() + std::chrono::seconds(10),
           [this, request = std::move(request), completion](auto deadline) {
             completion(ReplayNow(request, deadline));
           },
           [completion](uint32_t error) { completion({error, {}}); }});
}
CommunityReplayResult NativeService::ReplayNow(const CommunityReplay& request, Clock::time_point deadline) {
  Require(schema_.valid_view(request.view) && request.capacity > schema_.replay_header &&
          request.capacity <= schema_.replay_max);
  if (request.upload) {
    Require(request.xuid == kNativeCommunityLocalXuid && request.record.size() == request.capacity);
    auto found = uploads_.find(request.view);
    Require(found != uploads_.end());
    const auto& reference = found->second;
    REXLOG_INFO("Native replay upload: view={} bytes={}", request.view, request.record.size());
    schema_.match_replay(request.view, reference.properties, request.record);
    const auto hash = ReplaySha256(request.record);
    // Attach a recording: POST /replays as JSON. score_id is the id from POST /scores for this board.
    // Send the same nickname, protocol=1, format="sdoj-disc-native-v1", and the original recording
    // as base64 data. sha256 is the hex digest of the raw bytes. The receipt returns the replay id.
    auto reply = Http("/replays",
                      {{"protocol", 1},
                       {"score_id", reference.id},
                       {"nickname", nickname_},
                       {"format", std::string(schema_.replay_format)},
                       {"sha256", hash},
                       {"data", ReplayBase64(request.record)}},
                      deadline);
    if (auto error = Error(reply))
      return {error, {}};
    Require(ValidReference(reply.body.at("id")) && reply.body.at("score_id") == reference.id &&
            reply.body.at("sha256") == hash && Unsigned(reply.body.at("bytes")) == request.record.size());
    found->second.replay = reply.body.at("id").get<std::string>();
    found->second.hash = hash;
    found->second.bytes = uint32_t(request.record.size());
    REXLOG_INFO("Native replay accepted: view={} bytes={} sha256={}", request.view, request.record.size(),
                hash);
    return {};
  }
  Require(request.record.empty());
  auto found = downloads_.find({request.view, request.xuid});
  if (found == downloads_.end() || found->second.replay.empty())
    return {X_ERROR_FILE_NOT_FOUND, {}};
  const auto& reference = found->second;
  if (reference.bytes > request.capacity)
    return {X_ERROR_INSUFFICIENT_BUFFER, {}};
  // Play a ranked row's recording: GET /replays/{replay_id}, using the id from GET /leaderboard.
  // The JSON reply contains id, format, bytes (decoded size), sha256 and data (base64).
  // Check those against the selected row before handing the decoded recording to the game.
  auto reply = Http("/replays/" + reference.replay, nullptr, deadline);
  if (auto error = Error(reply))
    return {error, {}};
  Require(reply.body.at("id") == reference.replay &&
          reply.body.at("format") == std::string(schema_.replay_format) &&
          reply.body.at("sha256") == reference.hash && Unsigned(reply.body.at("bytes")) == reference.bytes);
  auto record = ReplayUnbase64(reply.body.at("data").get<std::string>());
  Require(record.size() == reference.bytes && ReplaySha256(record) == reference.hash);
  schema_.match_replay(request.view, reference.properties, record);
  REXLOG_INFO("Native replay downloaded: view={} bytes={} source_sha256={}", request.view, record.size(),
              reference.hash);
  return {0, std::move(record)};
}
}  // namespace cave::community
