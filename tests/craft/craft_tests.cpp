#include <limits>
#include <stdexcept>
#include <variant>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "flop/craft/craft.hpp"
#include "flop/craft/plugins/behavior_plugins.hpp"
#include "flop/craft/plugins/part_plugin.hpp"
#include "flop/craft/plugins/resource_plugin.hpp"

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

TEST_CASE("Craft owns its resolved orbital state and initial patch") {
  auto config = basic_config();
  const flop::testing::StubUniverseContext context(frames());
  const auto &reference_frames = context.reference_frames();
  flop::craft::Craft craft(config, context, basic_types());
  config.name = "Changed";

  CHECK(craft.name() == "Test Craft");
  CHECK(craft.orbit_state().frame_id == reference_frames.frame_id("Hamswell"));
  CHECK(craft.orbit_state().epoch == 0);
  REQUIRE(craft.patches().patches.size() == 1);
  CHECK(craft.patches().patches.front().orbit.frame_id == craft.orbit_state().frame_id);

  craft.predict_patches(reference_frames, 0, 60);
  CHECK(craft.patches().searched_through == 60);
  CHECK(craft.patches().patch_at(60) != nullptr);
}

TEST_CASE("Craft validates parent reference and typed orbital inputs") {
  const flop::testing::StubUniverseContext context(frames());
  auto config = basic_config();
  config.orbit->parent_name = "missing";
  CHECK_THROWS_AS(flop::craft::Craft(config, context, basic_types()), std::invalid_argument);

  config = basic_config();
  config.orbit->state.position = {0, 0}; // a degenerate seed at the parent center is rejected.
  CHECK_THROWS_AS(flop::craft::Craft(config, context, basic_types()), std::invalid_argument);
}

TEST_CASE("Craft cannot be built from a template that has no orbit") {
  const flop::testing::StubUniverseContext context(frames());
  auto config = basic_config();
  config.orbit.reset(); // a template: no world position.
  CHECK_THROWS_WITH(
      flop::craft::Craft(config, context, basic_types()),
      Catch::Matchers::ContainsSubstring("no orbit")
  );
}

TEST_CASE("Craft owns installed parts and resolves reciprocal attachment nodes") {
  auto config = basic_config();
  config.parts = {
      {"Pod", "pod_1", "Pod", {0, 0}, 0, {{"bottom", "tank_2", "top"}}},
      {"Tank", "tank_2", "Tank", {0, -2}, 0, {{"top", "pod_1", "bottom"}}},
  };
  std::vector<flop::craft::PartTypeConfig> types{
      {"Pod", "Pod", 800, true, {{"bottom", {0, -1}}}},
      {"Tank", "Tank", 600, true, {{"top", {0, 1}}}},
  };
  const flop::testing::StubUniverseContext context(frames());
  const flop::craft::Craft craft(config, context, types);
  REQUIRE(craft.parts().size() == 2);
  CHECK(craft.part("tank_2").dry_mass_kg == 600);
  CHECK(craft.part("tank_2").position_meters.y == -2);
  CHECK(craft.part("pod_1").attachments.front().attach_part_id == "tank_2");

  config.parts[1].attachments.clear();
  CHECK_THROWS_AS(flop::craft::Craft(config, context, types), std::invalid_argument);
}

TEST_CASE("Craft exposes runtime resource state from installed container plugins") {
  using flop::craft::ResourceFlow;
  auto config = basic_config();
  config.plugins_resolved = true;
  config.parts = {
      {"Pod", "pod_1", "Pod", {0, 0}, 0, {{"bottom", "tank_2", "top"}}, {}},
      {"Tank",
       "tank_2",
       "Tank",
       {0, -2},
       0,
       {{"top", "pod_1", "bottom"}},
       {{"ResourceContainerPlugin",
         true,
         flop::craft::plugin::ResourceConfig{{{"NTO", 3600}}, {{"NTO", 1200}}, {{"NTO", 1}}}}}},
  };
  std::vector<flop::craft::PartTypeConfig> types{
      {"Pod", "Pod", 800, true, {{"bottom", {0, -1}}}},
      {"Tank", "Tank", 600, true, {{"top", {0, 1}}}},
  };
  const flop::testing::StubUniverseContext context(
      frames(), {{"NTO", "N2O4", 1.0, ResourceFlow::stage}}
  );
  const flop::craft::Craft craft(config, context, types);

  const auto &tank_plugins = craft.part("tank_2").installed_plugins();
  REQUIRE(tank_plugins.size() == 1);
  const auto *container =
      dynamic_cast<const flop::craft::plugin::ResourcePlugin *>(tank_plugins[0].get());
  REQUIRE(container != nullptr);
  REQUIRE(container->slots().size() == 1);
  CHECK(container->slots().front().resource_id == "NTO");
  CHECK(container->slots().front().amount_units == 1200);
  CHECK(container->slots().front().max_amount_units == 3600);
}

TEST_CASE("Craft builds runtime plugin objects for each installed plugin") {
  using flop::craft::ResourceFlow;
  using flop::craft::plugin::EnginePlugin;
  using flop::craft::plugin::ResourcePlugin;
  auto config = basic_config();
  config.plugins_resolved = true;
  config.parts = {
      {"Pod", "pod_1", "Pod", {0, 0}, 0, {}, {}},
      {"Tank",
       "tank_2",
       "Tank",
       {0, -2},
       0,
       {},
       {{"EnginePlugin", true, flop::craft::plugin::EngineConfig{}},
        {"ResourceContainerPlugin",
         false,
         flop::craft::plugin::ResourceConfig{{{"NTO", 3600}}, {{"NTO", 1200}}, {}}}}},
  };
  std::vector<flop::craft::PartTypeConfig> types{
      {"Pod", "Pod", 800, true, {{"bottom", {0, -1}}}},
      {"Tank", "Tank", 600, true, {{"top", {0, 1}}}},
  };
  const flop::testing::StubUniverseContext context(
      frames(), {{"NTO", "N2O4", 1.0, ResourceFlow::stage}}
  );
  const flop::craft::Craft craft(config, context, types);

  CHECK(craft.part("pod_1").installed_plugins().empty());
  const auto &tank_plugins = craft.part("tank_2").installed_plugins();
  REQUIRE(tank_plugins.size() == 2);
  auto *engine = dynamic_cast<EnginePlugin *>(tank_plugins[0].get());
  REQUIRE(engine != nullptr);
  CHECK(engine->enabled());
  auto *container = dynamic_cast<ResourcePlugin *>(tank_plugins[1].get());
  REQUIRE(container != nullptr);
  // the configured enabled flag is carried onto the runtime plugin.
  CHECK_FALSE(container->enabled());
  REQUIRE(container->slots().size() == 1);
  CHECK(container->slots().front().resource_id == "NTO");
  CHECK(container->slots().front().amount_units == 1200);
}

TEST_CASE("Craft step integrates attitude from the configured spin") {
  auto config = basic_config();
  // give the craft an initial spin; two separated parts give it a real inertia.
  config.initial_attitude = {0, 1600};
  config.parts = {
      {"Pod", "pod_1", "Pod", {0, 1}, 0, {{"bottom", "tank_2", "top"}}, {}},
      {"Tank", "tank_2", "Tank", {0, -1}, 0, {{"top", "pod_1", "bottom"}}, {}},
  };
  std::vector<flop::craft::PartTypeConfig> types{
      {"Pod", "Pod", 800, true, {{"bottom", {0, -1}}}},
      {"Tank", "Tank", 800, true, {{"top", {0, 1}}}},
  };
  const flop::testing::StubUniverseContext context(frames());
  flop::craft::Craft craft(config, context, types);

  const double before = craft.newtonian().angle_rad();
  craft.step(0, 1.0);
  // equal masses at y = +/-1 about the com at the origin: I = 800+800 = 1600.
  // omega = L / I = 1600 / 1600 = 1 rad/s, so the angle advances by 1 rad.
  CHECK(craft.newtonian().angular_velocity_rad_per_s() == Catch::Approx(1));
  CHECK(craft.newtonian().angle_rad() == Catch::Approx(before + 1));
}

TEST_CASE("Craft step preserves the analytical seed while coasting") {
  auto config = basic_config();
  const flop::testing::StubUniverseContext context(frames());
  flop::craft::Craft craft(config, context, basic_types());

  const auto seed_before = craft.orbit_state();
  REQUIRE(seed_before.epoch == 0);

  // no force is applied, so the craft coasts: the seed must be left untouched so
  // that later sampling stays exact rather than drifting through re-seeds.
  craft.step(120, 1.0);
  CHECK(craft.orbit_state().epoch == 0);
  CHECK(craft.orbit_state().state.position == seed_before.state.position);
  CHECK(craft.orbit_state().state.velocity == seed_before.state.velocity);
}

TEST_CASE("Craft step reuses the cached propagator across coasting steps") {
  auto config = basic_config();
  const flop::testing::StubUniverseContext context(frames());
  flop::craft::Craft craft(config, context, basic_types());

  const auto seed_before = craft.orbit_state();

  // many coasting steps at advancing UT: the seed must stay fixed throughout, so
  // the cached propagator built from it is reused rather than rebuilt per step.
  for (double ut = 10; ut <= 600; ut += 10) {
    craft.step(ut, 1.0);
  }
  CHECK(craft.orbit_state().epoch == seed_before.epoch);
  CHECK(craft.orbit_state().state.position == seed_before.state.position);
  CHECK(craft.orbit_state().state.velocity == seed_before.state.velocity);
}

namespace {

// a two-part craft wired for a burn: a command pod at the origin and a combined
// engine/tank below it. the engine draws the tank's propellant (stage flow
// reaches the engine's own part), so a burn empties the tank and shifts the
// center of mass toward the pod.
flop::craft::CraftConfig burn_config() {
  using flop::craft::plugin::CommandConfig;
  using flop::craft::plugin::EngineConfig;
  using flop::craft::plugin::ResourceConfig;
  flop::craft::CraftConfig config{
      "Burn Craft",
      flop::craft::CraftOrbitState{"Hamswell", {{3.0e6, 0}, {0, 5000}}, 0},
      {0, 0},
      {}
  };
  config.plugins_resolved = true;
  config.parts = {
      {"Pod",
       "pod",
       "Pod",
       {0, 0},
       0,
       {{"bottom", "engine", "top"}},
       {{"CommandPlugin", true, CommandConfig{}}}},
      {"Engine",
       "engine",
       "Engine",
       {0, -2},
       0,
       {{"top", "pod", "bottom"}},
       {{"EnginePlugin", true, EngineConfig{200.0, 1000.0, {0, 1}, {{"NTO", 1.0}}}},
        {"ResourceContainerPlugin", true, ResourceConfig{{{"NTO", 1000}}, {{"NTO", 1000}}, {}}}}},
  };
  return config;
}

std::vector<flop::craft::PartTypeConfig> burn_types() {
  return {
      {"Pod", "Pod", 400, true, {{"bottom", {0, -1}}}},
      {"Engine", "Engine", 200, true, {{"top", {0, 1}}}},
  };
}

flop::craft::plugin::CommandPlugin &command_of(flop::craft::Craft &craft) {
  for (const auto &plugin : craft.part("pod").installed_plugins()) {
    if (auto *command = dynamic_cast<flop::craft::plugin::CommandPlugin *>(plugin.get())) {
      return *command;
    }
  }
  FAIL("pod has no command plugin");
  throw std::logic_error("unreachable");
}

} // namespace

TEST_CASE("Craft burn drains propellant, lightens the craft, and shifts the center of mass") {
  const flop::testing::StubUniverseContext context(
      frames(), {{"NTO", "N2O4", 1.0, flop::craft::ResourceFlow::stage}}
  );
  flop::craft::Craft craft(burn_config(), context, burn_types());

  // full dry + propellant mass: pod 400, engine 200 dry, 1000 units * 1 kg/unit.
  const auto mass_before = craft.mass_properties();
  CHECK(mass_before.total_mass_kg == Catch::Approx(1600));
  const double com_before = mass_before.center_of_mass_meters.y;

  // drive the throttle fully open in one step, then hold it there.
  command_of(craft).set_throttle_input(10.0);

  double previous_mass = mass_before.total_mass_kg;
  double ut = 0.0;
  for (int i = 0; i < 20; ++i) {
    ut += 1.0;
    craft.step(ut, 1.0);
    const double mass_now = craft.mass_properties().total_mass_kg;
    // each thrusting step burns propellant, so the craft is strictly lighter.
    CHECK(mass_now < previous_mass);
    previous_mass = mass_now;
  }

  // the tank (below the pod) lost mass, so the center of mass moved up toward
  // the pod at the origin.
  const double com_after = craft.mass_properties().center_of_mass_meters.y;
  CHECK(com_after > com_before);
}

TEST_CASE("Craft burn reseeds the orbit while thrusting and holds the seed when coasting") {
  const flop::testing::StubUniverseContext context(
      frames(), {{"NTO", "N2O4", 1.0, flop::craft::ResourceFlow::stage}}
  );
  flop::craft::Craft craft(burn_config(), context, burn_types());

  command_of(craft).set_throttle_input(10.0);

  // a thrusting step changes the linear velocity, so the seed is reinstalled at
  // the step's UT rather than staying at the epoch.
  craft.step(1.0, 1.0);
  CHECK(craft.orbit_state().epoch == 1.0);
  const auto velocity_after_burn = craft.orbit_state().state.velocity;

  craft.step(2.0, 1.0);
  CHECK(craft.orbit_state().epoch == 2.0);
  CHECK(craft.orbit_state().state.velocity != velocity_after_burn);

  // cut the throttle and let it fall to zero, then coast: the seed stops moving.
  command_of(craft).set_throttle_input(-10.0);
  craft.step(3.0, 1.0); // throttle integrates back to zero this step
  const auto seed_coasting = craft.orbit_state();
  for (double t = 4.0; t <= 10.0; t += 1.0) {
    craft.step(t, 1.0);
  }
  CHECK(craft.orbit_state().epoch == seed_coasting.epoch);
  CHECK(craft.orbit_state().state.position == seed_coasting.state.position);
  CHECK(craft.orbit_state().state.velocity == seed_coasting.state.velocity);
}

TEST_CASE("Craft burn flames out the engine when the propellant runs dry") {
  const flop::testing::StubUniverseContext context(
      frames(), {{"NTO", "N2O4", 1.0, flop::craft::ResourceFlow::stage}}
  );
  // a small tank so a handful of steps empties it.
  auto config = burn_config();
  std::get<flop::craft::plugin::ResourceConfig>(config.parts[1].plugins[1].values) =
      flop::craft::plugin::ResourceConfig{{{"NTO", 1000}}, {{"NTO", 1.0}}, {}};
  flop::craft::Craft craft(config, context, burn_types());

  command_of(craft).set_throttle_input(10.0);

  // burn until the tank is dry; the engine disables itself on the empty step.
  double ut = 0.0;
  for (int i = 0; i < 10; ++i) {
    ut += 1.0;
    craft.step(ut, 1.0);
  }

  const auto &engine_plugins = craft.part("engine").installed_plugins();
  const auto *engine =
      dynamic_cast<const flop::craft::plugin::EnginePlugin *>(engine_plugins[0].get());
  REQUIRE(engine != nullptr);
  CHECK_FALSE(engine->enabled());

  // with the engine off, the craft coasts: mass holds steady across more steps.
  const double mass_dry = craft.mass_properties().total_mass_kg;
  craft.step(ut + 1.0, 1.0);
  CHECK(craft.mass_properties().total_mass_kg == Catch::Approx(mass_dry));
}
