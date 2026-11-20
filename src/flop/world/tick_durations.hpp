#pragma once

namespace flop::world {

// the two time steps in one fixed simulation step
struct TickDurations {
  // real wall time
  double control_dt_sec{};
  // in-universe step time (affected by time warp)
  double simulation_dt_sec{};

  friend constexpr bool operator==(const TickDurations &, const TickDurations &) = default;
};

} // namespace flop::world
