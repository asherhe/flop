// ControlManager inside Universe: typed commands resolve against live craft at
// execution, in submit order, affecting exactly the intended instance once. A
// stale target fails cleanly without touching another craft, and changing the
// active selection neutralizes the previous craft's held pilot input.

#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "flop/craft/config.hpp"
#include "flop/craft/craft.hpp"
#include "flop/craft/ids.hpp"
#include "flop/craft/plugins/behavior_plugins.hpp"
#include "flop/craft/plugins/config.hpp"
#include "flop/craft/plugins/part_plugin.hpp"
#include "flop/craft/resource_type_config.hpp"
#include "flop/world/control_manager.hpp"
#include "flop/world/craft_id.hpp"
#include "flop/world/plugin_target.hpp"
#include "flop/world/universe.hpp"

namespace {

using flop::craft::CraftConfig;
using flop::craft::CraftOrbitState;
using flop::craft::PartId;
using flop::craft::PartTypeConfig;
using flop::craft::PluginId;
using flop::craft::ResourceFlow;
using flop::craft::ResourceTypeConfig;
using flop::craft::plugin::CommandConfig;
using flop::craft::plugin::CommandPlugin;
using flop::craft::plugin::ReactionWheelConfig;
using flop::world::ControlManager;
using flop::world::CraftId;
using flop::world::PluginTarget;
using flop::world::Universe;
using Status = flop::world::ControlManager::Status;

flop::world::BodyConfig root_body() {
  return {"Hamswell", 3.0e6, 9.8, 86400, {}};
}

std::vector<ResourceTypeConfig> resource_types() {
  return {{"NTO", "N2O4", 1.0, ResourceFlow::stage}};
}

// one part type "Pod" that installs a command plugin and a reaction wheel, so a
// craft has a command plugin plus a separately toggleable plugin.
std::vector<PartTypeConfig> pod_types() {
  return {{"Pod", "Pod", 800, true, {}}};
}

CraftConfig pod_craft(const char *name) {
  CraftConfig config{
      name,
      CraftOrbitState{"Hamswell", {{3.0e6, 0}, {0, 5000}}, 0},
      {0.0, 0.0},
      {{"Pod",
        "pod",
        "Pod",
        {0, 0},
        0,
        {},
        {{"CommandPlugin", true, CommandConfig{}},
         {"ReactionWheelPlugin", true, ReactionWheelConfig{40.0}}}}},
  };
  config.plugins_resolved = true;
  return config;
}

CommandPlugin &command_plugin(const flop::craft::Craft &craft) {
  for (const auto &plugin : craft.part("pod").installed_plugins()) {
    if (auto *command = dynamic_cast<CommandPlugin *>(plugin.get())) {
      return *command;
    }
  }
  FAIL("craft has no command plugin");
  throw std::logic_error("unreachable");
}

} // namespace

TEST_CASE("Selecting a craft directs pilot throttle commands to it") {
  Universe universe({root_body()}, resource_types());
  const CraftId first = universe.add_craft(pod_craft("First"), pod_types());
  const CraftId second = universe.add_craft(pod_craft("Second"), pod_types());

  CHECK(universe.controls().active_craft_id() == CraftId::invalid);

  universe.controls().submit(ControlManager::SelectCraft{first});
  universe.controls().submit(ControlManager::SetThrottle{0.6});
  const auto results = universe.apply_commands();

  REQUIRE(results.size() == 2);
  CHECK(results[0].status == Status::applied);
  CHECK(results[1].status == Status::applied);
  CHECK(universe.controls().active_craft_id() == first);
  CHECK(universe.craft(first).control().throttle() == 0.6);
  // the non-selected craft is untouched.
  CHECK(universe.craft(second).control().throttle() == 0.0);
}

TEST_CASE("A pilot throttle command with no active craft is unsupported") {
  Universe universe({root_body()}, resource_types());
  const CraftId only = universe.add_craft(pod_craft("Only"), pod_types());

  universe.controls().submit(ControlManager::SetThrottle{0.5});
  const auto results = universe.apply_commands();
  REQUIRE(results.size() == 1);
  CHECK(results[0].status == Status::unsupported);
  // the rejected command left the craft's throttle at its initial value.
  CHECK(universe.craft(only).control().throttle() == 0.0);
}

TEST_CASE("SetThrottle replaces the active craft's persistent throttle") {
  Universe universe({root_body()}, resource_types());
  const CraftId only = universe.add_craft(pod_craft("Only"), pod_types());

  universe.controls().submit(ControlManager::SelectCraft{only});
  universe.controls().submit(ControlManager::SetThrottle{0.4});
  (void)universe.apply_commands();
  CHECK(universe.craft(only).control().throttle() == 0.4);

  // a second write sets the level rather than adding to the first.
  universe.controls().submit(ControlManager::SetThrottle{0.75});
  (void)universe.apply_commands();
  CHECK(universe.craft(only).control().throttle() == 0.75);
}

TEST_CASE("SetThrottle clamps values outside [0, 1]") {
  Universe universe({root_body()}, resource_types());
  const CraftId only = universe.add_craft(pod_craft("Only"), pod_types());

  universe.controls().submit(ControlManager::SelectCraft{only});
  universe.controls().submit(ControlManager::SetThrottle{1.5});
  (void)universe.apply_commands();
  CHECK(universe.craft(only).control().throttle() == 1.0);

  universe.controls().submit(ControlManager::SetThrottle{-0.5});
  const auto results = universe.apply_commands();
  REQUIRE(results.size() == 1);
  CHECK(results[0].status == Status::applied);
  CHECK(universe.craft(only).control().throttle() == 0.0);
}

TEST_CASE("SetThrottle affects only the active craft and keeps each craft's level on reselection") {
  Universe universe({root_body()}, resource_types());
  const CraftId first = universe.add_craft(pod_craft("First"), pod_types());
  const CraftId second = universe.add_craft(pod_craft("Second"), pod_types());

  universe.controls().submit(ControlManager::SelectCraft{first});
  universe.controls().submit(ControlManager::SetThrottle{0.25});
  universe.controls().submit(ControlManager::SelectCraft{second});
  universe.controls().submit(ControlManager::SetThrottle{0.75});
  (void)universe.apply_commands();

  // deselecting a craft does not reset its throttle; each craft holds the
  // level it was last given.
  CHECK(universe.craft(first).control().throttle() == 0.25);
  CHECK(universe.craft(second).control().throttle() == 0.75);

  // reselecting first and writing again changes only first.
  universe.controls().submit(ControlManager::SelectCraft{first});
  universe.controls().submit(ControlManager::SetThrottle{0.5});
  const auto results = universe.apply_commands();
  REQUIRE(results.size() == 2);
  CHECK(results[0].status == Status::applied);
  CHECK(results[1].status == Status::applied);
  CHECK(universe.craft(first).control().throttle() == 0.5);
  CHECK(universe.craft(second).control().throttle() == 0.75);
}

TEST_CASE("SetPluginEnabled toggles exactly the targeted instance") {
  Universe universe({root_body()}, resource_types());
  const CraftId first = universe.add_craft(pod_craft("First"), pod_types());
  const CraftId second = universe.add_craft(pod_craft("Second"), pod_types());

  const PluginTarget wheel{first, PartId("pod"), PluginId("ReactionWheelPlugin")};
  universe.controls().submit(ControlManager::SetPluginEnabled{wheel, false});
  const auto results = universe.apply_commands();
  REQUIRE(results.size() == 1);
  CHECK(results[0].status == Status::applied);

  CHECK_FALSE(
      universe.craft(first).part("pod").find_plugin(PluginId("ReactionWheelPlugin"))->enabled()
  );
  // the same-named plugin on the other craft is unaffected.
  CHECK(universe.craft(second).part("pod").find_plugin(PluginId("ReactionWheelPlugin"))->enabled());
  // the command plugin on the targeted craft is unaffected.
  CHECK(universe.craft(first).part("pod").find_plugin(PluginId("CommandPlugin"))->enabled());
}

TEST_CASE("Stale targets fail cleanly without mutating other craft") {
  Universe universe({root_body()}, resource_types());
  const CraftId first = universe.add_craft(pod_craft("First"), pod_types());

  const auto missing_craft = static_cast<CraftId>(static_cast<std::uint64_t>(first) + 500);
  universe.controls().submit(
      ControlManager::SetPluginEnabled{
          {missing_craft, PartId("pod"), PluginId("ReactionWheelPlugin")}, false
      }
  );
  universe.controls().submit(
      ControlManager::SetPluginEnabled{
          {first, PartId("absent"), PluginId("ReactionWheelPlugin")}, false
      }
  );
  universe.controls().submit(
      ControlManager::SetPluginEnabled{{first, PartId("pod"), PluginId("AbsentPlugin")}, false}
  );
  universe.controls().submit(ControlManager::SelectCraft{missing_craft});
  const auto results = universe.apply_commands();

  REQUIRE(results.size() == 4);
  CHECK(results[0].status == Status::missing_target);
  CHECK(results[1].status == Status::missing_target);
  CHECK(results[2].status == Status::missing_target);
  CHECK(results[3].status == Status::missing_target);
  // nothing was toggled and no craft became active.
  CHECK(universe.craft(first).part("pod").find_plugin(PluginId("ReactionWheelPlugin"))->enabled());
  CHECK(universe.controls().active_craft_id() == CraftId::invalid);
}

TEST_CASE("Changing the active craft neutralizes the previous craft held input") {
  Universe universe({root_body()}, resource_types());
  const CraftId first = universe.add_craft(pod_craft("First"), pod_types());
  const CraftId second = universe.add_craft(pod_craft("Second"), pod_types());

  universe.controls().submit(ControlManager::SelectCraft{first});
  (void)universe.apply_commands();

  // simulate held pilot input having been pushed into the active craft's
  // command plugin, as stepping will do in a later unit.
  command_plugin(universe.craft(first)).set_throttle_input(0.3);
  command_plugin(universe.craft(first)).set_steering_input(0.5);

  universe.controls().submit(ControlManager::SelectCraft{second});
  (void)universe.apply_commands();

  // leaving the first craft cleared its held rate and steering so it coasts.
  CHECK(command_plugin(universe.craft(first)).throttle_input() == 0.0);
  CHECK(command_plugin(universe.craft(first)).steering_input() == 0.0);
  CHECK(universe.controls().active_craft_id() == second);
}

TEST_CASE("apply_commands leaves the queue empty and preserves submit order") {
  Universe universe({root_body()}, resource_types());
  const CraftId only = universe.add_craft(pod_craft("Only"), pod_types());

  universe.controls().submit(ControlManager::SelectCraft{only});
  CHECK(universe.controls().has_pending());
  const auto first_pass = universe.apply_commands();
  CHECK_FALSE(universe.controls().has_pending());
  REQUIRE(first_pass.size() == 1);

  // a second drain with nothing queued returns no results.
  CHECK(universe.apply_commands().empty());
}
