#include "flop/input/pilot_control.hpp"

#include <algorithm>

namespace flop::input {

void PilotControl::update(
    const PilotActions &action, double current_throttle, double real_dt
) {
  const double d_throttle = action.throttle_rate * real_dt * THROTTLE_RATE;
  sample_.throttle = std::clamp(current_throttle + d_throttle, 0.0, 1.0);
  sample_.steering = action.steering;
}

} // namespace flop::input
