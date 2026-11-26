#pragma once

#include <charconv>
#include <cmath>

#include "flop/utils/color.hpp"
#include "flop/config/load.hpp"

namespace flop::config {

inline Color decode_srgb_color(const ConfigView &field) {
  const auto value = field.nonempty_string();
  if (value.size() != 7 || value[0] != '#')
    field.fail(field.mark(), "Expected color in #rrggbb format");
  float channels[3]{};
  for (int i = 0; i < 3; ++i) {
    unsigned channel = 0;
    const char *begin = value.data() + 1 + 2 * i;
    const auto parsed = std::from_chars(begin, begin + 2, channel, 16);
    if (parsed.ec != std::errc{} || parsed.ptr != begin + 2)
      field.fail(field.mark(), "Expected color in #rrggbb format");
    channels[i] = static_cast<float>(channel) / 255.0f;
  }
  return {channels[0], channels[1], channels[2]};
}

inline Color decode_color(const ConfigView &field) {
  const auto srgb = decode_srgb_color(field);
  const auto linear = [](double component) {
    const float value = static_cast<float>(component);
    return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
  };
  return {linear(srgb.r), linear(srgb.g), linear(srgb.b)};
}

} // namespace flop::config
