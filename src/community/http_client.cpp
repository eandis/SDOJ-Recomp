#include "community/http_client.h"
#include "common/nickname.h"
#include "community/http_transport.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <rex/logging.h>
#include <rex/string.h>
#include <rex/system/xtypes.h>

namespace cave::community {
using rex::X_RESULT;
namespace {
constexpr unsigned kInvalidData = 13;

bool UnsafeChar(std::string_view text) {
  return std::any_of(text.begin(), text.end(),
                     [](unsigned char c) { return c <= 0x20 || c == 0x7f || c == '\\' || c == '#'; });
}

}  // namespace

bool ValidEndpoint(std::string_view endpoint) {
  if (endpoint.empty() || endpoint.size() > 8192 || UnsafeChar(endpoint) ||
      endpoint.find('?') != endpoint.npos)
    return false;
  const auto scheme_end = endpoint.find("://");
  if (scheme_end == endpoint.npos)
    return false;
  const auto remainder = endpoint.substr(scheme_end + 3);
  const auto authority = remainder.substr(0, remainder.find('/'));
  if (authority.empty() || authority.find('@') != authority.npos)
    return false;
  return http::ValidServer(endpoint);
}

std::string NormalizeNickname(std::string_view name) { return cave::NormalizeDisplayNickname(name); }

bool ValidNickname(std::string_view name) {
  if (name.empty() || name.size() > 48 || name.front() == ' ' || name.back() == ' ')
    return false;
  try {
    const auto wide = rex::string::to_utf16(name);
    if (wide.empty() || wide.size() > 32)
      return false;
    for (char16_t c : wide) {
      if (c < 0x20 || (c >= 0x7f && c <= 0x9f) || (c >= 0x200b && c <= 0x200f) ||
          (c >= 0x202a && c <= 0x202e) || (c >= 0x2060 && c <= 0x206f) || c == 0xfeff)
        return false;
    }
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

HttpClient::HttpClient(std::string endpoint) : endpoint_(std::move(endpoint)) {
  while (!endpoint_.empty() && endpoint_.back() == '/')
    endpoint_.pop_back();
  if (!ValidEndpoint(endpoint_))
    throw std::runtime_error("Invalid community endpoint; use HTTPS or loopback HTTP");
  worker_ = std::thread([this] { Run(); });
}
HttpClient::~HttpClient() { Stop(); }

void HttpClient::Stop() {
  std::lock_guard stop_lock(stop_mutex_);
  stopping_ = true;
  {
    std::lock_guard lock(mutex_);
    while (!jobs_.empty()) {
      jobs_.front().completion.set_value({0, X_ERROR_CANCELLED, {}});
      jobs_.pop_front();
    }
  }
  wake_.notify_all();
  if (worker_.joinable())
    worker_.join();
}

std::future<HttpResult> HttpClient::Request(std::string path, nlohmann::json body) {
  Job job{std::move(path), std::move(body)};
  auto future = job.completion.get_future();
  {
    std::lock_guard lock(mutex_);
    if (stopping_ || jobs_.size() >= 16) {
      job.completion.set_value({0, unsigned(stopping_ ? X_ERROR_CANCELLED : X_ERROR_BUSY), {}});
      return future;
    }
    jobs_.push_back(std::move(job));
  }
  wake_.notify_one();
  return future;
}

void HttpClient::Run() {
  for (;;) {
    Job job;
    {
      std::unique_lock lock(mutex_);
      wake_.wait(lock, [this] { return stopping_ || !jobs_.empty(); });
      if (stopping_)
        return;
      job = std::move(jobs_.front());
      jobs_.pop_front();
    }
    auto started = std::chrono::steady_clock::now();
    HttpResult result;
    try {
      result = Perform(job);
    } catch (...) {
      result.error = kInvalidData;
    }
    const auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started)
            .count();
    // Log only the operation, HTTP status/error and timing. Never nickname,
    // machine identity, payload, full URL or replay data.
    auto operation = job.path.substr(0, job.path.find('?'));
    if (operation.starts_with("/replays/"))
      operation = "/replays/:id";
    REXLOG_INFO("Community request={} status={} error={} ms={}", operation, result.status, result.error,
                duration);
    job.completion.set_value(std::move(result));
  }
}

HttpResult HttpClient::Perform(const Job& job) {
  if (job.path.empty() || job.path.size() > 8192 || job.path.front() != '/' || job.path.starts_with("//") ||
      UnsafeChar(job.path))
    return {0, X_ERROR_INVALID_PARAMETER, {}};
  const std::string url = endpoint_ + job.path;
  std::string payload = job.body.is_null() ? "" : job.body.dump();
  if (payload.size() > http::kMaxBodyBytes)
    return {0, X_ERROR_INSUFFICIENT_BUFFER, {}};

  std::string response;
  auto result = http::Exchange(url, payload, stopping_, response);
  if (stopping_)
    return {result.status, X_ERROR_CANCELLED, {}};
  if (result.error)
    return result;
  result.body = nlohmann::json::parse(response, nullptr, false);
  if (!result.body.is_object())
    return {result.status, kInvalidData, {}};
  return result;
}
}  // namespace cave::community
