#include <algorithm>
#include <filesystem>
#include <variant>
#include <yaml-cpp/yaml.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "flop/config/decode/body.hpp"
#include "flop/config/decode/craft.hpp"
#include "flop/config/decode/part.hpp"
#include "flop/config/decode/part_plugin.hpp"
#include "flop/craft/craft.hpp"
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
  const auto sprite =
      std::ranges::find(pod.plugins, "SpriteRendererPlugin", &flop::craft::PartPluginConfig::name);
  REQUIRE(sprite != pod.plugins.end());
  const auto &sprite_config = std::get<flop::craft::SpriteRendererConfig>(sprite->values);
  CHECK(sprite_config.sprite == "assets/parts/TestingCommandPod/TestingCommandPod.png");
  CHECK(sprite_config.material.path == "assets/shared/materials/part.mat");
  CHECK(sprite_config.material.properties.at("_EmissionTex").type == "Texture2D");
  CHECK(
      sprite_config.material.properties.at("_EmissionTex").value ==
      "assets/parts/TestingCommandPod/TestingCommandPod-Light.png"
  );
  const auto light =
      std::ranges::find(pod.plugins, "LightPlugin", &flop::craft::PartPluginConfig::name);
  REQUIRE(light != pod.plugins.end());
  const auto &light_config = std::get<flop::craft::LightConfig>(light->values);
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
  auto sprite =
      *std::ranges::find(pod.plugins, "SpriteRendererPlugin", &flop::craft::PartPluginConfig::name);
  auto light = *std::ranges::find(pod.plugins, "LightPlugin", &flop::craft::PartPluginConfig::name);
  flop::config::apply_part_plugin_fields(
      sprite, ConfigView(YAML::Load("sprite: replacement.png"), "assets/spacecraft/test.yaml")
  );
  flop::config::apply_part_plugin_fields(
      light, ConfigView(YAML::Load("intensity: 2"), "assets/spacecraft/test.yaml")
  );
  CHECK(std::get<flop::craft::SpriteRendererConfig>(sprite.values).sprite == "replacement.png");
  CHECK(
      std::get<flop::craft::SpriteRendererConfig>(sprite.values).material.path ==
      "assets/shared/materials/part.mat"
  );
  CHECK(std::get<flop::craft::LightConfig>(light.values).intensity == 2);
  CHECK(std::get<flop::craft::LightConfig>(light.values).color == "#FFECB1");
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
  const auto &engine = std::get<flop::craft::EngineConfig>(plugin.values);
  CHECK(engine.isp_sec == 200);
  CHECK(engine.thrust_newtons == 7000);
  CHECK(engine.thrust_direction.y == 1);
  CHECK(engine.propellant_ratio.at("Fuel") == 1);
}

TEST_CASE("Shipped craft assembles its three decoded part types") {
  const AssetResolver resolver(std::filesystem::path(FLOP_TEST_ASSET_ROOT));
  const auto sun = load_config<flop::world::BodyConfig>(
      resolver, AssetPath("assets/celestialbodies/Sun/Sun.yaml")
  );
  const auto hamswell = load_config<flop::world::BodyConfig>(
      resolver, AssetPath("assets/celestialbodies/Hamswell/Hamswell.yaml")
  );
  const flop::world::Universe universe({sun, hamswell});
  const std::vector<PartTypeConfig> types{
      load_part_type(resolver, "TestingCommandPod"),
      load_part_type(resolver, "TestingTank"),
      load_part_type(resolver, "TestingEngine"),
  };
  const auto config = flop::config::load_craft_config(
      resolver, AssetPath("assets/spacecraft/OrbitalTestCraft.yaml"), types
  );
  const flop::craft::Craft craft(config, universe.reference_frames(), types);
  REQUIRE(craft.parts().size() == 3);
  CHECK(craft.part("TestingEngine_3").dry_mass_kg == 1200);
  CHECK(craft.part("TestingTank_2").attachments.size() == 2);
  const auto &tank_plugins = craft.part("TestingTank_2").plugins;
  const auto tank_plugin = std::ranges::find(
      tank_plugins, "ResourceContainerPlugin", &flop::craft::PartPluginConfig::name
  );
  REQUIRE(tank_plugin != tank_plugins.end());
  const auto &tank_state = std::get<flop::craft::ResourceContainerConfig>(tank_plugin->values);
  CHECK(tank_state.max_amount_units.at("NTO") == 3600);
  CHECK(tank_state.amount_units.at("NTO") == 3600);
  CHECK(tank_state.priority.at("Hydrazine") == 0);
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
      placement.plugins, "ReactionWheelPlugin", &flop::craft::PartPluginConfig::name
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
      "Test", {"Hamswell", 1.73e10, 0.08, 1, 1, 0}, {0, 0}, {placement}, true
  };
  const flop::craft::Craft craft(config, universe.reference_frames(), {pod});
  const auto &plugins = craft.part("pod_1").plugins;
  const auto wheel =
      std::ranges::find(plugins, "ReactionWheelPlugin", &flop::craft::PartPluginConfig::name);
  REQUIRE(wheel != plugins.end());
  CHECK_FALSE(wheel->enabled);
  CHECK(std::get<flop::craft::ReactionWheelConfig>(wheel->values).torque_newton_meters == 7000);
}
