#include "flop/app/tick_scheduler.hpp"

#include <algorithm>
#include <array>

namespace flop::app {
namespace {

// clang-format off

// time warp factor ladder
constexpr std::array<double, 19> WARP_LADDER{
    1,   2,   5,   10,  20,  50,  100, 300,
    1e3, 3e3, 1e4, 3e4, 1e5, 3e5, 1e6, 3e6,
    1e7, 3e7, 1e8,
};

// clang-format on

} // namespace

std::span<const double> TickScheduler::warp_ladder() noexcept {
  return WARP_LADDER;
}

double TickScheduler::warp() const noexcept {
  return WARP_LADDER.at(warp_index_);
}

void TickScheduler::clamp_to_ceiling(double max_warp) noexcept {
  while (warp_index_ > 0 && WARP_LADDER.at(warp_index_) > max_warp)
    --warp_index_;
}

FrameSchedule TickScheduler::advance(
    double real_dt, FrameActions actions, double max_warp
) {
  // apply one-off actions first
  if (actions.toggle_pause) {
    paused_ = !paused_;
  }
  if (actions.warp_cancel) {
    warp_index_ = 0;
  }
  if (actions.warp_steps != 0) {
    if (-actions.warp_steps > static_cast<int>(warp_index_))
      warp_index_ = 0;
    else
      warp_index_ = std::min(warp_index_ + actions.warp_steps, WARP_LADDER.size() - 1);
  }

  clamp_to_ceiling(max_warp);

  FrameSchedule schedule;
  schedule.warp = warp();
  schedule.paused = paused_;

  if (paused_) {
    // the accumulator does not run while paused
    real_time_accumulator_sec_ = 0.0;
    schedule.fixed_step_count = actions.single_steps;
    return schedule;
  }

  // accumulate elapsed real time
  if (real_dt > 0.0) {
    real_time_accumulator_sec_ += real_dt;
  }
  // schedule steps until accumulated time debt is resolved
  // step count is capped to prevent gigalag from evil unexpected frame delays
  int steps = static_cast<int>(real_time_accumulator_sec_ / REAL_TIME_STEP_SEC);
  if (steps > MAX_CATCHUP_STEPS) {
    // drop extra time to avoid accumulating massive time debt
    const double capped_consumption = MAX_CATCHUP_STEPS * REAL_TIME_STEP_SEC;
    const double excess = real_time_accumulator_sec_ - capped_consumption;
    real_time_accumulator_sec_ = 0.0;
    schedule.discarded_real_time_sec = excess;
    steps = MAX_CATCHUP_STEPS;
  } else {
    real_time_accumulator_sec_ -= steps * REAL_TIME_STEP_SEC;
  }
  schedule.fixed_step_count = steps;
  return schedule;
}

} // namespace flop::app
