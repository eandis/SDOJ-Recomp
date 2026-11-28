#pragma once

#include "community/http_client.h"

namespace cave::community::http {
inline constexpr size_t kMaxBodyBytes = 12 * 1024 * 1024;

bool ValidServer(std::string_view endpoint);
// An empty payload is a GET; every POST carries a serialized JSON value.
HttpResult Exchange(const std::string& url, std::string& payload, const std::atomic<bool>& stopping,
                    std::string& response);
}  // namespace cave::community::http
