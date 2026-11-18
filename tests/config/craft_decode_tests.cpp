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

const std::string BASIC_YAML =
    "name: Test\n"
    "orbit: {parent: Hamswell, h: 100, e: 0.1, omega: 0.2, M0: 0.3, t0: 4}\n"
    "rotation: {angle: 0.4, momentum: -5}\n"
    "parts:\n"
    "  - {type: Pod, id: pod_1, name: Pod}\n";

} // namespace

TEST_CASE("Shipped craft decodes its own fields independently of part configuration") {
  const AssetResolver resolver(std::filesystem::path(FLOP_TEST_ASSET_ROOT));
  const auto config =
      load_config<CraftConfig>(resolver, AssetPath("assets/spacecraft/OrbitalTestCraft.yaml"));

  CHECK(config.name == "Orbital Test Craft");
  CHECK(config.orbit.parent_name == "Hamswell");
  CHECK(config.orbit.specific_angular_momentum_m2_per_s == 1.73e10);
  CHECK(config.orbit.eccentricity == Approx(0.08));
  CHECK(config.orbit.longitude_periapsis_radians == Approx(1));
  CHECK(config.orbit.epoch_mean_anomaly_radians == Approx(1));
  CHECK(config.orbit.epoch_sec == 0);
  CHECK(config.initial_attitude.angle_radians == Approx(2.7));
  CHECK(config.initial_attitude.angular_momentum_kg_m2_per_s == -1000);
}

TEST_CASE("Craft decoder validates only the parts collection boundary") {
  auto yaml = BASIC_YAML;
  yaml.replace(yaml.find("parts:\n"), std::string::npos, "parts: wrong\n");
  CHECK_THROWS_WITH(decode(yaml), ContainsSubstring("[parts]"));

  yaml = BASIC_YAML;
  yaml.replace(yaml.find("type: Pod"), 9, "type: ''");
  CHECK(decode(yaml).name == "Test");
}

TEST_CASE("Craft decoder locates malformed craft fields") {
  auto yaml = BASIC_YAML;
  yaml.replace(yaml.find("h: 100"), 6, "h: 0");
  CHECK_THROWS_WITH(decode(yaml), ContainsSubstring("[orbit.h]"));

  yaml = BASIC_YAML;
  yaml.replace(yaml.find("momentum: -5"), 12, "momentum: .nan");
  CHECK_THROWS_WITH(decode(yaml), ContainsSubstring("[rotation.momentum]"));
}
