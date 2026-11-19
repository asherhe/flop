#pragma once

#include <cmath>
#include <numbers>

namespace flop {

[[nodiscard]] inline bool positive_finite(double value) {
  return std::isfinite(value) && value > 0;
}

// wrap an angle into [0, 2*pi).
[[nodiscard]] inline double wrap_angle(double angle_radians) {
  constexpr double TWO_PI = 2.0 * std::numbers::pi;
  double wrapped = std::fmod(angle_radians, TWO_PI);
  if (wrapped < 0.0) {
    wrapped += TWO_PI;
  }
  return wrapped;
}


} // namespace flop
