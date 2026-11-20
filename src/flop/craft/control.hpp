#pragma once

#include <algorithm>

namespace flop::craft {

// persistent control state shared between the universe (the writer) and its actuators (readers).
// and its actuators (readers). throttle is a fraction in [0, 1] that integrates
// across steps; steering is an instantaneous command in [-1, 1], positive being
// counterclockwise. both are clamped at every write so readers never observe an
// out-of-range value.
class CraftControl {
public:
  [[nodiscard]] double throttle() const noexcept { return throttle_; }
  [[nodiscard]] double steering() const noexcept { return steering_; }

  void set_throttle(double throttle) noexcept { throttle_ = std::clamp(throttle, 0.0, 1.0); }
  void set_steering(double steering) noexcept { steering_ = std::clamp(steering, -1.0, 1.0); }

  // advance the persistent throttle by a signed amount, then clamp back into
  // range. no simulation step calls this; the pilot's throttle is set directly.
  void integrate_throttle(double delta) noexcept {
    throttle_ = std::clamp(throttle_ + delta, 0.0, 1.0);
  }

private:
  double throttle_{};
  double steering_{};
};

} // namespace flop::craft
