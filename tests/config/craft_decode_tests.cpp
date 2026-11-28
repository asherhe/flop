#include <array>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <yaml-cpp/yaml.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "flop/config/decode/craft.hpp"

namespace {

using Catch::Approx;
using Catch::Matchers::ContainsSubstring;
using flop::config::AssetPath;
using flop::config::AssetResolver;
using flop::config::ConfigDecoder;
using flop::config::ConfigView;
using flop::config::load_config;
using flop::craft::CraftConfig;

CraftConfig decode(std::string source) {
  return ConfigDecoder<CraftConfig>::decode(ConfigView(YAML::Load(source), "assets/craft.yaml"));
}

// a template: no orbit block, since a template has no world position.
const std::string TEMPLATE_YAML =
    "name: Test\n"
    "rotation: {angle: 0.4, momentum: -5}\n"
    "parts:\n"
    "  - {type: Pod, id: pod_1, name: Pod, transform: {pos: {x: 0, y: 0}, rot: 0}, "
    "attachments: [], plugins: {}}\n";

// an instance: a template plus a cartesian orbit seed.
const std::string INSTANCE_YAML =
    "name: Test\n"
    "orbit: {parent: Hamswell, pos: {x: 100, y: -200}, vel: {x: 3, y: 4}, t0: 4}\n"
    "rotation: {angle: 0.4, momentum: -5}\n"
    "parts:\n"
    "  - {type: Pod, id: pod_1, name: Pod, transform: {pos: {x: 0, y: 0}, rot: 0}, "
    "attachments: [], plugins: {}}\n";

} // namespace

TEST_CASE("The shipped craft template decodes without a world orbit") {
  const AssetResolver resolver(std::filesystem::path(FLOP_TEST_ASSET_ROOT));
  const auto config =
      load_config<CraftConfig>(resolver, AssetPath("assets/spacecraft/OrbitalTestCraft.yaml"));

  CHECK(config.name == "Orbital Test Craft");
  CHECK_FALSE(config.orbit.has_value());
  CHECK(config.initial_attitude.angle_rad == Approx(2.7));
  CHECK(config.initial_attitude.angular_momentum_kg_m2_per_s == -1000);
  REQUIRE(config.parts.size() == 3);
  CHECK(config.parts[1].id == "TestingTank_2");
  CHECK(config.parts[1].position_meters.y == -2);
  REQUIRE(config.parts[1].attachments.size() == 2);
  CHECK(config.parts[1].attachments[1].attach_part_id == "TestingEngine_3");
}

TEST_CASE("A craft instance decodes its cartesian orbit seed") {
  const auto config = decode(INSTANCE_YAML);
  REQUIRE(config.orbit.has_value());
  CHECK(config.orbit->parent_name == "Hamswell");
  CHECK(config.orbit->state.position.x == Approx(100));
  CHECK(config.orbit->state.position.y == Approx(-200));
  CHECK(config.orbit->state.velocity.x == Approx(3));
  CHECK(config.orbit->state.velocity.y == Approx(4));
  CHECK(config.orbit->epoch_sec == Approx(4));
}

TEST_CASE("Craft decoder validates its parts collection and part fields") {
  auto yaml = TEMPLATE_YAML;
  yaml.replace(yaml.find("parts:\n"), std::string::npos, "parts: wrong\n");
  CHECK_THROWS_WITH(decode(yaml), ContainsSubstring("[parts]"));

  yaml = TEMPLATE_YAML;
  yaml.replace(yaml.find("type: Pod"), 9, "type: ''");
  CHECK_THROWS_WITH(decode(yaml), ContainsSubstring("[parts[0].type]"));
}

TEST_CASE("Craft decoder locates malformed craft fields") {
  auto yaml = INSTANCE_YAML;
  yaml.replace(yaml.find("parent: Hamswell"), 16, "parent: ''");
  CHECK_THROWS_WITH(decode(yaml), ContainsSubstring("[orbit.parent]"));

  yaml = TEMPLATE_YAML;
  yaml.replace(yaml.find("momentum: -5"), 12, "momentum: .nan");
  CHECK_THROWS_WITH(decode(yaml), ContainsSubstring("[rotation.momentum]"));
}

TEST_CASE("Missing craft texture identifies the override file and key") {
  namespace fs = std::filesystem;
  struct TempDirectory {
    fs::path path;
    TempDirectory() {
      do {
        path = fs::temp_directory_path() / ("flop-texture-test-" + std::to_string(std::random_device{}()));
      } while (!fs::create_directory(path));
    }
    ~TempDirectory() { std::error_code error; fs::remove_all(path, error); }
  } temp;
  fs::create_directory(temp.path / "assets");
  { std::ofstream file(temp.path / "assets" / "present.png"); file << "fixture"; }
  std::string override_yaml = "sprite: assets/missing.png";
  std::string expected_key = "parts[0].plugins.SpritePlugin.sprite";
  std::string missing = "missing.png";
  SECTION("missing sprite") {}
  SECTION("missing normal map") {
    override_yaml = "sprite: assets/present.png\n        material: {shader: shader:craft/sprite, properties: {normal_map: assets/missing_normal.png}}";
    expected_key = "parts[0].plugins.SpritePlugin.material.properties.normal_map";
    missing = "missing_normal.png";
  }
  SECTION("missing emission map") {
    override_yaml = "sprite: assets/present.png\n        material: {shader: shader:craft/sprite, properties: {emission_map: assets/missing_emission.png}}";
    expected_key = "parts[0].plugins.SpritePlugin.material.properties.emission_map";
    missing = "missing_emission.png";
  }
  {
    std::ofstream file(temp.path / "assets" / "craft.yaml");
    file << "name: Test\nrotation: {angle: 0, momentum: 0}\nparts:\n"
            "  - type: Pod\n    id: pod\n    name: Pod\n"
            "    transform: {pos: {x: 0, y: 0}, rot: 0}\n"
            "    attachments: []\n    plugins:\n"
            "      SpritePlugin:\n        " << override_yaml << "\n";
  }
  const auto part_yaml = YAML::Load(
      "name: Pod\nmass: 1\ncrossfeed: true\nattachment_nodes: []\nplugins:\n"
      "  SpritePlugin:\n    sprite: assets/original.png\n"
      "    size_meters: {x: 1, y: 1}\n    pivot: {x: 0.5, y: 0.5}\n"
      "    material: {shader: shader:craft/sprite}\n");
  auto part = ConfigDecoder<flop::craft::PartTypeConfig>::decode(
      ConfigView(part_yaml, "assets/part.yaml"));
  part.type_id = "Pod";
  const std::array types{part};
  try {
    static_cast<void>(flop::config::load_craft_config(
        AssetResolver(temp.path / "assets"), AssetPath("assets/craft.yaml"), types));
    FAIL("Expected a missing texture error");
  } catch (const flop::config::ConfigError &error) {
    CHECK(error.address() == "assets/craft.yaml");
    CHECK(error.key_path() == expected_key);
    CHECK_THAT(error.what(), ContainsSubstring(missing));
  }
}
