#include <filesystem>
#include <numbers>
#include <stdexcept>
#include <string>
#include <yaml-cpp/yaml.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "flop/config/decode/body.hpp"

namespace {

using Catch::Approx;
using Catch::Matchers::ContainsSubstring;
using flop::config::AssetPath;
using flop::config::AssetResolver;
using flop::config::ConfigDecoder;
using flop::config::ConfigView;
using flop::config::load_config;
using flop::world::BodyConfig;

BodyConfig decode(std::string source) {
  return ConfigDecoder<BodyConfig>::decode(ConfigView(YAML::Load(source), "assets/body.yaml"));
}

const std::string ROOT_YAML =
    "name: Sun\nradius: 600000\nsurface_gravity: 286\nday_length: 614\norbit: false\n";

} // namespace

TEST_CASE("Body config decoding converts authored units into owning SI values") {
  const auto root = decode(ROOT_YAML);
  CHECK(root.name == "Sun");
  CHECK(root.radius_meters == 6e8);
  CHECK(root.surface_gravity_m_per_s2 == 286);
  CHECK(root.day_length_sec == 614 * 3600);
  CHECK_FALSE(root.orbit.has_value());

  const auto child = decode(
      "name: Hamswell\nradius: 3000\nsurface_gravity: 9.8\nday_length: 24\n"
      "orbit:\n  parent: Sun\n  semimajor_axis: 9.2e7\n  eccentricity: 0.015\n"
      "  longitude_periapsis: 90\n  epoch_mean_anom: 180\n  epoch_time: 17\n"
  );
  CHECK(child.name == "Hamswell");
  REQUIRE(child.orbit.has_value());
  CHECK(child.orbit->parent_name == "Sun");
  CHECK(child.orbit->semimajor_axis_meters == 9.2e10);
  CHECK(child.orbit->longitude_periapsis_rad == Approx(std::numbers::pi / 2));
  CHECK(child.orbit->epoch_mean_anomaly_rad == Approx(std::numbers::pi));
  CHECK(child.orbit->epoch_sec == 17);
}

TEST_CASE("Body config errors identify invalid authored fields") {
  SECTION("empty name") {
    CHECK_THROWS_WITH(
        decode("name: ''\nradius: 1\nsurface_gravity: 1\nday_length: 1\norbit: false\n"),
        ContainsSubstring("[name]")
    );
  }
  SECTION("invalid radius") {
    CHECK_THROWS_WITH(
        decode("name: A\nradius: -1\nsurface_gravity: 1\nday_length: 1\norbit: false\n"),
        ContainsSubstring("[radius]")
    );
  }
  SECTION("overflowing unit conversion") {
    CHECK_THROWS_WITH(
        decode("name: A\nradius: 1e308\nsurface_gravity: 1\nday_length: 1\norbit: false\n"),
        ContainsSubstring("[radius]")
    );
  }
  SECTION("root orbit must be false") {
    CHECK_THROWS_WITH(
        decode("name: A\nradius: 1\nsurface_gravity: 1\nday_length: 1\norbit: true\n"),
        ContainsSubstring("[orbit]")
    );
  }
  SECTION("missing child parent") {
    CHECK_THROWS_WITH(
        decode("name: A\nradius: 1\nsurface_gravity: 1\nday_length: 1\norbit: {eccentricity: 0}\n"),
        ContainsSubstring("[orbit.parent]")
    );
  }
  SECTION("unbound eccentricity") {
    CHECK_THROWS_WITH(
        decode(
            "name: A\nradius: 1\nsurface_gravity: 1\nday_length: 1\n"
            "orbit: {parent: Sun, semimajor_axis: 2, eccentricity: 1, longitude_periapsis: 0, "
            "epoch_mean_anom: 0, epoch_time: 0}\n"
        ),
        ContainsSubstring("[orbit.eccentricity]")
    );
  }
}

TEST_CASE("Shipped Sun and Hamswell body configs decode through the asset resolver") {
  const AssetResolver resolver(std::filesystem::path(FLOP_TEST_ASSET_ROOT));
  const auto sun =
      load_config<BodyConfig>(resolver, AssetPath("assets/celestialbodies/Sun/Sun.yaml"));
  const auto hamswell =
      load_config<BodyConfig>(resolver, AssetPath("assets/celestialbodies/Hamswell/Hamswell.yaml"));
  CHECK(sun.name == "Sun");
  CHECK(sun.radius_meters == 6e8);
  CHECK_FALSE(sun.orbit.has_value());
  CHECK(hamswell.name == "Hamswell");
  CHECK(hamswell.radius_meters == 3e6);
  REQUIRE(hamswell.orbit.has_value());
  CHECK(hamswell.orbit->parent_name == sun.name);
  CHECK(hamswell.orbit->semimajor_axis_meters == 9.2e10);
}
