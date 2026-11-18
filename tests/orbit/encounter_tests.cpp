#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "flop/orbit/prediction/encounter.hpp"
#include "flop/orbit/propagators/universal.hpp"
#include "flop/orbit/ref_frame.hpp"

using Catch::Approx;
using namespace flop;
using namespace flop::orbit;

namespace {

constexpr FrameId PLANET{1};
constexpr double PLANET_MU = 1e8;
constexpr double MOON_ORBIT_RADIUS = 10000;
constexpr double MOON_SPEED = 100;
constexpr double MOON_MU = 5e4;
constexpr OrbitState MOON{0, {{MOON_ORBIT_RADIUS, 0}, {0, MOON_SPEED}}, PLANET};
constexpr OrbitState FLYBY{0, {{MOON_ORBIT_RADIUS + 75, -600}, {0, 140}}, PLANET};

RefFrameManager planet_and_moon(double radius) {
  return RefFrameManager({
      {"planet", PLANET_MU, {}, {}, 0},
      {"moon", MOON_MU, "planet", MOON, radius},
  });
}

RefFrameManager escape_frames(double mu, double radius) {
  return RefFrameManager({
      {"star", PLANET_MU, {}, {}, 0},
      {"planet", mu, "star", MOON, radius},
  });
}

double moon_distance(double ut) {
  const Vec2d moon_position =
      Vec2d{MOON_ORBIT_RADIUS, 0}.rotate(ut * MOON_SPEED / MOON_ORBIT_RADIUS);
  return (UniversalPropagator(PLANET_MU, FLYBY).position_at(ut) - moon_position).length();
}

void check_escape(double mu, OrbitState orbit, double radius, double begin_ut, double expected_ut) {
  const auto frames = escape_frames(mu, radius);
  orbit.frame_id = frames.frame_id("planet");
  const double margin = std::abs(expected_ut - begin_ut) * 1e-7;
  // with no sampling budget these cases must be resolved analytically.
  const auto before = first_soi_escape(frames, orbit, begin_ut, expected_ut - margin, 0);
  CHECK_FALSE(before.crossing_ut);
  CHECK(before.searched_through == expected_ut - margin);

  const auto escape = first_soi_escape(frames, orbit, begin_ut, expected_ut + margin, 0);
  REQUIRE(escape.crossing_ut.has_value());
  CHECK(*escape.crossing_ut == Approx(expected_ut).margin(margin));
  CHECK(escape.searched_through == *escape.crossing_ut);
  const auto state = UniversalPropagator(mu, orbit).state_at(*escape.crossing_ut);
  CHECK(state.position.length() == Approx(radius).epsilon(1e-12));
  CHECK(state.position.dot(state.velocity) > 0);
}

} // namespace

TEST_CASE("Closest approach finds the conjunction of two circular body orbits") {
  const auto frames = planet_and_moon(500);
  constexpr double RADIUS = 9000;
  constexpr double PHASE = -0.2;
  const double speed = std::sqrt(PLANET_MU / RADIUS);
  const OrbitState subject{
      0, {Vec2d{RADIUS, 0}.rotate(PHASE), Vec2d{0, speed}.rotate(PHASE)}, PLANET
  };
  const double expected_ut = -PHASE / (speed / RADIUS - MOON_SPEED / MOON_ORBIT_RADIUS);
  const auto result = closest_approaches(frames, subject, MOON, 0, 2 * expected_ut);
  REQUIRE(result.encounters.size() == 1);
  const auto &encounter = result.encounters.front();
  CHECK(encounter.ut == Approx(expected_ut).margin(1e-9));
  CHECK(
      (encounter.first.position - encounter.second.position).length() ==
      Approx(MOON_ORBIT_RADIUS - RADIUS).margin(1e-8)
  );
  CHECK(result.searched_through == 2 * expected_ut);

  const auto disjoint =
      first_soi_entry(frames, subject, frames.frame_id("moon"), 0, 2 * expected_ut, 0);
  CHECK_FALSE(disjoint.crossing_ut);
  CHECK(disjoint.searched_through == 2 * expected_ut);

  const auto grazing_frames = planet_and_moon(MOON_ORBIT_RADIUS - RADIUS);
  const auto grazing = first_soi_entry(
      grazing_frames, subject, grazing_frames.frame_id("moon"), expected_ut - 1, expected_ut + 1, 1
  );
  CHECK_FALSE(grazing.crossing_ut);
  CHECK(grazing.searched_through == expected_ut + 1);
}

TEST_CASE("SOI entry finds a short flyby with both interval endpoints outside") {
  const auto result = closest_approaches(planet_and_moon(150), FLYBY, MOON, 0, 25);
  REQUIRE(result.encounters.size() == 1);
  const double minimum_ut = result.encounters.front().ut;
  const double radius = moon_distance(minimum_ut) + 0.01;
  const double begin_ut = minimum_ut - 0.5;
  const double end_ut = minimum_ut + 0.5;
  REQUIRE(moon_distance(begin_ut) > radius);
  REQUIRE(moon_distance(end_ut) > radius);

  const auto frames = planet_and_moon(radius);
  const auto entry = first_soi_entry(frames, FLYBY, frames.frame_id("moon"), begin_ut, end_ut, 1);
  REQUIRE(entry.crossing_ut.has_value());
  CHECK(*entry.crossing_ut > begin_ut);
  CHECK(*entry.crossing_ut < minimum_ut);
  CHECK(moon_distance(*entry.crossing_ut) == Approx(radius).margin(1e-9));
  CHECK(entry.searched_through == *entry.crossing_ut);
}

TEST_CASE("Constant separation produces neither closest approaches nor SOI entry") {
  const double radius = std::sqrt(2.0);
  const OrbitState center{0, {{0, 1}, {-1, 0}}, PLANET};
  const RefFrameManager frames({
      {"planet", 1, {}, {}, 0},
      {"moon", MOON_MU, "planet", center, radius},
  });
  const OrbitState subject{0, {{1, 0}, {0, 1}}, PLANET};
  constexpr double END_UT = 2 * std::numbers::pi;
  const auto approaches = closest_approaches(frames, subject, center, 0, END_UT);
  CHECK(approaches.encounters.empty());
  CHECK(approaches.searched_through == END_UT);

  const auto entry = first_soi_entry(frames, subject, frames.frame_id("moon"), 0, END_UT);
  CHECK_FALSE(entry.crossing_ut);
  CHECK(entry.searched_through == END_UT);
}

TEST_CASE("SOI searches report their completed coverage and reject invalid comparisons") {
  const auto frames = planet_and_moon(150);
  const auto moon = frames.frame_id("moon");
  const auto limited = first_soi_entry(frames, FLYBY, moon, 0, 25, 1);
  CHECK_FALSE(limited.crossing_ut);
  CHECK(limited.searched_through > 0);
  CHECK(limited.searched_through < 25);
  const auto continuation = first_soi_entry(frames, FLYBY, moon, limited.searched_through, 25);
  REQUIRE(continuation.crossing_ut.has_value());
  CHECK(moon_distance(*continuation.crossing_ut) == Approx(150).margin(1e-9));
  const auto short_search = closest_approaches(frames, FLYBY, MOON, 0, 25, 0);
  CHECK(short_search.encounters.empty());
  CHECK(short_search.searched_through == 0);
  const auto empty = first_soi_entry(frames, FLYBY, moon, 25, 25);
  CHECK_FALSE(empty.crossing_ut);
  CHECK(empty.searched_through == 25);

  auto other_frame = MOON;
  other_frame.frame_id = moon;
  CHECK_THROWS_AS(closest_approaches(frames, FLYBY, other_frame, 0, 25), std::invalid_argument);
  CHECK_THROWS_AS(first_soi_entry(frames, FLYBY, PLANET, 0, 25), std::invalid_argument);
  CHECK_THROWS_AS(first_soi_entry(frames, FLYBY, FrameId{}, 0, 25), std::invalid_argument);
  CHECK_THROWS_AS(first_soi_escape(frames, FLYBY, 25, 0), std::invalid_argument);
  CHECK_THROWS_AS(
      first_soi_escape(frames, FLYBY, 0, std::numeric_limits<double>::infinity()),
      std::invalid_argument
  );
  other_frame.frame_id = FrameId{3};
  CHECK_THROWS_AS(
      closest_approaches(frames, other_frame, other_frame, 0, 25), std::invalid_argument
  );
  CHECK_THROWS_AS(first_soi_escape(frames, other_frame, 0, 25), std::invalid_argument);

  const auto root_escape = first_soi_escape(frames, FLYBY, 0, 25, 0);
  CHECK_FALSE(root_escape.crossing_ut);
  CHECK(root_escape.searched_through == 25);
  CHECK_THROWS_AS(frames.propagator_about_parent(PLANET), std::invalid_argument);
}

TEST_CASE("Analytic SOI escape selects the next outbound crossing of a bound orbit") {
  constexpr double MU = 3;
  const auto frames = escape_frames(MU, 4);
  const OrbitState orbit{0, {{2, 0}, {0, -1.5}}, frames.frame_id("planet")};
  constexpr double SEMIMAJOR = 4;
  constexpr double E = 0.5;
  const double anomaly = std::numbers::pi / 2;
  const double time_scale = std::sqrt(SEMIMAJOR * SEMIMAJOR * SEMIMAJOR / MU);
  const double crossing_ut = (anomaly - E * std::sin(anomaly)) * time_scale;
  const double period = 2 * std::numbers::pi * time_scale;
  check_escape(MU, orbit, 4, 0, crossing_ut);
  check_escape(MU, orbit, 4, period - crossing_ut / 2, period + crossing_ut);
  check_escape(MU, orbit, 4, 1000 * period - crossing_ut / 2, 1000 * period + crossing_ut);
  check_escape(
      MU,
      orbit,
      6 - 1e-5,
      0,
      (std::acos(-1 + 1e-5 / 2) - E * std::sin(std::acos(-1 + 1e-5 / 2))) * time_scale
  );

  const auto tangent = first_soi_escape(escape_frames(MU, 6), orbit, 0, period, 0);
  CHECK_FALSE(tangent.crossing_ut);
  CHECK(tangent.searched_through == period);
  const auto beyond_apoapsis = first_soi_escape(escape_frames(MU, 7), orbit, 0, period, 0);
  CHECK_FALSE(beyond_apoapsis.crossing_ut);
  CHECK(beyond_apoapsis.searched_through == period);
  const auto already_outgoing = first_soi_escape(frames, orbit, 2 * crossing_ut, period, 0);
  REQUIRE(already_outgoing.crossing_ut.has_value());
  CHECK(*already_outgoing.crossing_ut == 2 * crossing_ut);
}

TEST_CASE("SOI escape detects a bound orbit starting just inside the boundary") {
  constexpr double MU = 3;
  constexpr double END_UT = 0.001;
  const double radius = std::nextafter(2.0, 3.0);
  const auto frames = escape_frames(MU, radius);
  const OrbitState orbit{0, {{2, 0}, {0.375, 0.625}}, frames.frame_id("planet")};
  const UniversalPropagator propagator(MU, orbit);
  REQUIRE(propagator.position_at(0).length() < radius);
  REQUIRE(propagator.position_at(END_UT).length() > radius);
  const auto escape = first_soi_escape(frames, orbit, 0, END_UT);
  REQUIRE(escape.crossing_ut.has_value());
  CHECK(*escape.crossing_ut > 0);
  CHECK(*escape.crossing_ut < 1e-12);
  CHECK(escape.searched_through == *escape.crossing_ut);
}

TEST_CASE("Analytic SOI escape handles parabolic, near-parabolic and hyperbolic motion") {
  constexpr double MU = 2;
  constexpr double P = 8;
  for (double e : {0.999999, 1.0, 1.000001, 1.5}) {
    CAPTURE(e);
    const double periapsis = P / (1 + e);
    const OrbitState seed{0, {{periapsis, 0}, {0, std::sqrt(MU * P) / periapsis}}};
    constexpr double HALF_ANGLE_VARIABLE = 0.5;
    const double nu = 2 * std::atan(HALF_ANGLE_VARIABLE);
    const double radius = P / (1 + e * std::cos(nu));
    double expected_ut;
    if (e == 1) {
      expected_ut =
          P * std::sqrt(P / MU) * (HALF_ANGLE_VARIABLE / 2 + std::pow(HALF_ANGLE_VARIABLE, 3) / 6);
    } else {
      const double a = P / std::abs(1 - e * e);
      const double n = std::sqrt(MU / a) / a;
      const double x = HALF_ANGLE_VARIABLE * std::sqrt(std::abs(1 - e) / (1 + e));
      if (e < 1) {
        const double anomaly = 2 * std::atan(x);
        expected_ut = (anomaly - e * std::sin(anomaly)) / n;
      } else {
        const double anomaly = 2 * std::atanh(x);
        expected_ut = (e * std::sinh(anomaly) - anomaly) / n;
      }
    }
    check_escape(MU, seed, radius, 0, expected_ut);
    check_escape(MU, seed, radius, -expected_ut / 2, expected_ut);
  }
}
