#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace flop::app {

// input actions for TickScheduler
struct FrameActions {
  // toggle pause state in this frame?
  bool toggle_pause{false};
  // advance exactly one fixed step while paused.
  std::uint8_t single_steps{0};
  // requested changes in the warp ladder
  std::int8_t warp_steps{0};
  // return to 1x warp.
  bool warp_cancel{false};
};

// requests to tick scheduler after handling frame inputs
struct FrameSchedule {
  int fixed_step_count{0};
  double warp{1.0};
  bool paused{false};
  // wall seconds dropped this frame because the catch-up cap was hit. reported
  // for telemetry
  double discarded_wall_sec{0.0};
};

// directs the flow of simulation time.
class TickScheduler {
public:
  // real seconds per fixed step, without warp.
  static constexpr double FIXED_STEP_SEC = 0.02;
  // most fixed steps run in one frame. excess time debt is dropped in case of extreme lag spikes
  static constexpr int MAX_CATCHUP_STEPS = 8;

  TickScheduler() = default;

  // the warp factors, lowest first. index 0 is 1x
  [[nodiscard]] static std::span<const double> warp_ladder() noexcept;

  // advance one frame. wall_elapsed_sec is the elapsed time since the last advance() call. max_warp
  // is the current ceiling that the selected warp may not exceed
  FrameSchedule advance(double wall_elapsed_sec, FrameActions actions, double max_warp);

  [[nodiscard]] bool paused() const noexcept { return paused_; }
  [[nodiscard]] double warp() const noexcept;
  [[nodiscard]] std::size_t warp_index() const noexcept { return warp_index_; }

private:
  // clamp the warp index down so its factor does not exceed max_warp
  void clamp_to_ceiling(double max_warp) noexcept;

  bool paused_{false};
  std::size_t warp_index_{0};
  // real wall seconds not yet consumed by a fixed step
  double accumulator_sec_{0.0};
};

} // namespace flop::app
