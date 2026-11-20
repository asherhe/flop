#pragma once

namespace flop::world {

// the latest held flight controls sampled from the pilot for the active craft,
// carried into each fixed simulation step. the throttle rate is a signed change
// per second that the active command plugin integrates into its persistent
// throttle; steering is an instantaneous level in [-1, 1], positive being
// counterclockwise. a neutral sample (both zero) applies no held input, which
// is what an inactive or focus-lost craft receives.
struct PilotSample {
  double throttle_rate_per_sec{};
  double steering{};

  friend constexpr bool operator==(const PilotSample &, const PilotSample &) = default;
};

} // namespace flop::world
