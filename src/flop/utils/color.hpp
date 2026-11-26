#pragma once

#include <cmath>

namespace flop {

// rgb components; the caller defines their color space.
struct Color {
  float r{}, g{}, b{};

  friend constexpr Color operator+(Color a, Color b) { return {a.r + b.r, a.g + b.g, a.b + b.b}; }
  friend constexpr Color operator*(Color color, double factor) {
    return {
        static_cast<float>(color.r * factor),
        static_cast<float>(color.g * factor),
        static_cast<float>(color.b * factor)
    };
  }
};

// convert normalized srgb components into linear rgb components.
inline Color srgb_to_linear(Color color) {
  const auto convert = [](float value) {
    return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
  };
  return {convert(color.r), convert(color.g), convert(color.b)};
}

} // namespace flop
