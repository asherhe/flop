// Universe::tick(dt): the held pilot sample is written into the active craft
// before the step, only the active craft receives it, UT advances by dt, and the
// world-owned warp ceiling that the tick scheduler is handed.

#include <cmath>
#include <limits>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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
  universe.tick(0.4);
  CHECK(universe.ut_sec() == Approx(0.4));
  universe.tick(0.4);
  CHECK(universe.ut_sec() == Approx(0.8));
}

TEST_CASE("tick rejects nonpositive or nonfinite simulation durations") {
  Universe universe({root_body()}, resource_types());
  CHECK_THROWS(universe.tick(0.0));
  CHECK_THROWS(universe.tick(-0.4));
  CHECK_THROWS(universe.tick(std::numeric_limits<double>::infinity()));
}

TEST_CASE("The pilot throttle is written to the craft before the step and burns over the step") {
  Universe universe({root_body()}, resource_types());
  const CraftId id = universe.add_craft(burn_craft(), craft_types());
  universe.controls().submit(ControlManager::SelectCraft{id});
  (void)universe.apply_commands();

  // an absolute throttle of 0.6 from the pilot sample.
  const double throttle = 0.6;
  universe.controls().set_pilot_sample(PilotSample{throttle, 0.0});

  const double fuel_before = tank_fuel(universe.craft(id));

  // one step at 20x warp: the simulation duration is 0.4 s.
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
  const CraftId first = universe.add_craft(burn_craft(), craft_types());
  const CraftId second = universe.add_craft(burn_craft(), craft_types());

  universe.controls().submit(ControlManager::SelectCraft{first});
  (void)universe.apply_commands();
  universe.controls().set_pilot_sample(PilotSample{0.5, 0.0});
  universe.tick(0.02);

  CHECK(universe.craft(first).control().throttle() == Approx(0.5));
  // the unselected craft received no held input.
  CHECK(universe.craft(second).control().throttle() == 0.0);
}

TEST_CASE("The pilot steering is written to the active craft before the step") {
  Universe universe({root_body()}, resource_types());
  const CraftId first = universe.add_craft(burn_craft(), craft_types());
  const CraftId second = universe.add_craft(burn_craft(), craft_types());

  universe.controls().submit(ControlManager::SelectCraft{first});
  (void)universe.apply_commands();
  universe.controls().set_pilot_sample(PilotSample{0.0, -0.25});
  universe.tick(0.02);

  CHECK(universe.craft(first).control().steering() == Approx(-0.25));
  CHECK(universe.craft(second).control().steering() == 0.0);
}

TEST_CASE("The warp ceiling drops to the powered limit when a craft is burning") {
  Universe universe({root_body()}, resource_types());
  const CraftId id = universe.add_craft(burn_craft(), craft_types());

  // no craft powered yet: no ceiling.
  CHECK(universe.any_craft_powered() == false);
  CHECK(std::isinf(flop::world::warp_ceiling(universe)));

  // open the throttle on the craft.
  universe.controls().submit(ControlManager::SelectCraft{id});
  universe.controls().submit(ControlManager::SetThrottle{0.5});
  (void)universe.apply_commands();

  CHECK(universe.any_craft_powered());
  CHECK(flop::world::warp_ceiling(universe) == flop::world::POWERED_WARP_CEILING);
}
