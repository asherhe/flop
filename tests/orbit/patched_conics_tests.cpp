#include <array>
#include <cmath>
#include <stdexcept>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "flop/orbit/prediction/patched_conics.hpp"
#include "flop/orbit/ref_frame.hpp"

using Catch::Approx;
using namespace flop;
using namespace flop::orbit;

namespace {

constexpr double PLANET_MU = 1e8;
constexpr double MOON_MU = 5e4;
constexpr double MOON_ORBIT_RADIUS = 10000;
constexpr double MOON_SPEED = 100;
constexpr double SOI_RADIUS = 150;
constexpr OrbitState MOON{0, {{MOON_ORBIT_RADIUS, 0}, {0, MOON_SPEED}}};
constexpr StateVectors APPROACH{{MOON_ORBIT_RADIUS + 75, -600}, {0, 140}};

StateVectors analytic_moon(double ut) {
  const double angle = ut * MOON_SPEED / MOON_ORBIT_RADIUS;
  return {MOON.state.position.rotate(angle), MOON.state.velocity.rotate(angle)};
}

StateVectors add(StateVectors state, StateVectors delta, double scale = 1) {
  return {state.position + scale * delta.position, state.velocity + scale * delta.velocity};
}

StateVectors rk4(StateVectors state, double step, double mu) {
  const auto rate = [=](StateVectors value) {
    const double radius = value.position.length();
    return StateVectors{value.velocity, (-mu / (radius * radius * radius)) * value.position};
  };
  const auto k1 = rate(state);
  const auto k2 = rate(add(state, k1, step / 2));
  const auto k3 = rate(add(state, k2, step / 2));
  const auto k4 = rate(add(state, k3, step));
  return add(state, add(add(k1, k2, 2), add(k4, k3, 2)), step / 6);
}

// independent, small-step reference with analytic moon motion. locate each
// sign change by interpolating the crossing in the bracketing rk4 step.
std::array<double, 2> reference_flyby() {
  constexpr double STEP = 0.0005;
  std::array<double, 2> times{};
  StateVectors state = APPROACH;
  std::size_t hits = 0;
  double ut = 0;
  for (int sample = 0; sample < 120000 && hits < times.size(); ++sample) {
    const bool entering = hits == 0;
    const double mu = entering ? PLANET_MU : MOON_MU;
    const auto signed_distance = [=](StateVectors point, double time) {
      return entering ? (point.position - analytic_moon(time).position).length() - SOI_RADIUS
                      : SOI_RADIUS - point.position.length();
    };
    const auto next = rk4(state, STEP, mu);
    const double end = signed_distance(next, ut + STEP);
    if (end <= 0) {
      const double start = signed_distance(state, ut);
      const double clip = STEP * start / (start - end);
      state = rk4(state, clip, mu);
      ut += clip;
      times.at(hits++) = ut;
      state = add(state, analytic_moon(ut), entering ? -1 : 1);
    } else {
      state = next;
      ut += STEP;
    }
  }
  REQUIRE(hits == times.size());
  return times;
}

} // namespace

TEST_CASE("Patched conics predict entry and escape with continuous state") {
  const RefFrameManager frames({
      {"planet", PLANET_MU, {}, {}, 0},
      {"moon", MOON_MU, "planet", MOON, SOI_RADIUS},
  });
  const auto planet = frames.frame_id("planet");
  const auto moon = frames.frame_id("moon");
  const OrbitState initial{0, APPROACH, planet};
  const auto times = reference_flyby();
  constexpr double END_UT = 60;
  const auto result = build_patches(frames, initial, 1, END_UT);

  REQUIRE(result.patches.size() == 3);
  CHECK(result.patches.at(0).orbit.frame_id == planet);
  CHECK(result.patches.at(1).orbit.frame_id == moon);
  CHECK(result.patches.at(2).orbit.frame_id == planet);
  CHECK(result.patches.front().orbit.epoch == 0);
  CHECK(result.patches.front().begin_ut == 1);
  CHECK(result.searched_through == END_UT);
  CHECK(result.patches.back().end_ut == END_UT);
  CHECK(result.patch_at(0) == nullptr);
  CHECK(result.patch_at(END_UT) == &result.patches.back());
  CHECK(result.patch_at(std::nextafter(END_UT, 61.0)) == nullptr);

  for (std::size_t index = 0; index < times.size(); ++index) {
    const auto &before = result.patches.at(index);
    const auto &after = result.patches.at(index + 1);
    const double ut = after.begin_ut;
    CHECK(ut == Approx(times.at(index)).margin(1e-3));
    CHECK(before.end_ut == ut);
    CHECK(after.orbit.epoch == ut);
    CHECK(result.patch_at(std::nextafter(ut, before.begin_ut)) == &before);
    CHECK(result.patch_at(ut) == &after);

    const auto &frame = frames.frame(before.orbit.frame_id);
    const auto before_state =
        UniversalPropagator(frame.gravitational_parameter, before.orbit).state_at(ut);
    const auto before_offset = frames.offset(before.orbit.frame_id, planet, ut);
    const auto after_offset = frames.offset(after.orbit.frame_id, planet, ut);
    const auto inertial_before = add(before_state, before_offset);
    const auto inertial_after = add(after.orbit.state, after_offset);
    CHECK((inertial_before.position - inertial_after.position).length() < 1e-8);
    CHECK((inertial_before.velocity - inertial_after.velocity).length() < 1e-10);
  }
}

TEST_CASE("A candidate transition waits for other searches to reach its time") {
  const RefFrameManager frames({
      {"star", 1, {}, {}, 0},
      {"planet", 3, "star", OrbitState{0, {{1, 0}, {0, 1}}}, 4},
      {"moon", 1e-4, "planet", OrbitState{0, {{-3, 0}, {0, -1}}}, 0.1},
  });
  const auto star = frames.frame_id("star");
  const auto planet = frames.frame_id("planet");
  const OrbitState initial{0, {{2, 0}, {0, 1.5}}, planet};
  const auto escape = first_soi_escape(frames, initial, 0, 10, 0);
  REQUIRE(escape.crossing_ut.has_value());

  const auto stopped = next_soi_transition(frames, initial, 0, 10, 0);
  CHECK_FALSE(stopped.transition);
  CHECK(stopped.searched_through == 0);
  const auto partial = build_patches(frames, initial, 0, 10, 1);
  REQUIRE(partial.patches.size() == 1);
  CHECK(partial.searched_through > 0);
  CHECK(partial.searched_through < *escape.crossing_ut);
  CHECK(partial.patches.front().end_ut == partial.searched_through);
  CHECK(partial.patch_at(*escape.crossing_ut) == nullptr);

  const auto resumed = next_soi_transition(frames, initial, partial.searched_through, 10);
  REQUIRE(resumed.transition.has_value());
  CHECK(resumed.transition->from == planet);
  CHECK(resumed.transition->next_orbit.frame_id == star);
  CHECK(resumed.transition->next_orbit.epoch == Approx(*escape.crossing_ut).margin(1e-9));
}

TEST_CASE("Simultaneous SOI entries prefer the lowest destination frame ID") {
  const RefFrameManager frames({
      {"moon_a", MOON_MU, "planet", MOON, SOI_RADIUS},
      {"planet", PLANET_MU, {}, {}, 0},
      {"moon_b", MOON_MU, "planet", MOON, SOI_RADIUS},
  });
  const auto planet = frames.frame_id("planet");
  const auto first_moon = frames.frame_id("moon_a");
  const OrbitState initial{0, APPROACH, planet};
  const auto result = next_soi_transition(frames, initial, 0, 60);
  REQUIRE(result.transition.has_value());
  CHECK(result.transition->next_orbit.frame_id == first_moon);

  const double ut = result.transition->next_orbit.epoch;
  const auto at_horizon = build_patches(frames, initial, 0, ut);
  REQUIRE(at_horizon.patches.size() == 2);
  CHECK(at_horizon.patches.back().begin_ut == ut);
  CHECK(at_horizon.patches.back().end_ut == ut);
  CHECK(at_horizon.patch_at(ut) == &at_horizon.patches.back());

  const double inside_ut = ut + 0.1;
  const auto immediate = next_soi_transition(frames, initial, inside_ut, 60, 0);
  REQUIRE(immediate.transition.has_value());
  CHECK(immediate.transition->next_orbit.frame_id == first_moon);
  CHECK(immediate.transition->next_orbit.epoch == inside_ut);
}

TEST_CASE("Patch prediction follows a chain of SOI transitions at the same UT") {
  const RefFrameManager frames({
      {"star", 1, {}, {}, 0},
      {"planet", 3, "star", OrbitState{0, {{1, 0}, {0, 1}}}, 10},
      {"moon", 1e-4, "planet", OrbitState{0, {{3, 0}, {0, 1}}}, 2},
  });
  const auto star = frames.frame_id("star");
  const auto planet = frames.frame_id("planet");
  const auto moon = frames.frame_id("moon");
  const OrbitState initial{0, {{5, 0}, {-0.5, 2}}, star};

  for (double end_ut : {0.0, 1.0}) {
    CAPTURE(end_ut);
    const auto result = build_patches(frames, initial, 0, end_ut);
    REQUIRE(result.patches.size() == 3);
    CHECK(result.endpoint_resolved);
    CHECK(result.searched_through == end_ut);
    CHECK(result.patches.at(0).orbit.frame_id == star);
    CHECK(result.patches.at(1).orbit.frame_id == planet);
    CHECK(result.patches.at(1).begin_ut == 0);
    CHECK(result.patches.at(1).end_ut == 0);
    CHECK(result.patches.at(2).orbit.frame_id == moon);
    CHECK(result.patches.at(2).begin_ut == 0);
    CHECK(result.patch_at(0) == &result.patches.back());
    CHECK(result.patches.back().orbit.state.position.x == 1);
    CHECK(result.patches.back().orbit.state.velocity.x == -0.5);
    CHECK(result.patches.back().orbit.state.velocity.y == 0);
  }
}

TEST_CASE("Patch prediction validates inputs and retains the seed over a singleton interval") {
  const RefFrameManager frames({{"planet", PLANET_MU, {}, {}, 0}});
  const OrbitState initial{0, APPROACH, frames.frame_id("planet")};
  const auto result = build_patches(frames, initial, 10, 10);
  REQUIRE(result.patches.size() == 1);
  CHECK(result.searched_through == 10);
  CHECK(result.patches.front().orbit.epoch == initial.epoch);
  CHECK(result.patch_at(10) == &result.patches.front());
  CHECK_THROWS_AS(build_patches(frames, initial, 10, 1), std::invalid_argument);
  CHECK_THROWS_AS(build_patches(frames, OrbitState{0, APPROACH}, 0, 0), std::invalid_argument);
  CHECK_THROWS_AS(
      build_patches(frames, OrbitState{0, {}, initial.frame_id}, 0, 10), std::invalid_argument
  );
}
