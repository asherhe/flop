#pragma once

#include <limits>

#include "flop/world/universe.hpp"

namespace flop::app {
inline constexpr double POWERED_WARP_CEILING = 20.0;
[[nodiscard]] inline double warp_ceiling(const world::Universe &universe) noexcept {
  return universe.any_craft_powered() ? POWERED_WARP_CEILING
                                      : std::numeric_limits<double>::infinity();
}
} // namespace flop::app
