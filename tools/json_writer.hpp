#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace flop::tools::json {

// Reflection names with control characters are not supported by the format.
inline std::string quote(std::string_view value) {
  std::string result = "\"";
  for (const unsigned char c : value) {
    if (c < 32)
      throw std::runtime_error("control character in reflected name");
    if (c == '\\' || c == '"')
      result += '\\';
    result += static_cast<char>(c);
  }
  return result + '"';
}

inline std::string name(std::string_view value) {
  if (value.empty())
    throw std::runtime_error("reflection name missing");
  return quote(value);
}

inline std::string name(const char *value) {
  if (!value)
    throw std::runtime_error("reflection name missing");
  return name(std::string_view(value));
}

} // namespace flop::tools::json
