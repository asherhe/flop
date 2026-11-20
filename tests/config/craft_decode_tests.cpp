#include <filesystem>
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
