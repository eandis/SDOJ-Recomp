#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <nlohmann/json.hpp>

namespace cave::community {
struct HttpResult {
  unsigned status = 0, error = 0;
  nlohmann::json body;
  explicit operator bool() const { return status == 200 && error == 0; }
};

// One bounded background worker. Construction validates without network IO;
// Request only enqueues, and Stop cancels active IO and all pending futures.
class HttpClient final {
 public:
  explicit HttpClient(std::string endpoint);
  ~HttpClient();
  std::future<HttpResult> Request(std::string path, nlohmann::json body = nullptr);
  void Stop();

 private:
  struct Job {
    std::string path;
    nlohmann::json body;
    std::promise<HttpResult> completion;
  };
  void Run();
  HttpResult Perform(const Job& job);
  std::string endpoint_;
  std::mutex mutex_, stop_mutex_;
  std::condition_variable wake_;
  std::deque<Job> jobs_;
  std::thread worker_;
  std::atomic<bool> stopping_{false};
};

bool ValidEndpoint(std::string_view endpoint);
std::string NormalizeNickname(std::string_view name);
bool ValidNickname(std::string_view name);
}  // namespace cave::community
