#include <cmath>
#include <limits>
#include <stdexcept>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "flop/world/celestial_body.hpp"

namespace {

using Catch::Approx;
using flop::world::BodyConfig;
using flop::world::BodyOrbitConfig;
using flop::world::CelestialBody;

const BodyConfig ROOT{"root", 10, 1, 100, {}};
const BodyConfig CHILD{"child", 2, 1, 50, BodyOrbitConfig{"root", 4, 0, 0, 0, 100}};
constexpr double G = 6.67430e-11;

} // namespace

TEST_CASE("Celestial body owns its config and derives gravity and mass") {
  auto config = ROOT;
  CelestialBody body(config, G);
  config.name = "changed";
  CHECK(body.name() == "root");
  CHECK(body.config().radius_meters == 10);
  CHECK(body.gravitational_parameter() == 100);
  CHECK(body.mass_kg() == Approx(100 / G));

  CelestialBody other(ROOT, 1e-10);
  CHECK(other.gravitational_parameter() == body.gravitational_parameter());
  CHECK(other.mass_kg() == Approx(1e12));
}

TEST_CASE("Celestial body supplies root and child frame definitions") {
  const CelestialBody root(ROOT, G);
  const auto root_frame = root.ref_frame_definition();
  CHECK(root_frame.name == "root");
  CHECK(root_frame.gravitational_parameter == 100);
  CHECK_FALSE(root_frame.parent_name.has_value());
  CHECK_FALSE(root_frame.orbit_about_parent.has_value());
  CHECK(std::isinf(root_frame.sphere_of_influence_radius));

  const CelestialBody child(CHILD, G);
  const auto child_frame = child.ref_frame_definition(root.gravitational_parameter());
  REQUIRE(child_frame.parent_name.has_value());
  CHECK(*child_frame.parent_name == "root");
  REQUIRE(child_frame.orbit_about_parent.has_value());
  CHECK(child_frame.orbit_about_parent->epoch == 100);
  CHECK(child_frame.orbit_about_parent->state.position.x == Approx(4));
  CHECK(child_frame.orbit_about_parent->state.position.y == Approx(0));
  CHECK(child_frame.orbit_about_parent->state.velocity.y == Approx(5));
  CHECK(child_frame.sphere_of_influence_radius == Approx(4 * std::pow(4.0 / 100, 0.4)));
}

TEST_CASE("Celestial body rejects invalid physical and orbit config") {
  auto body = ROOT;
  SECTION("empty name") {
    body.name.clear();
  }
  SECTION("zero radius") {
    body.radius_meters = 0;
  }
  SECTION("nonfinite radius") {
    body.radius_meters = std::numeric_limits<double>::infinity();
  }
  SECTION("negative gravity") {
    body.surface_gravity_m_per_s2 = -1;
  }
  SECTION("zero day") {
    body.day_length_sec = 0;
  }
  SECTION("overflowing gravitational parameter") {
    body.radius_meters = 1e200;
  }
  SECTION("overflowing mass") {
    body.radius_meters = 1e150;
  }
  SECTION("empty parent") {
    body.orbit = BodyOrbitConfig{"", 4, 0, 0, 0, 0};
  }
  SECTION("nonpositive semimajor axis") {
    body.orbit = BodyOrbitConfig{"root", 0, 0, 0, 0, 0};
  }
  SECTION("unbound orbit") {
    body.orbit = BodyOrbitConfig{"root", 4, 1, 0, 0, 0};
  }
  SECTION("negative eccentricity") {
    body.orbit = BodyOrbitConfig{"root", 4, -1, 0, 0, 0};
  }
  SECTION("nonfinite angle") {
    body.orbit = BodyOrbitConfig{"root", 4, 0, std::numeric_limits<double>::quiet_NaN(), 0, 0};
  }
  SECTION("nonfinite epoch") {
    body.orbit = BodyOrbitConfig{"root", 4, 0, 0, 0, std::numeric_limits<double>::infinity()};
  }
  CHECK_THROWS_AS(CelestialBody(body, G), std::invalid_argument);
}

TEST_CASE("Celestial body checks parent gravity and derived orbit domain") {
  const CelestialBody root(ROOT, G);
  const CelestialBody child(CHILD, G);
  CHECK_THROWS_AS(root.ref_frame_definition(100), std::invalid_argument);
  CHECK_THROWS_AS(child.ref_frame_definition(), std::invalid_argument);
  CHECK_THROWS_AS(child.ref_frame_definition(0), std::invalid_argument);
  CHECK_THROWS_AS(
      child.ref_frame_definition(std::numeric_limits<double>::infinity()), std::invalid_argument
  );
  CHECK_THROWS_AS(CelestialBody(ROOT, 0), std::invalid_argument);

  auto large_orbit = CHILD;
  large_orbit.orbit->semimajor_axis_meters = 1e300;
  CHECK_THROWS_AS(CelestialBody(large_orbit, G).ref_frame_definition(1e300), std::invalid_argument);
}
