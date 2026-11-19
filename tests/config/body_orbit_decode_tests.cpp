#include <numbers>
#include <string>
#include <yaml-cpp/yaml.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "flop/config/decode/body_orbit.hpp"

namespace {

using Catch::Approx;
using Catch::Matchers::ContainsSubstring;
using flop::config::ConfigDecoder;
using flop::config::ConfigView;
using flop::world::BodyOrbitConfig;

BodyOrbitConfig decode(std::string source) {
  return ConfigDecoder<BodyOrbitConfig>::decode(
      ConfigView(YAML::Load(source), "assets/orbit.yaml", "orbit")
  );
}

const std::string ORBIT_YAML = "parent: Sun\nsemimajor_axis: 92000000\neccentricity: 0.015\n"
                               "longitude_periapsis: 90\nepoch_mean_anom: 180\nepoch_time: 17\n";

} // namespace

TEST_CASE("Body orbit decoder converts authored elements independently") {
  const auto orbit = decode(ORBIT_YAML);
  CHECK(orbit.parent_name == "Sun");
  CHECK(orbit.semimajor_axis_meters == 9.2e10);
  CHECK(orbit.eccentricity == 0.015);
  CHECK(orbit.longitude_periapsis_rad == Approx(std::numbers::pi / 2));
  CHECK(orbit.epoch_mean_anomaly_rad == Approx(std::numbers::pi));
  CHECK(orbit.epoch_sec == 17);
}

TEST_CASE("Body orbit decoder reports invalid element fields") {
  SECTION("missing parent") {
    CHECK_THROWS_WITH(decode("eccentricity: 0\n"), ContainsSubstring("[orbit.parent]"));
  }
  SECTION("invalid semimajor axis") {
    auto source = ORBIT_YAML;
    source.replace(source.find("92000000"), 8, "0");
    CHECK_THROWS_WITH(decode(source), ContainsSubstring("[orbit.semimajor_axis]"));
  }
  SECTION("unbound eccentricity") {
    auto source = ORBIT_YAML;
    source.replace(source.find("0.015"), 5, "1");
    CHECK_THROWS_WITH(decode(source), ContainsSubstring("[orbit.eccentricity]"));
  }
  SECTION("nonfinite angle") {
    auto source = ORBIT_YAML;
    source.replace(source.find("90"), 2, ".nan");
    CHECK_THROWS_WITH(decode(source), ContainsSubstring("[orbit.longitude_periapsis]"));
  }
}
