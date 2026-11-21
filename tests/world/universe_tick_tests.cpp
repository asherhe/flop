// World stepping and explicit control handoff.

#include <cmath>
#include <limits>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "flop/app/warp_policy.hpp"
#include "flop/craft/config.hpp"
#include "flop/craft/craft.hpp"
#include "flop/craft/plugins/behavior_plugins.hpp"
#include "flop/craft/plugins/config.hpp"
#include "flop/craft/plugins/resource_plugin.hpp"
#include "flop/craft/resource_type_config.hpp"
#include "flop/world/control_manager.hpp"
#include "flop/world/pilot_sample.hpp"
#include "flop/world/universe.hpp"

namespace {

using Catch::Approx;
using flop::craft::CraftConfig;
using flop::craft::CraftOrbitState;
using flop::craft::PartTypeConfig;
using flop::craft::ResourceFlow;
using flop::craft::ResourceTypeConfig;
using flop::craft::plugin::CommandConfig;
using flop::craft::plugin::EngineConfig;
using flop::craft::plugin::ResourceConfig;
using flop::craft::plugin::ResourcePlugin;
using flop::world::ControlManager;
using flop::world::CraftId;
using flop::world::PilotSample;
using flop::world::Universe;

flop::world::BodyConfig root_body() {
  return {"Hamswell", 3.0e6, 9.8, 86400, {}};
}

std::vector<ResourceTypeConfig> resource_types() {
  return {{"NTO", "N2O4", 1.0, ResourceFlow::stage}};
}

// a pod with a command plugin, and an engine that carries its own propellant so
// a powered step burns fuel without needing crossfeed attachments.
std::vector<PartTypeConfig> craft_types() {
  return {
      {"Pod", "Pod", 400, true, {}},
      {"Engine", "Engine", 200, true, {}},
  };
}

CraftConfig burn_craft() {
  CraftConfig config{
      "Burner",
      CraftOrbitState{"Hamswell", {{3.0e6, 0}, {0, 5000}}, 0},
      {0.0, 0.0},
      {
          {"Pod", "pod", "Pod", {0, 0}, 0, {}, {{"CommandPlugin", true, CommandConfig{}}}},
          {"Engine",
           "engine",
           "Engine",
           {0, -2},
           0,
           {},
           {{"EnginePlugin", true, EngineConfig{100.0, 980.0, {1, 0}, {{"NTO", 1.0}}}},
            {"ResourceContainerPlugin",
             true,
             ResourceConfig{{{"NTO", 100.0}}, {{"NTO", 100.0}}, {}}}}},
      },
  };
  config.plugins_resolved = true;
  return config;
}

double tank_fuel(const flop::craft::Craft &craft) {
  for (const auto &plugin : craft.part("engine").installed_plugins()) {
    if (auto *tank = dynamic_cast<const ResourcePlugin *>(plugin.get())) {
      return tank->find_slot("NTO")->amount_units;
    }
  }
  return -1;
}

} // namespace

TEST_CASE("tick advances UT by the simulation duration") {
  Universe universe({root_body()}, resource_types());
  ControlManager &controls = universe.controls();
  universe.tick(0.4);
  CHECK(universe.ut_sec() == Approx(0.4));
  universe.tick(0.4);
  CHECK(universe.ut_sec() == Approx(0.8));
}

TEST_CASE("tick rejects nonpositive or nonfinite simulation durations") {
  Universe universe({root_body()}, resource_types());
  ControlManager &controls = universe.controls();
  CHECK_THROWS(universe.tick(0.0));
  CHECK_THROWS(universe.tick(-0.4));
  CHECK_THROWS(universe.tick(std::numeric_limits<double>::infinity()));
}

TEST_CASE("The pilot throttle is written to the craft before the step and burns over the step") {
  Universe universe({root_body()}, resource_types());
  ControlManager &controls = universe.controls();
  const CraftId id = universe.add_craft(burn_craft(), craft_types());
  controls.submit(ControlManager::SelectCraft{id});
  (void)controls.apply_commands();

  // an absolute throttle of 0.6 from the pilot sample.
  const double throttle = 0.6;
  controls.set_pilot_sample(PilotSample{throttle, 0.0});

  const double fuel_before = tank_fuel(universe.craft(id));

  // one step at 20x warp: the simulation duration is 0.4 s.
  controls.apply_pilot_sample();
  universe.tick(0.4);

  CHECK(universe.craft(id).control().throttle() == Approx(throttle));
  // UT advanced by the simulation duration.
  CHECK(universe.ut_sec() == Approx(0.4));
  // the engine burned fuel over the step at the throttle written before it:
  // units = dt * thrust * throttle / (isp * g0).
  const double expected_burn = 0.4 * 980.0 * throttle / (100.0 * 9.8);
  CHECK(tank_fuel(universe.craft(id)) == Approx(fuel_before - expected_burn));
  CHECK(expected_burn > 0.0);
}

TEST_CASE("Held pilot input only reaches the active craft") {
  Universe universe({root_body()}, resource_types());
  ControlManager &controls = universe.controls();
  const CraftId first = universe.add_craft(burn_craft(), craft_types());
  const CraftId second = universe.add_craft(burn_craft(), craft_types());

  controls.submit(ControlManager::SelectCraft{first});
  (void)controls.apply_commands();
  controls.set_pilot_sample(PilotSample{0.5, 0.0});
  controls.apply_pilot_sample();
  universe.tick(0.02);

  CHECK(universe.craft(first).control().throttle() == Approx(0.5));
  // the unselected craft received no held input.
  CHECK(universe.craft(second).control().throttle() == 0.0);
}

TEST_CASE("The pilot steering is written to the active craft before the step") {
  Universe universe({root_body()}, resource_types());
  ControlManager &controls = universe.controls();
  const CraftId first = universe.add_craft(burn_craft(), craft_types());
  const CraftId second = universe.add_craft(burn_craft(), craft_types());

  controls.submit(ControlManager::SelectCraft{first});
  (void)controls.apply_commands();
  controls.set_pilot_sample(PilotSample{0.0, -0.25});
  controls.apply_pilot_sample();
  universe.tick(0.02);

  CHECK(universe.craft(first).control().steering() == Approx(-0.25));
  CHECK(universe.craft(second).control().steering() == 0.0);
}

TEST_CASE("The warp ceiling drops to the powered limit when a craft is burning") {
  Universe universe({root_body()}, resource_types());
  ControlManager &controls = universe.controls();
  const CraftId id = universe.add_craft(burn_craft(), craft_types());

  // no craft powered yet: no ceiling.
  CHECK(universe.any_craft_powered() == false);
  CHECK(std::isinf(flop::app::warp_ceiling(universe)));

  // open the throttle on the craft.
  controls.submit(ControlManager::SelectCraft{id});
  controls.submit(ControlManager::SetThrottle{0.5});
  (void)controls.apply_commands();

  CHECK(universe.any_craft_powered());
  CHECK(flop::app::warp_ceiling(universe) == flop::app::POWERED_WARP_CEILING);
}

TEST_CASE("Inactive powered craft continues burning and limits warp") {
  Universe universe({root_body()}, resource_types());
  ControlManager &controls = universe.controls();
  const auto burning = universe.add_craft(burn_craft(), craft_types());
  const auto other = universe.add_craft(burn_craft(), craft_types());
  controls.submit(ControlManager::SelectCraft{burning});
  controls.submit(ControlManager::SetThrottle{0.5});
  controls.submit(ControlManager::SelectCraft{other});
  (void)controls.apply_commands();
  const double before = tank_fuel(universe.craft(burning));
  universe.tick(0.4);
  CHECK(tank_fuel(universe.craft(burning)) < before);
  CHECK(flop::app::warp_ceiling(universe) == flop::app::POWERED_WARP_CEILING);
}

TEST_CASE("World tick does not apply a held pilot sample") {
  Universe universe({root_body()}, resource_types());
  ControlManager &controls = universe.controls();
  const auto id = universe.add_craft(burn_craft(), craft_types());
  controls.submit(ControlManager::SelectCraft{id});
  (void)controls.apply_commands();
  controls.set_pilot_sample(PilotSample{0.7, 0.5});
  universe.tick(0.02);
  CHECK(universe.craft(id).control().throttle() == 0.0);
  CHECK(universe.craft(id).control().steering() == 0.0);
}

TEST_CASE("Every craft steps from the same pre-tick UT") {
  Universe universe({root_body()}, resource_types(), 10.0);
  ControlManager &controls = universe.controls();
  const auto first = universe.add_craft(burn_craft(), craft_types());
  const auto second = universe.add_craft(burn_craft(), craft_types());
  controls.submit(ControlManager::SelectCraft{first});
  controls.submit(ControlManager::SetThrottle{0.5});
  controls.submit(ControlManager::SelectCraft{second});
  controls.submit(ControlManager::SetThrottle{0.5});
  (void)controls.apply_commands();
  universe.tick(0.4);
  CHECK(universe.craft(first).orbit_state().epoch == Approx(10.0));
  CHECK(universe.craft(second).orbit_state().epoch == Approx(10.0));
  CHECK(universe.ut_sec() == Approx(10.4));
}
