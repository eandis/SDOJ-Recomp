#include "nickname.h"
#ifdef _WIN32
#include <Windows.h>
#else
#include <glib.h>
#endif
#include <rex/crypto/sha256.h>
#include <memory>
#include <algorithm>
#include <stdexcept>

namespace cave {
std::string NormalizeDisplayNickname(std::string_view name) {
  if (name.empty() || name.size() > 512 || name.find('\0') != name.npos)
    return {};
#ifdef _WIN32
  auto size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.data(), int(name.size()), nullptr, 0);
  if (size <= 0)
    return {};
  std::wstring wide(size, L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.data(), int(name.size()), wide.data(), size) !=
      size)
    return {};
  size = NormalizeString(NormalizationKC, wide.data(), int(wide.size()), nullptr, 0);
  if (size <= 0 || size > 512)
    return {};
  std::wstring normalized(size, L'\0');
  size = NormalizeString(NormalizationKC, wide.data(), int(wide.size()), normalized.data(), size);
  if (size <= 0)
    return {};
  normalized.resize(size);
  auto space = [](wchar_t ch) {
    WORD type = 0;
    return GetStringTypeW(CT_CTYPE1, &ch, 1, &type) && (type & C1_SPACE);
  };
  const auto first = std::find_if_not(normalized.begin(), normalized.end(), space);
  const auto last = std::find_if_not(normalized.rbegin(), normalized.rend(), space).base();
  if (first >= last)
    return {};
  const std::wstring_view trimmed(&*first, size_t(last - first));
  size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, trimmed.data(), int(trimmed.size()), nullptr, 0,
                             nullptr, nullptr);
  if (size <= 0)
    return {};
  std::string result(size, '\0');
  if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, trimmed.data(), int(trimmed.size()), result.data(),
                          size, nullptr, nullptr) != size)
    return {};
  return result;
#else
  if (!g_utf8_validate(name.data(), name.size(), nullptr))
    return {};
  std::unique_ptr<char, decltype(&g_free)> normalized(
      g_utf8_normalize(name.data(), name.size(), G_NORMALIZE_NFKC), g_free);
  if (!normalized)
    return {};
  const char* first = normalized.get();
  while (*first && g_unichar_isspace(g_utf8_get_char(first)))
    first = g_utf8_next_char(first);
  const char* last = first;
  for (const char* p = first; *p; p = g_utf8_next_char(p)) {
    if (!g_unichar_isspace(g_utf8_get_char(p)))
      last = g_utf8_next_char(p);
  }
  return std::string(first, last);
#endif
}
std::string LegacyDisplayNickname(std::string_view name) {
  if (!name.empty() &&
      std::all_of(name.begin(), name.end(), [](unsigned char c) { return c >= 32 && c <= 126; }))
    return std::string(name.substr(0, 15));
  if (name.size() > 512)
    throw std::invalid_argument("Invalid native nickname");
  return "Player-" + rex::crypto::sha256(name).substr(0, 8);
}
}  // namespace cave
