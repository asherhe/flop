#include <limits>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "flop/craft/plugins/part_plugin_config.hpp"
#include "flop/craft/plugins/behavior_plugins.hpp"
#include "flop/craft/plugins/part_plugin.hpp"
#include "flop/craft/plugins/part_plugin_factory.hpp"
#include "flop/craft/plugins/resource_plugin.hpp"
#include "flop/craft/resource_registry.hpp"
#include "flop/craft/resource_type_config.hpp"

namespace {
using Catch::Matchers::ContainsSubstring;
using flop::craft::plugin::CommandConfig;
using flop::craft::plugin::CommandPlugin;
using flop::craft::plugin::EngineConfig;
using flop::craft::plugin::EnginePlugin;
using flop::craft::plugin::LightConfig;
using flop::craft::plugin::LightPlugin;
using flop::craft::plugin::make_plugin;
using flop::craft::plugin::PartPlugin;
using flop::craft::plugin::PartPluginConfig;
using flop::craft::plugin::ReactionWheelConfig;
using flop::craft::plugin::ReactionWheelPlugin;
using flop::craft::plugin::ResourceConfig;
using flop::craft::ResourceFlow;
using flop::craft::plugin::ResourcePlugin;
using flop::craft::ResourceRegistry;
using flop::craft::plugin::ResourceSlot;
using flop::craft::plugin::SpritePlugin;
using flop::craft::plugin::SpriteRendererConfig;

ResourceRegistry registry() {
  return ResourceRegistry(
      {{"NTO", "N2O4", 1.0, ResourceFlow::stage}, {"Fuel", "Hydrazine", 2.0, ResourceFlow::stage}}
  );
}

const ResourceSlot &slot(const ResourcePlugin &plugin, std::string_view resource_id) {
  const auto *found = plugin.find_slot(resource_id);
  REQUIRE(found != nullptr);
  return *found;
}
} // namespace

TEST_CASE("ResourcePlugin builds one slot per configured resource in sorted order") {
  const ResourcePlugin plugin(
      {{{"NTO", 3600}, {"Fuel", 1000}}, {{"NTO", 1800}, {"Fuel", 500}}, {{"NTO", 2}}},
      registry(),
      true
  );
  REQUIRE(plugin.slots().size() == 2);
  // std::map sorted-key order places "Fuel" before "NTO".
  CHECK(plugin.slots()[0].resource_id == "Fuel");
  CHECK(plugin.slots()[1].resource_id == "NTO");
  CHECK(slot(plugin, "NTO").amount_units == 1800);
  CHECK(slot(plugin, "NTO").max_amount_units == 3600);
  CHECK(slot(plugin, "NTO").priority == 2);
  CHECK(plugin.enabled());
}

TEST_CASE("ResourcePlugin defaults omitted starting amount and priority to zero") {
  const ResourcePlugin plugin({{{"NTO", 3600}}, {}, {}}, registry(), true);
  const auto &nto = slot(plugin, "NTO");
  CHECK(nto.amount_units == 0);
  CHECK(nto.priority == 0);
  CHECK(nto.max_amount_units == 3600);
}

TEST_CASE("ResourcePlugin clamps starting amounts into the capacity range") {
  const ResourcePlugin plugin(
      {{{"NTO", 3600}, {"Fuel", 1000}}, {{"NTO", 5000}, {"Fuel", -200}}, {}}, registry(), true
  );
  CHECK(slot(plugin, "NTO").amount_units == 3600);
  CHECK(slot(plugin, "Fuel").amount_units == 0);
}

TEST_CASE("ResourcePlugin records the enabled flag") {
  const ResourcePlugin plugin({{{"NTO", 3600}}, {{"NTO", 100}}, {}}, registry(), false);
  CHECK_FALSE(plugin.enabled());
}

TEST_CASE("ResourcePlugin rejects unknown resource ids and non-finite capacities") {
  const auto reg = registry();

  CHECK_THROWS_WITH(
      ResourcePlugin({{{"Xenon", 10}}, {}, {}}, reg, true), ContainsSubstring("Unknown resource ID")
  );
  CHECK_THROWS_WITH(
      ResourcePlugin({{{"NTO", std::numeric_limits<double>::infinity()}}, {}, {}}, reg, true),
      ContainsSubstring("capacity must be finite")
  );
  CHECK_THROWS_WITH(
      ResourcePlugin({{{"NTO", -1}}, {}, {}}, reg, true),
      ContainsSubstring("capacity must be finite")
  );
  CHECK_THROWS_WITH(
      ResourcePlugin(
          {{{"NTO", 10}}, {{"NTO", std::numeric_limits<double>::infinity()}}, {}}, reg, true
      ),
      ContainsSubstring("amount must be finite")
  );
}

TEST_CASE("make_plugin builds the runtime type matching each config alternative") {
  const auto reg = registry();

  const auto command = make_plugin({"Command", true, CommandConfig{}}, reg);
  CHECK(dynamic_cast<CommandPlugin *>(command.get()) != nullptr);

  const auto wheel = make_plugin({"ReactionWheel", true, ReactionWheelConfig{}}, reg);
  CHECK(dynamic_cast<ReactionWheelPlugin *>(wheel.get()) != nullptr);

  const auto engine = make_plugin({"Engine", true, EngineConfig{}}, reg);
  CHECK(dynamic_cast<EnginePlugin *>(engine.get()) != nullptr);

  const auto container =
      make_plugin({"ResourceContainer", true, ResourceConfig{{{"NTO", 10}}, {}, {}}}, reg);
  CHECK(dynamic_cast<ResourcePlugin *>(container.get()) != nullptr);

  const auto sprite = make_plugin({"Sprite", true, SpriteRendererConfig{}}, reg);
  CHECK(dynamic_cast<SpritePlugin *>(sprite.get()) != nullptr);

  const auto light = make_plugin({"Light", true, LightConfig{}}, reg);
  CHECK(dynamic_cast<LightPlugin *>(light.get()) != nullptr);
}

TEST_CASE("make_plugin seeds the enabled flag from the config") {
  const auto reg = registry();
  const auto disabled = make_plugin({"Engine", false, EngineConfig{}}, reg);
  CHECK_FALSE(disabled->enabled());
  const auto enabled = make_plugin({"Engine", true, EngineConfig{}}, reg);
  CHECK(enabled->enabled());
}

TEST_CASE("make_plugin propagates resource container validation errors") {
  const auto reg = registry();
  CHECK_THROWS_WITH(
      make_plugin({"ResourceContainer", true, ResourceConfig{{{"Xenon", 10}}, {}, {}}}, reg),
      ContainsSubstring("Unknown resource ID")
  );
}
