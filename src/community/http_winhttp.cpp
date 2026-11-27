#include "community/http_transport.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>

#include <array>
#include <chrono>

namespace cave::community::http {
namespace {
struct Handle {
  HINTERNET value;
  ~Handle() {
    if (value)
      WinHttpCloseHandle(value);
  }
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  explicit Handle(HINTERNET handle) : value(handle) {}
};

struct Url {
  std::wstring text;
  URL_COMPONENTS parts{};

  bool Parse(std::string_view value) {
    const int size = static_cast<int>(value.size());
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), size, nullptr, 0);
    if (count == 0)
      return false;
    text.resize(count);
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), size, text.data(), count) != count)
      return false;
    parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = DWORD(-1);
    parts.dwUserNameLength = parts.dwPasswordLength = DWORD(-1);
    return WinHttpCrackUrl(text.c_str(), count, 0, &parts) && parts.dwHostNameLength &&
           !parts.dwUserNameLength && !parts.dwPasswordLength;
  }
};
}  // namespace

bool ValidServer(std::string_view endpoint) {
  Url url;
  if (!url.Parse(endpoint))
    return false;
  const auto& parts = url.parts;
  if (parts.nScheme == INTERNET_SCHEME_HTTPS)
    return true;
  const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
  return parts.nScheme == INTERNET_SCHEME_HTTP &&
         (_wcsicmp(host.c_str(), L"localhost") == 0 || host == L"127.0.0.1" || host == L"[::1]" ||
          host == L"::1");
}

HttpResult Exchange(const std::string& address, std::string& payload, const std::atomic<bool>& stopping,
                    std::string& response) {
  Url url;
  if (!url.Parse(address))
    return {0, ERROR_INVALID_PARAMETER, {}};
  const auto& parts = url.parts;
  const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
  std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
  if (parts.dwExtraInfoLength)
    path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
  // As in the upstream update checker, only the worker owns synchronous WinHTTP handles.
  Handle session(WinHttpOpen(L"SDOJ-Recomp/Community", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                             WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
  if (!session.value || !WinHttpSetTimeouts(session.value, 2000, 2000, 3000, 3000))
    return {0, GetLastError(), {}};
  Handle connection(WinHttpConnect(session.value, host.c_str(), parts.nPort, 0));
  if (!connection.value)
    return {0, GetLastError(), {}};
  Handle request(WinHttpOpenRequest(connection.value, payload.empty() ? L"GET" : L"POST", path.c_str(),
                                    nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                    parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0));
  if (!request.value)
    return {0, GetLastError(), {}};
  DWORD disable = WINHTTP_DISABLE_REDIRECTS | WINHTTP_DISABLE_AUTHENTICATION | WINHTTP_DISABLE_COOKIES;
  if (!WinHttpSetOption(request.value, WINHTTP_OPTION_DISABLE_FEATURE, &disable, sizeof(disable)))
    return {0, GetLastError(), {}};

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
  const auto size = static_cast<DWORD>(payload.size());
  if (stopping)
    return {0, ERROR_CANCELLED, {}};
  if (!WinHttpSendRequest(
          request.value, size ? L"Content-Type: application/json" : WINHTTP_NO_ADDITIONAL_HEADERS,
          size ? DWORD(-1) : 0, size ? payload.data() : WINHTTP_NO_REQUEST_DATA, size, size, 0))
    return {0, GetLastError(), {}};
  if (stopping)
    return {0, ERROR_CANCELLED, {}};
  if (!WinHttpReceiveResponse(request.value, nullptr))
    return {0, GetLastError(), {}};
  DWORD status = 0, length = sizeof(status);
  if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                           WINHTTP_HEADER_NAME_BY_INDEX, &status, &length, WINHTTP_NO_HEADER_INDEX))
    return {0, GetLastError(), {}};
  std::array<char, 16384> chunk{};
  while (!stopping) {
    if (std::chrono::steady_clock::now() >= deadline)
      return {status, ERROR_TIMEOUT, {}};
    DWORD read = 0;
    if (!WinHttpReadData(request.value, chunk.data(), static_cast<DWORD>(chunk.size()), &read))
      return {status, GetLastError(), {}};
    if (read == 0)
      return {status, 0, {}};
    if (read > kMaxBodyBytes - response.size())
      return {status, ERROR_INSUFFICIENT_BUFFER, {}};
    response.append(chunk.data(), read);
  }
  return {status, ERROR_CANCELLED, {}};
}
}  // namespace cave::community::http
