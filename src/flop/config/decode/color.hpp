#pragma once

#include <charconv>

#include "flop/config/load.hpp"
#include "flop/utils/color.hpp"

namespace flop::config {

// authored hex colors decode to normalized srgb components.
template <> struct ConfigDecoder<Color> {
  [[nodiscard]] static Color decode(const ConfigView &field) {
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
};

} // namespace flop::config
