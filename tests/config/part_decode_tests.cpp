#include <algorithm>
#include <cmath>
#include <filesystem>
#include <variant>
#include <yaml-cpp/yaml.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
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
  const auto engine_light = std::ranges::find(
      engine.plugins, "LightPlugin", &flop::craft::plugin::PartPluginConfig::name);
  REQUIRE(engine_light != engine.plugins.end());
  const auto &engine_light_config = std::get<flop::craft::plugin::LightConfig>(engine_light->values);
  CHECK(engine_light_config.light_position_meters.y == -0.75);
  CHECK(engine_light_config.strength == 0.0);
  CHECK(engine_light_config.outer_radius_meters == 4.5);
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
  CHECK(sprite_config.material.shader == "craft/lit_sprite");
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
  CHECK(light_config.strength == 1.0);
}

TEST_CASE("light and engine flicker validation reports source fields") {
  auto light = flop::config::decode_part_plugin("LightPlugin", ConfigView(YAML::Load(
      "light_position: {x: 0, y: 0}\ncolor: '#ffffff'\nintensity: 1\n"
      "inner_radius: 0\nouter_radius: 1\nfalloff: 0.5\n"), "part.yaml", "plugins.LightPlugin"));
  for (const auto *invalid : {"outer_radius: -1", "intensity: -1", "falloff: 1.1", "color: nope"}) {
    auto copy = light;
    CHECK_THROWS(flop::config::apply_part_plugin_fields(copy,
        ConfigView(YAML::Load(invalid), "save.yaml", "plugins.LightPlugin")));
  }
  CHECK_THROWS_WITH(flop::config::apply_part_plugin_fields(light,
      ConfigView(YAML::Load("inner_radius: 2"), "save.yaml", "plugins.LightPlugin")),
      ContainsSubstring("[plugins.LightPlugin.inner_radius]"));
  auto engine = flop::config::decode_part_plugin("EnginePlugin", ConfigView(YAML::Load(
      "isp: 100\nthrust: 1\nthrust_dir: {x: 0, y: 1}\npropellant_ratio: {NTO: 1}\n"),
      "part.yaml", "plugins.EnginePlugin"));
  CHECK_THROWS_WITH(flop::config::apply_part_plugin_fields(engine,
      ConfigView(YAML::Load("flicker_min: 2"), "save.yaml", "plugins.EnginePlugin")),
      ContainsSubstring("[plugins.EnginePlugin.flicker_min]"));
}

TEST_CASE("light initial strength is configurable and rejects invalid overrides") {
  auto light = flop::config::decode_part_plugin(
      "LightPlugin", ConfigView(YAML::Load(
          "light_position: {x: 0, y: 0}\ncolor: '#ffffff'\nintensity: 1\n"
          "inner_radius: 0\nouter_radius: 1\nfalloff: 0.5\nstrength: 0\n"),
          "part.yaml", "plugins.LightPlugin"));
  CHECK(std::get<flop::craft::plugin::LightConfig>(light.values).strength == 0.0);
  CHECK_THROWS_WITH(flop::config::apply_part_plugin_fields(light,
      ConfigView(YAML::Load("strength: -1"), "save.yaml", "plugins.LightPlugin")),
      ContainsSubstring("[plugins.LightPlugin.strength]"));
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
      sprite, ConfigView(YAML::Load("sprite: assets/replacement.png"), "assets/spacecraft/test.yaml")
  );
  flop::config::apply_part_plugin_fields(
      light, ConfigView(YAML::Load("intensity: 2"), "assets/spacecraft/test.yaml")
  );
  CHECK(
      std::get<flop::craft::plugin::SpriteConfig>(sprite.values).sprite == "assets/replacement.png"
  );
  CHECK(std::get<flop::craft::plugin::SpriteConfig>(sprite.values).sprite_source.address ==
        "assets/spacecraft/test.yaml");
  CHECK(std::get<flop::craft::plugin::SpriteConfig>(sprite.values).sprite_source.key == "sprite");
  CHECK(
      std::get<flop::craft::plugin::SpriteConfig>(sprite.values).material.shader ==
      "craft/lit_sprite"
  );
  CHECK(std::get<flop::craft::plugin::LightConfig>(light.values).intensity == 2);
  CHECK(std::get<flop::craft::plugin::LightConfig>(light.values).color == "#FFECB1");
  CHECK(std::get<flop::craft::plugin::SpriteConfig>(sprite.values).size_meters.y == 2.0);
}

TEST_CASE("Particle plugin decodes ranges and rejects invalid overrides at their source") {
  auto particle = flop::config::decode_part_plugin(
      "ParticlePlugin", ConfigView(YAML::Load(
          "line_pos: {x: 0, y: -1}\nline_width: 1\nline_angle: 180\nspread_angle: 30\n"
          "lifetime: [0.2, 0.3]\nrate: [300, 500]\n"
          "start: {size: [0.1, 0.4], speed: [20, 30]}\n"
          "life: {color: {color_keys: {0: '#ffffff'}, alpha_keys: {0: 0, 1: 1}}, size: {0: 1}}\n"),
          "part.yaml", "plugins.ParticlePlugin"));
  const auto &config = std::get<flop::craft::plugin::ParticleConfig>(particle.values);
  CHECK(config.rate_per_sec[0] == 300);
  CHECK(config.line_angle_rad == Catch::Approx(std::acos(-1.0)));
  CHECK(config.start_speed_m_per_s[1] == 30);
  CHECK(config.color.evaluate(0).r == 1.0);
  CHECK_THROWS_WITH(flop::config::apply_part_plugin_fields(particle,
      ConfigView(YAML::Load("lifetime: [0, 1]"), "save.yaml", "plugins.ParticlePlugin")),
      ContainsSubstring("[plugins.ParticlePlugin.lifetime]"));
  CHECK_THROWS_WITH(flop::config::apply_part_plugin_fields(particle,
      ConfigView(YAML::Load("spread_angle: -1"), "save.yaml", "plugins.ParticlePlugin")),
      ContainsSubstring("[plugins.ParticlePlugin.spread_angle]"));
  CHECK_THROWS_WITH(flop::config::apply_part_plugin_fields(particle,
      ConfigView(YAML::Load("life: {color: {color_keys: {0: '#GG0000'}, alpha_keys: {0: 1}}, size: {0: 1}}"),
                 "save.yaml", "plugins.ParticlePlugin")),
      ContainsSubstring("[plugins.ParticlePlugin.life.color.color_keys.0]"));
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

TEST_CASE("Lit sprite material is an explicit opt in") {
  using flop::config::ConfigView;
  const auto material = flop::config::detail::decode_material(ConfigView(YAML::Load(
      "shader: craft/lit_sprite\nproperties:\n  normal_map: assets/normal.png\n  emission_map: assets/emission.png\n"),
      "assets/parts/fixture.yaml"));
  CHECK(material.shader == "craft/lit_sprite");
  CHECK(material.normal_map == "assets/normal.png");
  CHECK(material.emission_map == "assets/emission.png");
  CHECK(material.normal_map_source.address == "assets/parts/fixture.yaml");
  CHECK(material.normal_map_source.key == "properties.normal_map");
  const auto defaults = flop::config::detail::decode_material(ConfigView(
      YAML::Load("shader: craft/lit_sprite"), "assets/parts/fixture.yaml"));
  CHECK(defaults.normal_map.empty());
  CHECK(defaults.emission_map.empty());
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
