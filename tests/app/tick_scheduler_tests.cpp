// TickScheduler: pause, single-step, the warp ladder and ceiling, the 20 ms
// fixed-step accumulator, and the eight-step catch-up cap. The scheduler has no
// SDL or Universe dependency; these drive it with plain wall time, actions, and
// a warp ceiling.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "flop/app/tick_scheduler.hpp"

namespace {

using Catch::Approx;
using flop::app::FrameActions;
using flop::app::TickScheduler;

constexpr double STEP = TickScheduler::FIXED_STEP_SEC;
constexpr double NO_CEILING = 1e18;

} // namespace

TEST_CASE("A frame emits one fixed step per whole 20 ms of wall time") {
  TickScheduler scheduler;
  // exactly three steps' worth of wall time.
  const auto schedule = scheduler.advance(3 * STEP, {}, NO_CEILING);
  CHECK(schedule.fixed_step_count == 3);
  CHECK(schedule.warp == 1.0);
  CHECK_FALSE(schedule.paused);
  CHECK(schedule.discarded_wall_sec == 0.0);
}

TEST_CASE("Leftover wall time carries into the next frame") {
  TickScheduler scheduler;
  // 2.5 steps: two now, half a step retained.
  CHECK(scheduler.advance(2.5 * STEP, {}, NO_CEILING).fixed_step_count == 2);
  // another 0.6 steps brings the remainder to 1.1 steps: one more now.
  CHECK(scheduler.advance(0.6 * STEP, {}, NO_CEILING).fixed_step_count == 1);
}

TEST_CASE("Pause halts stepping and clears the accumulator") {
  TickScheduler scheduler;
  FrameActions pause;
  pause.toggle_pause = true;
  auto schedule = scheduler.advance(5 * STEP, pause, NO_CEILING);
  CHECK(schedule.paused);
  CHECK(schedule.fixed_step_count == 0);

  // while paused, wall time does not accumulate into later steps.
  CHECK(scheduler.advance(10 * STEP, {}, NO_CEILING).fixed_step_count == 0);

  FrameActions unpause;
  unpause.toggle_pause = true;
  // resuming does not replay the wall time spent paused.
  CHECK(scheduler.advance(0.0, unpause, NO_CEILING).fixed_step_count == 0);
}

TEST_CASE("Single-step runs exactly one step while paused") {
  TickScheduler scheduler;
  FrameActions pause;
  pause.toggle_pause = true;
  (void)scheduler.advance(0.0, pause, NO_CEILING);

  FrameActions step;
  step.single_steps = 1;
  const auto schedule = scheduler.advance(0.0, step, NO_CEILING);
  CHECK(schedule.paused);
  CHECK(schedule.fixed_step_count == 1);

  // single-step does not consume or depend on wall time.
  CHECK(scheduler.advance(0.0, {}, NO_CEILING).fixed_step_count == 0);
}

TEST_CASE("The catch-up cap bounds steps and reports discarded wall time") {
  TickScheduler scheduler;
  // twelve steps' worth of wall time, capped at eight.
  const auto schedule = scheduler.advance(12 * STEP, {}, NO_CEILING);
  CHECK(schedule.fixed_step_count == TickScheduler::MAX_CATCHUP_STEPS);
  CHECK(schedule.discarded_wall_sec == Approx(4 * STEP));
  // the accumulator was cleared, so the next frame starts fresh.
  CHECK(scheduler.advance(STEP, {}, NO_CEILING).fixed_step_count == 1);
}

TEST_CASE("Warp climbs and descends the ladder and cancels to 1x") {
  TickScheduler scheduler;
  FrameActions up;
  up.warp_steps = 1;
  CHECK(scheduler.advance(0.0, up, NO_CEILING).warp == 2.0);
  CHECK(scheduler.advance(0.0, up, NO_CEILING).warp == 5.0);
  CHECK(scheduler.advance(0.0, up, NO_CEILING).warp == 10.0);

  FrameActions down;
  down.warp_steps = -1;
  CHECK(scheduler.advance(0.0, down, NO_CEILING).warp == 5.0);

  FrameActions cancel;
  cancel.warp_cancel = true;
  CHECK(scheduler.advance(0.0, cancel, NO_CEILING).warp == 1.0);
}

TEST_CASE("Each fixed step advances simulation by 20 ms times the warp factor") {
  TickScheduler scheduler;
  FrameActions up;
  up.warp_steps = 1;
  for (int i = 0; i < 4; ++i) {
    (void)scheduler.advance(0.0, up, NO_CEILING); // climb to 20x.
  }
  REQUIRE(scheduler.warp() == 20.0);
  const auto schedule = scheduler.advance(STEP, {}, NO_CEILING);
  REQUIRE(schedule.fixed_step_count == 1);
  // a 20 ms wall step at 20x covers 0.4 s of simulation time.
  CHECK(schedule.warp * STEP == Approx(0.4));
}

TEST_CASE("The warp ceiling clamps the selected warp down") {
  TickScheduler scheduler;
  FrameActions up;
  up.warp_steps = 1;
  for (int i = 0; i < 6; ++i) {
    (void)scheduler.advance(0.0, up, NO_CEILING); // climb well past 20x.
  }
  REQUIRE(scheduler.warp() > 20.0);

  // a powered ceiling of 20x clamps the next frame down to 20x.
  const auto schedule = scheduler.advance(0.0, {}, 20.0);
  CHECK(schedule.warp == 20.0);

  // attempts to climb above the ceiling are held at it while it stands.
  CHECK(scheduler.advance(0.0, up, 20.0).warp == 20.0);

  // once the ceiling lifts, warp can climb again.
  CHECK(scheduler.advance(0.0, up, NO_CEILING).warp == 50.0);
}
