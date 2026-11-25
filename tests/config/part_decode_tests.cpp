#include <algorithm>
#include <cmath>
#include <filesystem>
#include <variant>
#include <yaml-cpp/yaml.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "flop/config/decode/body.hpp"
#include "flop/config/decode/craft.hpp"
#include "flop/config/decode/part.hpp"
#include "flop/config/decode/part_plugin.hpp"
#include "flop/config/decode/resources.hpp"
#include "flop/config/decode/save.hpp"
#include "flop/craft/craft.hpp"
#include "flop/craft/plugins/plugins.hpp"
#include "flop/world/universe.hpp"

namespace {
using Catch::Matchers::ContainsSubstring;
using flop::config::AssetPath;
using flop::config::AssetResolver;
using flop::config::ConfigDecoder;
using flop::config::ConfigView;
using flop::config::load_config;
using flop::config::load_part_type;
using flop::craft::PartTypeConfig;
} // namespace

TEST_CASE("Shipped part types decode dry mass and attachment nodes") {
  const AssetResolver resolver(std::filesystem::path(FLOP_TEST_ASSET_ROOT));
  const auto pod = load_part_type(resolver, "TestingCommandPod");
  const auto tank = load_part_type(resolver, "TestingTank");
  const auto engine = load_part_type(resolver, "TestingEngine");
  CHECK(pod.type_id == "TestingCommandPod");
  CHECK(pod.mass_kg == 800);
  CHECK(tank.mass_kg == 600);
  CHECK(engine.mass_kg == 1200);
  REQUIRE(engine.attachment_nodes.size() == 2);
  CHECK(engine.attachment_nodes[0].position_meters.y == 0.75);
  const auto sprite = std::ranges::find(
      pod.plugins, "SpritePlugin", &flop::craft::plugin::PartPluginConfig::name
  );
  REQUIRE(sprite != pod.plugins.end());
  const auto &sprite_config = std::get<flop::craft::plugin::SpriteConfig>(sprite->values);
  CHECK(sprite_config.sprite == "assets/parts/TestingCommandPod/TestingCommandPod.png");
  CHECK(sprite_config.size_meters.x == 2.0);
  CHECK(sprite_config.size_meters.y == 2.0);
  CHECK(sprite_config.pivot.x == 0.5);
  CHECK(sprite_config.pivot.y == 0.5);
  CHECK(sprite_config.material.shader == "craft/sprite");
  CHECK(
      sprite_config.material.emission_map ==
      "assets/parts/TestingCommandPod/TestingCommandPod-Light.png"
  );
  const auto light =
      std::ranges::find(pod.plugins, "LightPlugin", &flop::craft::plugin::PartPluginConfig::name);
  REQUIRE(light != pod.plugins.end());
  const auto &light_config = std::get<flop::craft::plugin::LightConfig>(light->values);
  CHECK(light_config.light_position_meters.y == -0.125);
  CHECK(light_config.color == "#FFECB1");
  CHECK(light_config.intensity == 0.13);
  CHECK(light_config.inner_radius_meters == 0.25);
  CHECK(light_config.outer_radius_meters == 1.68);
  CHECK(light_config.falloff == 0.89);
}

TEST_CASE("Visual plugin overrides update their own config types") {
  const AssetResolver resolver(std::filesystem::path(FLOP_TEST_ASSET_ROOT));
  const auto pod = load_part_type(resolver, "TestingCommandPod");
  auto sprite = *std::ranges::find(
      pod.plugins, "SpritePlugin", &flop::craft::plugin::PartPluginConfig::name
  );
  auto light =
      *std::ranges::find(pod.plugins, "LightPlugin", &flop::craft::plugin::PartPluginConfig::name);
  flop::config::apply_part_plugin_fields(
      sprite, ConfigView(YAML::Load("sprite: replacement.png"), "assets/spacecraft/test.yaml")
  );
  flop::config::apply_part_plugin_fields(
      light, ConfigView(YAML::Load("intensity: 2"), "assets/spacecraft/test.yaml")
  );
  CHECK(
      std::get<flop::craft::plugin::SpriteConfig>(sprite.values).sprite == "replacement.png"
  );
  CHECK(
      std::get<flop::craft::plugin::SpriteConfig>(sprite.values).material.shader ==
      "craft/sprite"
  );
  CHECK(std::get<flop::craft::plugin::LightConfig>(light.values).intensity == 2);
  CHECK(std::get<flop::craft::plugin::LightConfig>(light.values).color == "#FFECB1");
  CHECK(std::get<flop::craft::plugin::SpriteConfig>(sprite.values).size_meters.y == 2.0);
}

TEST_CASE("Sprite materials reject unsupported shaders and properties") {
  using flop::config::ConfigView;
  CHECK_THROWS(flop::config::detail::decode_material(
      ConfigView(YAML::Load("shader: craft/unknown"), "assets/parts/test.yaml")));
  CHECK_THROWS(flop::config::detail::decode_material(
      ConfigView(YAML::Load("shader: craft/sprite\nproperties: {unknown: value}"), "assets/parts/test.yaml")));
  CHECK_THROWS(flop::config::detail::decode_material(
      ConfigView(YAML::Load("shader: craft/sprite\nproperties: {normal_map: [a, b]}"), "assets/parts/test.yaml")));
}

TEST_CASE("Sprite geometry rejects invalid dimensions and pivots") {
  const AssetResolver resolver(std::filesystem::path(FLOP_TEST_ASSET_ROOT));
  const auto pod = load_part_type(resolver, "TestingCommandPod");
  auto sprite = *std::ranges::find(
      pod.plugins, "SpritePlugin", &flop::craft::plugin::PartPluginConfig::name
  );
  CHECK_THROWS_WITH(
      flop::config::apply_part_plugin_fields(
          sprite, ConfigView(YAML::Load("size_meters: {x: 0, y: 2}"), "part.yaml")
      ), ContainsSubstring("[size_meters.x]")
  );
  CHECK_THROWS_WITH(
      flop::config::apply_part_plugin_fields(
          sprite, ConfigView(YAML::Load("pivot: {x: 0.5, y: 1.1}"), "part.yaml")
      ), ContainsSubstring("[pivot.y]")
  );
}

TEST_CASE("Part type decoder reports invalid mass at the authored field") {
  const ConfigView source(
      YAML::Load("name: Pod\nmass: -1\ncrossfeed: true\nattachment_nodes: []\nplugins: {}\n"),
      "assets/parts/Pod/Pod.yaml"
  );
  CHECK_THROWS_WITH(ConfigDecoder<PartTypeConfig>::decode(source), ContainsSubstring("[mass]"));
}

TEST_CASE("Flight plugin decoder rejects unknown override fields") {
  auto plugin = flop::config::decode_part_plugin(
      "ReactionWheelPlugin", ConfigView(YAML::Load("torque: 5"), "assets/parts/Pod/Pod.yaml")
  );
  const ConfigView source(
      YAML::Load("torqe: 7"), "assets/spacecraft/test.yaml", "parts[0].plugins.ReactionWheelPlugin"
  );
  CHECK_THROWS_WITH(
      flop::config::apply_part_plugin_fields(plugin, source),
      ContainsSubstring("[parts[0].plugins.ReactionWheelPlugin.torqe]")
  );
}

TEST_CASE("Plugin definitions still require their physical fields") {
  const ConfigView source(
      YAML::Load("enabled: true"), "assets/parts/Pod/Pod.yaml", "plugins.ReactionWheelPlugin"
  );
  CHECK_THROWS_WITH(
      flop::config::decode_part_plugin("ReactionWheelPlugin", source),
      ContainsSubstring("[plugins.ReactionWheelPlugin.torque]")
  );
}

TEST_CASE("Engine definitions require every field and overrides retain omitted values") {
  const ConfigView incomplete(
      YAML::Load("isp: 200\nthrust: 5\nthrust_dir: {x: 0, y: 1}\n"),
      "assets/parts/Engine/Engine.yaml",
      "plugins.EnginePlugin"
  );
  CHECK_THROWS_WITH(
      flop::config::decode_part_plugin("EnginePlugin", incomplete),
      ContainsSubstring("[plugins.EnginePlugin.propellant_ratio]")
  );

  auto plugin = flop::config::decode_part_plugin(
      "EnginePlugin",
      ConfigView(
          YAML::Load(
              "isp: 200\nthrust: 5\nthrust_dir: {x: 0, y: 1}\n"
              "propellant_ratio: {Fuel: 1}\n"
          ),
          "assets/parts/Engine/Engine.yaml"
      )
  );
  flop::config::apply_part_plugin_fields(
      plugin, ConfigView(YAML::Load("thrust: 7"), "assets/spacecraft/test.yaml")
  );
  const auto &engine = std::get<flop::craft::plugin::EngineConfig>(plugin.values);
  CHECK(engine.isp_sec == 200);
  CHECK(engine.thrust_newtons == 7000);
  CHECK(engine.thrust_direction.y == 1);
  CHECK(engine.propellant_ratio.at("Fuel") == 1);
}

TEST_CASE("A saved craft instance assembles its three decoded part types") {
  const AssetResolver resolver(std::filesystem::path(FLOP_TEST_ASSET_ROOT));
  const auto sun = load_config<flop::world::BodyConfig>(
      resolver, AssetPath("assets/celestialbodies/Sun/Sun.yaml")
  );
  const auto hamswell = load_config<flop::world::BodyConfig>(
      resolver, AssetPath("assets/celestialbodies/Hamswell/Hamswell.yaml")
  );
  const auto resources = load_config<std::vector<flop::craft::ResourceTypeConfig>>(
      resolver, AssetPath("assets/resources.yaml")
  );
  const flop::world::Universe universe({sun, hamswell}, resources);
  const std::vector<PartTypeConfig> types{
      load_part_type(resolver, "TestingCommandPod"),
      load_part_type(resolver, "TestingTank"),
      load_part_type(resolver, "TestingEngine"),
  };
  const auto saved =
      flop::config::load_save(resolver, AssetPath("assets/saves/orbital_test.yaml"), types);
  REQUIRE(saved.size() == 1);
  const auto &config = saved.front();
  REQUIRE(config.orbit.has_value());
  CHECK(config.orbit->parent_name == "Hamswell");

  flop::craft::Craft craft(config, universe, types);
  REQUIRE(craft.parts().size() == 3);
  CHECK(craft.part("TestingEngine_3").dry_mass_kg == 1200);
  CHECK(craft.part("TestingTank_2").attachments.size() == 2);
  const auto &tank_plugins = craft.part("TestingTank_2").plugins;
  const auto tank_plugin = std::ranges::find(
      tank_plugins, "ResourceContainerPlugin", &flop::craft::plugin::PartPluginConfig::name
  );
  REQUIRE(tank_plugin != tank_plugins.end());
  const auto &tank_state = std::get<flop::craft::plugin::ResourceConfig>(tank_plugin->values);
  // capacity comes from the part type; the live balance comes from the save.
  CHECK(tank_state.max_amount_units.at("NTO") == 3600);
  CHECK(tank_state.amount_units.at("NTO") == 1800);
  CHECK(tank_state.priority.at("Hydrazine") == 0);

  // the craft is seeded directly from the saved cartesian state.
  CHECK(craft.orbit_state().state.position.x == config.orbit->state.position.x);
  CHECK(craft.orbit_state().state.velocity.y == config.orbit->state.velocity.y);
  CHECK(craft.orbit_state().epoch == 0);

  // a short powered step through the loaded craft: command input flows into the
  // control state, the engine burns, mass drops, motion stays finite, and the
  // orbit reseeds at the step endpoint.
  flop::craft::plugin::CommandPlugin *command = nullptr;
  for (const auto &plugin : craft.part("TestingCommandPod_1").installed_plugins()) {
    if (auto *found = dynamic_cast<flop::craft::plugin::CommandPlugin *>(plugin.get())) {
      command = found;
    }
  }
  REQUIRE(command != nullptr);
  craft.control().set_throttle(1.0); // drive the throttle fully open this step.

  const double mass_before = craft.mass_properties().total_mass_kg;
  const auto seed_before = craft.orbit_state();
  craft.step(1.0, 1.0);

  CHECK(craft.control().throttle() == 1.0);
  CHECK(craft.mass_properties().total_mass_kg < mass_before);
  CHECK(std::isfinite(craft.orbit_state().state.position.x));
  CHECK(std::isfinite(craft.orbit_state().state.position.y));
  CHECK(std::isfinite(craft.orbit_state().state.velocity.x));
  CHECK(std::isfinite(craft.orbit_state().state.velocity.y));
  CHECK(std::isfinite(craft.newtonian().angle_rad()));
  CHECK(craft.orbit_state().epoch == 1.0);
  CHECK(craft.orbit_state().state.velocity != seed_before.state.velocity);
}

TEST_CASE("Craft plugin fields override matching part type fields") {
  const AssetResolver resolver(std::filesystem::path(FLOP_TEST_ASSET_ROOT));
  auto pod = load_part_type(resolver, "TestingCommandPod");
  const ConfigView part_view(
      YAML::Load(
          "type: TestingCommandPod\nid: pod_1\nname: Pod\n"
          "transform: {pos: {x: 0, y: 0}, rot: 0}\nattachments: []\n"
          "plugins: {ReactionWheelPlugin: {enabled: false, torque: 7}}\n"
      ),
      "assets/spacecraft/test.yaml",
      "parts[0]"
  );
  auto placement = ConfigDecoder<flop::craft::PartConfig>::decode(part_view);
  placement.plugins = pod.plugins;
  auto wheel_config = std::ranges::find(
      placement.plugins, "ReactionWheelPlugin", &flop::craft::plugin::PartPluginConfig::name
  );
  REQUIRE(wheel_config != placement.plugins.end());
  flop::config::apply_part_plugin_fields(
      *wheel_config, part_view.required("plugins").required("ReactionWheelPlugin")
  );
  const auto sun = load_config<flop::world::BodyConfig>(
      resolver, AssetPath("assets/celestialbodies/Sun/Sun.yaml")
  );
  const auto hamswell = load_config<flop::world::BodyConfig>(
      resolver, AssetPath("assets/celestialbodies/Hamswell/Hamswell.yaml")
  );
  const flop::world::Universe universe({sun, hamswell});
  flop::craft::CraftConfig config{
      "Test",
      flop::craft::CraftOrbitState{"Hamswell", {{3.0e6, 0}, {0, 5000}}, 0},
      {0, 0},
      {placement},
      true
  };
  const flop::craft::Craft craft(config, universe, {pod});
  const auto &plugins = craft.part("pod_1").plugins;
  const auto wheel = std::ranges::find(
      plugins, "ReactionWheelPlugin", &flop::craft::plugin::PartPluginConfig::name
  );
  REQUIRE(wheel != plugins.end());
  CHECK_FALSE(wheel->enabled);
  CHECK(
      std::get<flop::craft::plugin::ReactionWheelConfig>(wheel->values).torque_newton_meters == 7000
  );
}
