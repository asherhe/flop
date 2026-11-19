#include <numbers>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "flop/craft/config.hpp"
#include "flop/craft/newtonian.hpp"
#include "flop/craft/resources.hpp"
#include "flop/math/vec2.hpp"
#include "flop/orbit/state.hpp"

#include "support/stub_craft_context.hpp"

namespace {
using Catch::Approx;
using flop::Vec2d;
using flop::craft::CraftAttitudeConfig;
using flop::craft::CraftNewtonian;
using flop::orbit::StateVectors;
using flop::testing::StubCraftContext;

// a context reporting a single mass snapshot centered at the origin (or the
// given com) with the given total mass and inertia, as the step reads once.
StubCraftContext ctx(double total_mass_kg, double inertia_kg_m2, Vec2d com = {}) {
  StubCraftContext context;
  context.with_mass_properties({total_mass_kg, com, inertia_kg_m2});
  return context;
}
} // namespace

TEST_CASE("CraftNewtonian starts from the configured attitude") {
  const CraftAttitudeConfig attitude{std::numbers::pi / 2, 10};
  const CraftNewtonian newtonian(attitude);
  CHECK(newtonian.angle_rad() == Approx(std::numbers::pi / 2));
  CHECK(newtonian.angular_momentum_kg_m2_per_s() == Approx(10));
  CHECK(newtonian.angular_velocity_rad_per_s() == 0);
}

TEST_CASE("CraftNewtonian wraps the configured angle into [0, 2pi)") {
  const CraftNewtonian newtonian(CraftAttitudeConfig{-std::numbers::pi / 2, 0});
  CHECK(newtonian.angle_rad() == Approx(3 * std::numbers::pi / 2));
}

TEST_CASE("CraftNewtonian force through the center of mass only accelerates") {
  CraftNewtonian newtonian(CraftAttitudeConfig{0, 0});
  StateVectors state{};
  // 200 N on a 100 kg craft, applied at the center of mass. attitude is 0, so
  // craft space and the frame coincide.
  newtonian.apply_force({200, 0}, {0, 0});
  CHECK(newtonian.step(ctx(100, 50), state, 2.0));
  // dv = F/m * dt = 200/100 * 2 = 4 m/s
  CHECK(state.velocity.approx({4, 0}));
  // no arm, so no torque and no rotation.
  CHECK(newtonian.angular_momentum_kg_m2_per_s() == Approx(0));
  CHECK(newtonian.angle_rad() == Approx(0));
}

TEST_CASE("CraftNewtonian off-center force produces torque about the center of mass") {
  CraftNewtonian newtonian(CraftAttitudeConfig{0, 0});
  StateVectors state{};
  // force +x applied 2 m above the com: arm (0, 2) x force (10, 0) = -20 N*m
  newtonian.apply_force({10, 0}, {0, 2});
  newtonian.step(ctx(100, 40), state, 1.0);
  CHECK(newtonian.angular_momentum_kg_m2_per_s() == Approx(-20));
  // angular velocity = L / I = -20 / 40 = -0.5 rad/s; angle wraps negative.
  CHECK(newtonian.angular_velocity_rad_per_s() == Approx(-0.5));
  CHECK(newtonian.angle_rad() == Approx(2 * std::numbers::pi - 0.5));
  // the force rotates with the (now nonzero) attitude, so only its magnitude is
  // fixed: |dv| = |F|/m * dt = 10/100 * 1.
  CHECK(state.velocity.length() == Approx(0.1));
}

TEST_CASE("CraftNewtonian torque arm uses the snapshot center of mass") {
  CraftNewtonian newtonian(CraftAttitudeConfig{0, 0});
  StateVectors state{};
  // same applied point, but the com is at (0, 2): arm is zero, so no torque.
  newtonian.apply_force({10, 0}, {0, 2});
  newtonian.step(ctx(100, 40, {0, 2}), state, 1.0);
  CHECK(newtonian.angular_momentum_kg_m2_per_s() == Approx(0));
  CHECK(newtonian.angle_rad() == Approx(0));
  // no rotation, so the +x force stays +x in the frame.
  CHECK(state.velocity.approx({0.1, 0}));
}

TEST_CASE("CraftNewtonian applies a pure torque without linear motion") {
  CraftNewtonian newtonian(CraftAttitudeConfig{0, 0});
  StateVectors state{};
  newtonian.apply_torque(8);
  CHECK_FALSE(newtonian.step(ctx(100, 4), state, 0.5));
  CHECK(state.velocity.approx({0, 0}));
  // L = torque * dt = 8 * 0.5 = 4; omega = 4 / 4 = 1 rad/s; angle = 1 * 0.5 = 0.5
  CHECK(newtonian.angular_momentum_kg_m2_per_s() == Approx(4));
  CHECK(newtonian.angular_velocity_rad_per_s() == Approx(1));
  CHECK(newtonian.angle_rad() == Approx(0.5));
}

TEST_CASE("CraftNewtonian with zero mass and inertia produces no motion or NaN") {
  CraftNewtonian newtonian(CraftAttitudeConfig{0, 5});
  StateVectors state{{1, 2}, {3, 4}};
  newtonian.apply_force({100, 100}, {1, 1});
  newtonian.apply_torque(50);
  CHECK_FALSE(newtonian.step(ctx(0, 0), state, 1.0));
  // zero mass leaves velocity untouched.
  CHECK(state.velocity.x == 3);
  CHECK(state.velocity.y == 4);
  CHECK(newtonian.angular_velocity_rad_per_s() == 0);
  // no inertia means no integration of spin this step.
  CHECK(newtonian.angular_momentum_kg_m2_per_s() == Approx(5));
  CHECK(newtonian.angle_rad() == Approx(0));
}

TEST_CASE("CraftNewtonian accumulates several forces before a step") {
  CraftNewtonian newtonian(CraftAttitudeConfig{0, 0});
  StateVectors state{};
  newtonian.apply_force({10, 0}, {0, 0});
  newtonian.apply_force({0, 20}, {0, 0});
  newtonian.step(ctx(10, 100), state, 1.0);
  CHECK(state.velocity.approx({1, 2})); // 10/10, 20/10
}

TEST_CASE("CraftNewtonian step clears accumulators") {
  CraftNewtonian newtonian(CraftAttitudeConfig{0, 0});
  StateVectors state{};
  newtonian.apply_force({10, 0}, {0, 0});
  newtonian.step(ctx(10, 100), state, 1.0);
  const Vec2d after_first = state.velocity;
  // a second step with nothing accumulated changes nothing and reports no change.
  CHECK_FALSE(newtonian.step(ctx(10, 100), state, 1.0));
  CHECK(state.velocity.approx(after_first));
}

TEST_CASE("CraftNewtonian gives a lighter craft more acceleration for the same force") {
  CraftNewtonian heavy(CraftAttitudeConfig{0, 0});
  CraftNewtonian light(CraftAttitudeConfig{0, 0});
  StateVectors heavy_state{};
  StateVectors light_state{};
  heavy.apply_force({100, 0}, {0, 0});
  heavy.step(ctx(1000, 10), heavy_state, 1.0);
  // same force on a craft that has since drained fuel down to 500 kg.
  light.apply_force({100, 0}, {0, 0});
  light.step(ctx(500, 10), light_state, 1.0);
  CHECK(light_state.velocity.x > heavy_state.velocity.x);
  CHECK(heavy_state.velocity.x == Approx(0.1));
  CHECK(light_state.velocity.x == Approx(0.2));
}
