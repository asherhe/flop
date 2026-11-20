#pragma once

namespace flop::world {

// pilot input state input for a Craft
struct PilotSample {
  double throttle{};
  double steering{};

  friend constexpr bool operator==(const PilotSample &, const PilotSample &) = default;
};

} // namespace flop::world
