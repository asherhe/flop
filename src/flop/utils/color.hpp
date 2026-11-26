#pragma once

namespace flop {

// rgb components; the caller defines their color space.
struct Color {
  float r{}, g{}, b{};

  friend constexpr Color operator+(Color a, Color b) {
    return {a.r + b.r, a.g + b.g, a.b + b.b};
  }
  friend constexpr Color operator*(Color color, double factor) {
    return {static_cast<float>(color.r * factor), static_cast<float>(color.g * factor),
            static_cast<float>(color.b * factor)};
  }
};

} // namespace flop
