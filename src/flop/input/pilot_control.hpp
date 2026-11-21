#pragma once

#include "flop/world/pilot_sample.hpp"

namespace flop::input {

// keyboard throttle rate (fraction per second).
constexpr double THROTTLE_RATE = 0.3;

struct PilotActions {
  // change in throttle. normalized to [-1.0, 1.0].
  // throttle cut and full events are handled separately
  double throttle_rate;
  // steer direction, positive is ccw. normalized to [-1.0, 1.0]
  double steering;
};

class PilotControl {
private:
  world::PilotSample sample_{0.0, 0.0};

public:
  void update(const PilotActions &action, double current_throttle, double real_dt);
  void reset_control() { sample_ = {0.0, 0.0}; }

  [[nodiscard]] const world::PilotSample &sample() const noexcept { return sample_; }
};

} // namespace flop::input
