// Strong craft identity: PartId/PluginId defaulting and scoped uniqueness.
// install_plugins assigns a stable id to each installed plugin (defaulting to
// its authored plugin name) and rejects a part that would install two plugins
// resolving to the same id. Craft assembly rejects duplicate part ids.

#include <limits>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "flop/craft/craft.hpp"
#include "flop/craft/ids.hpp"
#include "flop/craft/parts/part.hpp"
#include "flop/craft/plugins/config.hpp"
#include "flop/craft/plugins/part_plugin.hpp"
#include "flop/craft/plugins/plugins.hpp"
#include "flop/craft/resource_registry.hpp"
#include "flop/craft/resource_type_config.hpp"

#include "support/stub_universe_context.hpp"

namespace {

using Catch::Matchers::ContainsSubstring;
using flop::craft::Part;
using flop::craft::PartId;
using flop::craft::PluginId;
using flop::craft::ResourceFlow;
using flop::craft::ResourceRegistry;
using flop::craft::plugin::CommandConfig;
using flop::craft::plugin::PartPluginConfig;
using flop::craft::plugin::ReactionWheelConfig;

ResourceRegistry registry() {
  return ResourceRegistry({{"NTO", "N2O4", 1.0, ResourceFlow::stage}});
}

flop::orbit::RefFrameManager frames() {
  return flop::orbit::RefFrameManager(
      {{"Hamswell", 8.82e13, {}, {}, std::numeric_limits<double>::infinity()}}
  );
}

} // namespace

TEST_CASE("PartId and PluginId compare by value") {
  CHECK(PartId("pod") == PartId("pod"));
  CHECK_FALSE(PartId("pod") == PartId("tank"));
  CHECK(PluginId("CommandPlugin") == PluginId("CommandPlugin"));
  CHECK(PartId("").empty());
  CHECK_FALSE(PartId("pod").empty());
}

TEST_CASE("Installed plugins default their id to the authored plugin name") {
  Part part;
  part.id = "pod";
  part.plugins = {
      PartPluginConfig{"CommandPlugin", true, CommandConfig{}},
      PartPluginConfig{"ReactionWheelPlugin", true, ReactionWheelConfig{40.0}},
  };
  part.install_plugins(registry(), 0);

  CHECK(part.part_id() == PartId("pod"));
  REQUIRE(part.installed_plugins().size() == 2);
  CHECK(part.installed_plugins()[0]->id() == PluginId("CommandPlugin"));
  CHECK(part.installed_plugins()[1]->id() == PluginId("ReactionWheelPlugin"));
  CHECK(part.find_plugin(PluginId("CommandPlugin")) == part.installed_plugins()[0].get());
  CHECK(part.find_plugin(PluginId("ReactionWheelPlugin")) == part.installed_plugins()[1].get());
  CHECK(part.find_plugin(PluginId("Absent")) == nullptr);
}

TEST_CASE("An explicit plugin id overrides the plugin name") {
  Part part;
  part.id = "pod";
  PartPluginConfig command{"CommandPlugin", true, CommandConfig{}};
  command.id = "primary_command";
  part.plugins = {command};
  part.install_plugins(registry(), 0);

  REQUIRE(part.installed_plugins().size() == 1);
  CHECK(part.installed_plugins()[0]->id() == PluginId("primary_command"));
  CHECK(part.find_plugin(PluginId("primary_command")) != nullptr);
  CHECK(part.find_plugin(PluginId("CommandPlugin")) == nullptr);
}

TEST_CASE("typed plugin lookup returns the first matching plugin with const access") {
  Part part;
  part.id = "pod";
  PartPluginConfig a{"CommandPlugin", false, CommandConfig{}};
  PartPluginConfig b{"CommandPlugin", true, CommandConfig{}};
  a.id = "first";
  b.id = "second";
  part.plugins = {a, b};
  part.install_plugins(registry(), 0);
  using flop::craft::plugin::CommandPlugin;
  using flop::craft::plugin::SpritePlugin;
  REQUIRE(part.get_plugin<CommandPlugin>());
  CHECK(part.get_plugin<CommandPlugin>()->id() == PluginId("first"));
  CHECK(part.get_plugin<SpritePlugin>() == nullptr);
  const Part &const_part = part;
  CHECK(const_part.get_plugin<CommandPlugin>() == part.get_plugin<CommandPlugin>());
  CHECK(const_part.get_plugin<SpritePlugin>() == nullptr);
}

TEST_CASE("A part rejects two plugins that resolve to the same id") {
  Part part;
  part.id = "pod";
  PartPluginConfig first{"CommandPlugin", true, CommandConfig{}};
  PartPluginConfig second{"ReactionWheelPlugin", true, ReactionWheelConfig{40.0}};
  second.id = "CommandPlugin"; // collides with the first plugin's default id.
  part.plugins = {first, second};

  CHECK_THROWS_WITH(
      part.install_plugins(registry(), 0),
      ContainsSubstring("duplicate plugin id") && ContainsSubstring("CommandPlugin")
  );
}

TEST_CASE("Craft assembly rejects duplicate part ids") {
  using flop::craft::Craft;
  using flop::craft::CraftConfig;
  using flop::craft::PartTypeConfig;

  CraftConfig config{
      "Dupe",
      flop::craft::CraftOrbitState{"Hamswell", {{3.0e6, 0}, {0, 5000}}, 0},
      {0.0, 0.0},
      {
          {"Pod", "same", "Pod A", {0, 0}, 0, {}, {}},
          {"Pod", "same", "Pod B", {0, -2}, 0, {}, {}},
      },
  };
  config.plugins_resolved = true;
  const std::vector<PartTypeConfig> types{{"Pod", "Pod", 800, true, {}}};
  const flop::testing::StubUniverseContext context(frames(), {{"NTO", "N2O4", 1.0, ResourceFlow::stage}});

  CHECK_THROWS_WITH(
      Craft(config, context, types), ContainsSubstring("Duplicate craft part ID")
  );
}
