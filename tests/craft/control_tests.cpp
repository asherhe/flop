#include <limits>

#include <catch2/catch_test_macros.hpp>

#include "flop/craft/control.hpp"
#include "flop/craft/craft.hpp"
#include "flop/craft/plugins/behavior_plugins.hpp"
#include "flop/craft/plugins/part_plugin.hpp"

#include "support/stub_craft_context.hpp"
#include "support/stub_universe_context.hpp"

namespace {

flop::orbit::RefFrameManager frames() {
  return flop::orbit::RefFrameManager(
      {{"Hamswell", 8.82e13, {}, {}, std::numeric_limits<double>::infinity()}}
  );
}

flop::craft::CraftConfig basic_config() {
  return {
      "Test Craft",
      flop::craft::CraftOrbitState{"Hamswell", {{3.0e6, 0}, {0, 5000}}, 0},
      {2.7, -1000},
      {{"Pod", "pod_1", "Pod", {0, 0}, 0, {}}}
  };
}

std::vector<flop::craft::PartTypeConfig> basic_types() {
  return {{"Pod", "Pod", 800, true, {{"bottom", {0, -1}}}}};
}

} // namespace

TEST_CASE("CraftControl clamps throttle into [0, 1]") {
  flop::craft::CraftControl control;
  CHECK(control.throttle() == 0.0);

  control.set_throttle(0.5);
  CHECK(control.throttle() == 0.5);

  control.set_throttle(1.5);
  CHECK(control.throttle() == 1.0);

  control.set_throttle(-0.5);
  CHECK(control.throttle() == 0.0);
}

TEST_CASE("CraftControl clamps steering into [-1, 1]") {
  flop::craft::CraftControl control;
  CHECK(control.steering() == 0.0);

  control.set_steering(0.5);
  CHECK(control.steering() == 0.5);

  control.set_steering(1.5);
  CHECK(control.steering() == 1.0);

  control.set_steering(-1.5);
  CHECK(control.steering() == -1.0);
}

TEST_CASE("CraftControl integrates throttle and clamps the accumulation") {
  flop::craft::CraftControl control;

  control.integrate_throttle(0.4);
  CHECK(control.throttle() == 0.4);

  control.integrate_throttle(0.4);
  CHECK(control.throttle() == 0.8);

  // accumulation saturates at the upper bound rather than overshooting.
  control.integrate_throttle(0.4);
  CHECK(control.throttle() == 1.0);

  // a negative rate backs the throttle down and saturates at zero.
  control.integrate_throttle(-1.5);
  CHECK(control.throttle() == 0.0);
}

TEST_CASE("Craft exposes a control channel that starts neutral and round-trips writes") {
  auto config = basic_config();
  const flop::testing::StubUniverseContext context(frames());
  flop::craft::Craft craft(config, context, basic_types());

  CHECK(craft.control().throttle() == 0.0);
  CHECK(craft.control().steering() == 0.0);

  craft.control().set_throttle(0.5);
  CHECK(craft.control().throttle() == 0.5);

  // writes clamp at the boundary like any other control write.
  craft.control().set_throttle(1.5);
  CHECK(craft.control().throttle() == 1.0);

  // the const accessor observes the same state.
  const flop::craft::Craft &const_craft = craft;
  CHECK(const_craft.control().throttle() == 1.0);
}

TEST_CASE("StubCraftContext control() throws until wired and then returns the backing control") {
  flop::testing::StubCraftContext ctx;
  CHECK_THROWS_AS(ctx.control(), std::logic_error);

  flop::craft::CraftControl control;
  ctx.with_control(control);
  ctx.control().set_throttle(0.25);
  CHECK(control.throttle() == 0.25);
}

TEST_CASE("CommandPlugin leaves throttle and steering to the universe") {
  flop::craft::CraftControl control;
  flop::testing::StubCraftContext ctx;
  ctx.with_control(control);
  control.set_throttle(0.4);
  control.set_steering(-0.3);

  flop::craft::plugin::CommandPlugin command({}, true);
  command.tick(ctx, 1.0);

  CHECK(control.throttle() == 0.4);
  CHECK(control.steering() == -0.3);
}

TEST_CASE("CommandPlugin ticks in the control phase") {
  const flop::craft::plugin::CommandPlugin command({}, true);
  CHECK(command.tick_phase() == flop::craft::plugin::TickPhase::control);
}
