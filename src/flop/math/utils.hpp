#pragma once

#include <cmath>

namespace flop {

[[nodiscard]] inline bool positive_finite(double value) {
  return std::isfinite(value) && value > 0;
}

} // namespace flop
