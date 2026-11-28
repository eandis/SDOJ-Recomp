#include "community/http_transport.h"

#include <curl/curl.h>
#include <gio/gio.h>
#include <memory>
#include <rex/system/xtypes.h>

namespace cave::community::http {
using rex::X_RESULT;
namespace {
constexpr unsigned kOffline = 1231, kTimeout = 1460;

// The game and its update checker share libcurl for the process lifetime.
// Initialize before GTK startup, update checks, and the game audio/video workers.
const CURLcode curl_init_result = curl_global_init(CURL_GLOBAL_DEFAULT);

size_t Receive(char* data, size_t size, size_t count, void* user) noexcept {
  auto& response = *static_cast<std::string*>(user);
  if (size && count > (kMaxBodyBytes - response.size()) / size)
    return 0;
  try {
    response.append(data, size * count);
    return size * count;
  } catch (...) {
    return 0;
  }
}

int Cancel(void* context, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
  return static_cast<const std::atomic<bool>*>(context)->load() ? 1 : 0;
}
}  // namespace

bool ValidServer(std::string_view endpoint) {
  std::unique_ptr<GSocketConnectable, decltype(&g_object_unref)> address(
      g_network_address_parse_uri(std::string(endpoint).c_str(), 0, nullptr), g_object_unref);
  if (!address)
    return false;
  auto* parsed = G_NETWORK_ADDRESS(address.get());
  const char* scheme = g_network_address_get_scheme(parsed);
  const char* host = g_network_address_get_hostname(parsed);
  if (!scheme || !host || !*host)
    return false;
  return g_ascii_strcasecmp(scheme, "https") == 0 ||
         (g_ascii_strcasecmp(scheme, "http") == 0 &&
          (g_ascii_strcasecmp(host, "localhost") == 0 || std::string_view(host) == "127.0.0.1" ||
           std::string_view(host) == "::1"));
}

HttpResult Exchange(const std::string& url, std::string& payload, const std::atomic<bool>& stopping,
                    std::string& response) {
  if (curl_init_result != CURLE_OK)
    return {0, kOffline, {}};
  std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers(
      curl_slist_append(nullptr, "Content-Type: application/json"), curl_slist_free_all);
  std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> request(curl_easy_init(), curl_easy_cleanup);
  if (!request || !headers)
    return {0, kOffline, {}};
  auto set = [&](CURLoption option, auto value) {
    return curl_easy_setopt(request.get(), option, value) == CURLE_OK;
  };
  if (!set(CURLOPT_URL, url.c_str()) || !set(CURLOPT_USERAGENT, "SDOJ-Recomp/Community") ||
      !set(CURLOPT_FOLLOWLOCATION, 0L) || !set(CURLOPT_NOSIGNAL, 1L) ||
      !set(CURLOPT_CONNECTTIMEOUT_MS, 2000L) || !set(CURLOPT_TIMEOUT_MS, 8000L) ||
      !set(CURLOPT_WRITEFUNCTION, Receive) || !set(CURLOPT_WRITEDATA, &response) ||
      !set(CURLOPT_NOPROGRESS, 0L) || !set(CURLOPT_XFERINFOFUNCTION, Cancel) ||
      !set(CURLOPT_XFERINFODATA, &stopping))
    return {0, kOffline, {}};
  if (!payload.empty() &&
      (!set(CURLOPT_HTTPHEADER, headers.get()) || !set(CURLOPT_POSTFIELDS, payload.c_str()) ||
       !set(CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(payload.size()))))
    return {0, kOffline, {}};

  const auto code = curl_easy_perform(request.get());
  long status = 0;
  curl_easy_getinfo(request.get(), CURLINFO_RESPONSE_CODE, &status);
  const auto http_status = static_cast<unsigned>(status);
  if (stopping)
    return {http_status, X_ERROR_CANCELLED, {}};
  if (code != CURLE_OK)
    return {http_status, code == CURLE_OPERATION_TIMEDOUT ? kTimeout : kOffline, {}};
  return {http_status, 0, {}};
}
}  // namespace cave::community::http
