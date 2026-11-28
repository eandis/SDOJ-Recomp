#include "community/replay_encoding.h"
#include <rex/crypto/sha256.h>
#include <stdexcept>

extern "C" {
#include <libavutil/base64.h>
}

namespace cave::community {
namespace {
void Check(bool ok) {
  if (!ok)
    throw std::runtime_error("Invalid replay encoding");
}
}  // namespace

std::string ReplaySha256(std::span<const uint8_t> record) {
  Check(!record.empty() && record.size() <= kReplayEncodingLimit);
  return rex::crypto::sha256({reinterpret_cast<const char*>(record.data()), record.size()});
}

std::string ReplayBase64(std::span<const uint8_t> record) {
  Check(!record.empty() && record.size() <= kReplayEncodingLimit);
  std::string result(AV_BASE64_SIZE(record.size()), '\0');
  Check(av_base64_encode(result.data(), static_cast<int>(result.size()), record.data(),
                         static_cast<int>(record.size())) != nullptr);
  result.pop_back();  // av_base64_encode includes a trailing NUL.
  return result;
}

std::vector<uint8_t> ReplayUnbase64(const std::string& text) {
  Check(!text.empty() && text.size() <= (kReplayEncodingLimit + 2) / 3 * 4 && text.size() % 4 == 0);
  std::vector<uint8_t> result(text.size() / 4 * 3);
  const int size = av_base64_decode(result.data(), text.c_str(), static_cast<int>(result.size()));
  Check(size > 0 && static_cast<unsigned>(size) <= kReplayEncodingLimit);
  result.resize(size);
  Check(ReplayBase64(result) == text);
  return result;
}
}  // namespace cave::community
