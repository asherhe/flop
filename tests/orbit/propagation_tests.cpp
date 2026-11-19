#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "flop/orbit/propagators/universal.hpp"

using Catch::Approx;
using namespace flop;
using namespace flop::orbit;

namespace {

void check_state(StateVectors actual, StateVectors expected) {
  CHECK(
      (actual.position - expected.position).length() <= 1e-3 + 1e-10 * expected.position.length()
  );
  CHECK(
      (actual.velocity - expected.velocity).length() <= 1e-6 + 1e-10 * expected.velocity.length()
  );
}

struct AnalyticSample {
  double mean;
  double elapsed_from_periapsis;
  StateVectors state;
};

AnalyticSample analytical(double e, double anomaly, double p, double mu) {
  if (e == 1) {
    const double d2 = anomaly * anomaly;
    const double mean = anomaly * (0.5 + d2 / 6);
    return {
        mean,
        p * std::sqrt(p / mu) * mean,
        {{p / 2 * (1 - d2), p * anomaly}, std::sqrt(mu / p) / (1 + d2) * Vec2d{-2 * anomaly, 2}}
    };
  }
  const double a = p / std::abs(1 - e * e);
  const double n = std::sqrt(mu / a) / a;
  if (e < 1) {
    const double c = std::cos(anomaly);
    const double s = std::sin(anomaly);
    const double shape = std::sqrt(1 - e * e);
    const double mean = anomaly - e * s;
    return {
        mean, mean / n, {{a * (c - e), a * shape * s}, a * n / (1 - e * c) * Vec2d{-s, shape * c}}
    };
  }
  const double c = std::cosh(anomaly);
  const double s = std::sinh(anomaly);
  const double shape = std::sqrt(e * e - 1);
  const double mean = e * s - anomaly;
  return {
      mean, mean / n, {{a * (e - c), a * shape * s}, a * n / (e * c - 1) * Vec2d{-s, shape * c}}
  };
}

double energy(StateVectors state, double mu) {
  return state.velocity.dot(state.velocity) / 2 - mu / state.position.length();
}

} // namespace

TEST_CASE("Universal propagation matches analytic conics independently of epoch initialization") {
  constexpr double MU = 8.82e13;
  constexpr double P = 3.4e6;
  constexpr double EPOCH = 12345;
  for (double e : {0.0, 0.3, 0.99999, 1.0, 1.00001, 1.7}) {
    CAPTURE(e);
    const double anomaly = e != 1 && std::abs(e - 1) < 0.01 ? 0.004 : 0.7;
    const OrbitState initial{EPOCH, analytical(e, 0, P, MU).state};
    const UniversalPropagator propagator(MU, initial);
    for (double direction : {-1.0, 1.0}) {
      CAPTURE(direction);
      const auto target = analytical(e, direction * anomaly, P, MU);
      const double ut = EPOCH + target.elapsed_from_periapsis;
      const auto actual = propagator.state_at(ut);
      check_state(actual, target.state);
      check_state({propagator.position_at(ut), propagator.velocity_at(ut)}, target.state);
      CHECK(energy(actual, MU) == Approx(energy(initial.state, MU)).margin(1e-6).epsilon(1e-10));
      CHECK(
          actual.position.cross(actual.velocity) ==
          Approx(initial.state.position.cross(initial.state.velocity)).epsilon(1e-10)
      );

      const auto seeded =
          state_at_epoch_from_elements(MU, EPOCH, {std::sqrt(MU * P), e, 0, target.mean});
      check_state(seeded.state, target.state);
      const UniversalPropagator seeded_propagator(MU, seeded);
      check_state(seeded_propagator.state_at(EPOCH - target.elapsed_from_periapsis), initial.state);
    }
  }
}

TEST_CASE("Circular phase is retained over many periods and signed elements rotate correctly") {
  constexpr double RADIUS = 4e6;
  constexpr double SPEED = 4000;
  constexpr double MU = RADIUS * SPEED * SPEED;
  const OrbitState initial{0, {{RADIUS, 0}, {0, SPEED}}};
  const UniversalPropagator propagator(MU, initial);
  // the representable UT is about 1.178 microseconds before the ideal
  // quarter turn, from a high-precision subtraction using 2*pi*1000.
  constexpr double UT = 6283186877.975912;
  constexpr double TIME_BEFORE_QUARTER = 1.1777056950603807e-6;
  for (double sign : {-1., 1.}) {
    const double angle = sign * (std::numbers::pi / 2 - TIME_BEFORE_QUARTER / 1000);
    check_state(
        propagator.state_at(sign * UT),
        {Vec2d{RADIUS, 0}.rotate(angle), Vec2d{0, SPEED}.rotate(angle)}
    );
  }
  constexpr FrameId CENTER{7};
  const auto clockwise = state_at_epoch_from_elements(
      MU, 123, {-RADIUS * SPEED, 0, std::numbers::pi / 2, std::numbers::pi / 2}, CENTER
  );
  check_state(clockwise.state, {{RADIUS, 0}, {0, -SPEED}});
  CHECK(clockwise.epoch == 123);
  CHECK(clockwise.frame_id == CENTER);
}

TEST_CASE("Periapsis at zero mean anomaly is valid for all conics") {
  for (double e : {0.2, 1.0, 1.4}) {
    CAPTURE(e);
    const auto seed = state_at_epoch_from_elements(10, 42, {4, e, 0, 0});
    CHECK(seed.frame_id == FrameId{});
    CHECK(seed.state.position.approx({1.6 / (1 + e), 0}));
    CHECK(seed.state.velocity.y == Approx(2.5 * (1 + e)));
  }
  CHECK_THROWS_AS(state_at_epoch_from_elements(0, 0, {1, 0, 0, 0}), std::invalid_argument);
}

TEST_CASE("Element initialization handles every conic with Unity's orientation convention") {
  struct Sample {
    double eccentricity;
    double anomaly;
  };
  // use nonzero anomalies on both sides of periapsis, including orbits inside unity's
  // old parabolic tolerance and unbound mean anomalies larger than a full turn.
  constexpr std::array SAMPLES{
      Sample{0, 0.7},
      Sample{0.4, 0.7},
      Sample{0.999995, 0.004},
      Sample{1, 4},
      Sample{1.000005, 0.004},
      Sample{1.7, 3},
  };
  constexpr double MU = 25;
  constexpr double P = 4;
  const double momentum = std::sqrt(MU * P);
  constexpr double EPOCH = 42;
  constexpr double LONGITUDE = std::numbers::pi / 2;

  for (const auto [eccentricity, anomaly] : SAMPLES) {
    for (const double sign : {-1.0, 1.0}) {
      CAPTURE(eccentricity, anomaly, sign);
      const auto expected = analytical(eccentricity, sign * anomaly, P, MU);
      const double mean =
          eccentricity < 1 ? expected.mean + sign * 4 * std::numbers::pi : expected.mean;
      const auto seed =
          state_at_epoch_from_elements(MU, EPOCH, {sign * momentum, eccentricity, LONGITUDE, mean});

      // unity mirrors the perifocal y component for clockwise motion, then rotates by omega.
      const auto [position, velocity] = expected.state;
      check_state(seed.state, {{-sign * position.y, position.x}, {-sign * velocity.y, velocity.x}});
      CHECK(seed.state.position.cross(seed.state.velocity) == Approx(sign * momentum));
      CHECK(seed.epoch == EPOCH);
    }
  }
}

TEST_CASE("Hyperbolic epoch states retain their radius near the asymptote") {
  constexpr double ECCENTRICITY = 2;
  for (const double anomaly : {-38.0, 38.0}) {
    CAPTURE(anomaly);
    const auto expected = analytical(ECCENTRICITY, anomaly, 1, 1);
    const auto seed = state_at_epoch_from_elements(1, 0, {1, ECCENTRICITY, 0, expected.mean});
    check_state(seed.state, expected.state);
    CHECK(
        seed.state.position.length() / expected.state.position.length() == Approx(1).epsilon(1e-13)
    );
  }

  constexpr double MEAN = std::numeric_limits<double>::max();
  const auto distant = state_at_epoch_from_elements(1, 0, {1, ECCENTRICITY, 0, MEAN});
  // for this orbit, r = (e cosh(F) - 1) / 3. at this scale r / M rounds to 1/3.
  CHECK(distant.state.position.length() / (MEAN / 3) == Approx(1).epsilon(1e-13));
}

TEST_CASE("Parabolic epoch states retain their radius far from periapsis") {
  constexpr double D = 1e20;
  const auto expected = analytical(1, D, 1, 1);
  const auto seed = state_at_epoch_from_elements(1, 0, {1, 1, 0, expected.mean});
  CHECK(seed.state.position.x / expected.state.position.x == Approx(1).epsilon(1e-13));
  CHECK(seed.state.position.y / expected.state.position.y == Approx(1).epsilon(1e-13));
  CHECK(seed.state.velocity.x / expected.state.velocity.x == Approx(1).epsilon(1e-13));
  CHECK(seed.state.velocity.y / expected.state.velocity.y == Approx(1).epsilon(1e-13));
}

TEST_CASE("Orbital geometry describes bound and unbound conics") {
  const OrbitState circular{0, {{4, 0}, {0, -2}}};
  const auto bound = geometry(16, circular);
  CHECK(bound.specific_angular_momentum == -8);
  CHECK(bound.eccentricity == 0);
  CHECK(bound.semilatus_rectum == 4);
  CHECK(bound.periapsis == 4);
  CHECK(bound.apoapsis == 4);
  CHECK(bound.period == Approx(4 * std::numbers::pi));

  const OrbitState escape{0, {{1, 0}, {0, 2}}};
  const auto hyperbolic = geometry(1, escape);
  CHECK(hyperbolic.eccentricity == 3);
  CHECK(hyperbolic.periapsis == 1);
  CHECK(std::isinf(hyperbolic.apoapsis));
  CHECK(std::isinf(hyperbolic.period));
  const auto parabolic = geometry(2, escape);
  CHECK(parabolic.eccentricity == 1);
  CHECK(parabolic.periapsis == 1);
  CHECK(std::isinf(parabolic.apoapsis));
  CHECK(std::isinf(parabolic.period));
}

TEST_CASE("A prepared propagator owns its seed and returns it exactly at epoch") {
  OrbitState seed{42, {{3, -4}, {1, -2}}};
  const UniversalPropagator propagator(20, seed);
  seed = {};
  const auto state = propagator.state_at(42);
  CHECK(state.position.x == 3);
  CHECK(state.position.y == -4);
  CHECK(state.velocity.x == 1);
  CHECK(state.velocity.y == -2);
  CHECK((propagator.position_at(42) - state.position).length() == 0);
  CHECK((propagator.velocity_at(42) - state.velocity).length() == 0);
}

TEST_CASE("Orbital calculations follow the caller's length and time units") {
  constexpr double LENGTH_SCALE = 10;
  constexpr double TIME_SCALE = 0.25;
  constexpr double SPEED_SCALE = LENGTH_SCALE / TIME_SCALE;
  constexpr double MU = 16;
  constexpr double SCALED_MU = MU * LENGTH_SCALE * SPEED_SCALE * SPEED_SCALE;
  const OrbitState seed{5, {{4, 0}, {0, 2}}};
  const OrbitState scaled_seed{
      seed.epoch * TIME_SCALE,
      {seed.state.position * LENGTH_SCALE, seed.state.velocity * SPEED_SCALE}
  };
  constexpr double UT = 19;
  const auto state = UniversalPropagator(MU, seed).state_at(UT);
  const auto scaled_state = UniversalPropagator(SCALED_MU, scaled_seed).state_at(UT * TIME_SCALE);
  check_state({scaled_state.position / LENGTH_SCALE, scaled_state.velocity / SPEED_SCALE}, state);

  const auto shape = geometry(MU, seed);
  const auto scaled_shape = geometry(SCALED_MU, scaled_seed);
  CHECK(scaled_shape.periapsis == Approx(shape.periapsis * LENGTH_SCALE));
  CHECK(scaled_shape.period == Approx(shape.period * TIME_SCALE));
}

TEST_CASE("Reseeding replaces the propagator's coefficients after validation") {
  const OrbitState circular{0, {{1, 0}, {0, 1}}};
  const OrbitState escape{12, {{2, 0}, {0, 3}}};
  UniversalPropagator propagator(1, circular);
  const UniversalPropagator expected(2, escape);

  propagator = UniversalPropagator{2, escape};
  check_state(propagator.state_at(12), escape.state);
  check_state(propagator.state_at(30), expected.state_at(30));
  CHECK_THROWS_AS((propagator = UniversalPropagator{0, circular}), std::invalid_argument);
  check_state(propagator.state_at(30), expected.state_at(30));
}

TEST_CASE("Extreme unit scales preserve circular geometry and quarter-turn propagation") {
  for (const int sign : {-1, 1}) {
    CAPTURE(sign);
    const double radius = std::scalbn(1.0, -80 * sign);
    const double speed = std::scalbn(1.0, 540 * sign);
    const double mu = std::scalbn(1.0, 1000 * sign);
    const double time_scale = radius / speed;
    const OrbitState seed{0, {{radius, 0}, {0, speed}}};
    const auto shape = geometry(mu, seed);
    CHECK(shape.eccentricity == 0);
    CHECK(shape.semilatus_rectum / radius == 1);
    CHECK(shape.periapsis / radius == 1);
    CHECK(shape.apoapsis / radius == 1);
    CHECK(shape.period / time_scale == Approx(2 * std::numbers::pi).epsilon(1e-13));

    const auto from_elements = state_at_epoch_from_elements(mu, 0, {radius * speed, 0, 0, 0});
    CHECK(from_elements.state.position.x / radius == 1);
    CHECK(from_elements.state.velocity.y / speed == 1);

    const UniversalPropagator propagator(mu, seed);
    const auto state = propagator.state_at(time_scale * (std::numbers::pi / 2));
    CHECK((state.position / radius - Vec2d{0, 1}).length() < 1e-13);
    CHECK((state.velocity / speed - Vec2d{-1, 0}).length() < 1e-13);
  }
}

TEST_CASE("Universal propagation rejects invalid seeds and query times") {
  constexpr double POSITIVE_INFINITY = std::numeric_limits<double>::infinity();
  constexpr double LARGEST = std::numeric_limits<double>::max();
  const OrbitState seed{0, {{1, 0}, {0, 1}}};
  const OrbitState zero_radius{0, {{}, {0, 1}}};
  const OrbitState invalid_velocity{0, {{1, 0}, {0, POSITIVE_INFINITY}}};
  const OrbitState invalid_epoch{POSITIVE_INFINITY, seed.state};
  CHECK_THROWS_AS(UniversalPropagator(0, seed), std::invalid_argument);
  CHECK_THROWS_AS(UniversalPropagator(POSITIVE_INFINITY, seed), std::invalid_argument);
  CHECK_THROWS_AS(UniversalPropagator(1, zero_radius), std::invalid_argument);
  CHECK_THROWS_AS(UniversalPropagator(1, invalid_velocity), std::invalid_argument);
  CHECK_THROWS_AS(UniversalPropagator(1, invalid_epoch), std::invalid_argument);
  CHECK_THROWS_AS(geometry(0, seed), std::invalid_argument);
  CHECK_THROWS_AS(geometry(1, zero_radius), std::invalid_argument);

  const UniversalPropagator propagator(1, seed);
  CHECK_THROWS_AS(propagator.state_at(POSITIVE_INFINITY), std::invalid_argument);
  CHECK_THROWS_AS(propagator.position_at(POSITIVE_INFINITY), std::invalid_argument);
  CHECK_THROWS_AS(propagator.velocity_at(POSITIVE_INFINITY), std::invalid_argument);
  const UniversalPropagator distant_epoch(1, {-LARGEST, seed.state});
  CHECK_THROWS_AS(distant_epoch.state_at(LARGEST), std::domain_error);
  CHECK_THROWS_AS(distant_epoch.position_at(LARGEST), std::domain_error);
  CHECK_THROWS_AS(distant_epoch.velocity_at(LARGEST), std::domain_error);
}
