// Headless craft simulation acceptance checks.
//
// These exercise the full simulation loop through a real Craft and the public
// API: command input updates CraftControl, actuators drain propellant and
// accumulate force/torque, CraftNewtonian integrates motion from a fresh
// post-drain mass snapshot and conditionally reseeds the orbit, and an
// exhausted engine flames out. Every numerical expectation is computed
// independently from the fixture constants, never by calling the production
// mass aggregator or Newtonian integrator.

#include <cmath>
#include <limits>
#include <map>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "flop/craft/craft.hpp"
#include "flop/craft/plugins/behavior_plugins.hpp"
#include "flop/craft/plugins/engine_plugin.hpp"
#include "flop/craft/plugins/part_plugin.hpp"
#include "flop/craft/plugins/resource_plugin.hpp"
#include "flop/math/utils.hpp"
#include "flop/orbit/propagators/universal.hpp"
#include "flop/orbit/state.hpp"

#include "support/stub_universe_context.hpp"

namespace {

using flop::Vec2d;
using flop::wrap_angle;
using flop::craft::Craft;
using flop::craft::CraftConfig;
using flop::craft::PartTypeConfig;
using flop::craft::ResourceFlow;
using flop::craft::ResourceTypeConfig;
using flop::craft::plugin::CommandConfig;
using flop::craft::plugin::CommandPlugin;
using flop::craft::plugin::EngineConfig;
using flop::craft::plugin::EnginePlugin;
using flop::craft::plugin::ReactionWheelConfig;
using flop::craft::plugin::ResourceConfig;
using flop::craft::plugin::ResourcePlugin;

// synthetic Hamswell frame used by every scenario: a single infinite-SOI parent.
constexpr double mu_hamswell = 8.82e13;

flop::orbit::RefFrameManager frames() {
  return flop::orbit::RefFrameManager(
      {{"Hamswell", mu_hamswell, {}, {}, std::numeric_limits<double>::infinity()}}
  );
}

// fixture constants shared by the primary numerical scenarios.
constexpr double engine_thrust_newtons = 980.0;
constexpr double engine_isp_sec = 100.0;
constexpr double standard_gravity = 9.8; // the g0 the engine model uses.
constexpr double wheel_torque_newton_meters = 40.0;
constexpr double pod_dry_kg = 400.0;
constexpr double tank_dry_kg = 300.0;
constexpr double engine_dry_kg = 200.0;
const Vec2d engine_position{1.0, -3.0};
const Vec2d tank_position{0.0, -2.0};
const Vec2d pod_position{0.0, 0.0};
constexpr double nto_density_kg_per_unit = 1.0;
constexpr double initial_fuel_units = 10.0;
constexpr double initial_attitude_rad = 0.25;

// the craft orbit seed, identical across scenarios. a cartesian seed in
// Hamswell's frame equivalent to the elements h=1.73e10, e=0.08, omega=1, M0=1.
flop::craft::CraftOrbitState orbit_seed() {
  return {
      "Hamswell",
      {{-1775017.642566629, 2763005.5625398336}, {-4632.602307965213, -2535.239056863811}},
      0,
  };
}

// engine craft-space thrust direction: local (1,0) rotated by the engine's
// pi/2 part angle lands on +y.
Vec2d craft_thrust_unit() {
  return Vec2d{1.0, 0.0}.rotate(std::numbers::pi / 2.0);
}

// install the parts in the deliberately adversarial order: engine, then tank,
// then the command pod last, so that correct command-before-actuator ordering
// cannot depend on part order.
CraftConfig primary_config(double fuel_units = initial_fuel_units) {
  CraftConfig config{"Primary", orbit_seed(), {initial_attitude_rad, 0.0}, {}};
  config.plugins_resolved = true;
  config.parts = {
      {"Engine",
       "engine",
       "Engine",
       engine_position,
       std::numbers::pi / 2.0,
       {{"fwd", "tank", "aft"}},
       {{"EnginePlugin",
         true,
         EngineConfig{engine_isp_sec, engine_thrust_newtons, {1, 0}, {{"NTO", 1.0}}}}}},
      {"Tank",
       "tank",
       "Tank",
       tank_position,
       0.0,
       {{"aft", "engine", "fwd"}, {"fwd", "pod", "aft"}},
       {{"ResourceContainerPlugin",
         true,
         ResourceConfig{{{"NTO", initial_fuel_units}}, {{"NTO", fuel_units}}, {}}}}},
      {"Pod",
       "pod",
       "Pod",
       pod_position,
       0.0,
       {{"aft", "tank", "fwd"}},
       {{"CommandPlugin", true, CommandConfig{}},
        {"ReactionWheelPlugin", true, ReactionWheelConfig{wheel_torque_newton_meters}}}},
  };
  return config;
}

std::vector<PartTypeConfig> primary_types() {
  return {
      {"Engine", "Engine", engine_dry_kg, true, {{"fwd", {0, 1}}}},
      {"Tank", "Tank", tank_dry_kg, true, {{"aft", {0, -1}}, {"fwd", {0, 1}}}},
      {"Pod", "Pod", pod_dry_kg, true, {{"aft", {0, -1}}}},
  };
}

// the same craft with the pod placed first, used to prove the physical outcome
// does not depend on part ordering.
CraftConfig pod_first_config(double fuel_units = initial_fuel_units) {
  CraftConfig config{"PodFirst", orbit_seed(), {initial_attitude_rad, 0.0}, {}};
  config.plugins_resolved = true;
  config.parts = {
      {"Pod",
       "pod",
       "Pod",
       pod_position,
       0.0,
       {{"aft", "tank", "fwd"}},
       {{"CommandPlugin", true, CommandConfig{}},
        {"ReactionWheelPlugin", true, ReactionWheelConfig{wheel_torque_newton_meters}}}},
      {"Tank",
       "tank",
       "Tank",
       tank_position,
       0.0,
       {{"aft", "engine", "fwd"}, {"fwd", "pod", "aft"}},
       {{"ResourceContainerPlugin",
         true,
         ResourceConfig{{{"NTO", initial_fuel_units}}, {{"NTO", fuel_units}}, {}}}}},
      {"Engine",
       "engine",
       "Engine",
       engine_position,
       std::numbers::pi / 2.0,
       {{"fwd", "tank", "aft"}},
       {{"EnginePlugin",
         true,
         EngineConfig{engine_isp_sec, engine_thrust_newtons, {1, 0}, {{"NTO", 1.0}}}}}},
  };
  return config;
}

flop::testing::StubUniverseContext primary_context() {
  return flop::testing::StubUniverseContext(
      frames(), {{"NTO", "N2O4", nto_density_kg_per_unit, ResourceFlow::stage}}
  );
}

CommandPlugin &command_of(Craft &craft) {
  for (const auto &plugin : craft.part("pod").installed_plugins()) {
    if (auto *command = dynamic_cast<CommandPlugin *>(plugin.get())) {
      return *command;
    }
  }
  FAIL("pod has no command plugin");
  throw std::logic_error("unreachable");
}

const EnginePlugin &engine_of(const Craft &craft) {
  for (const auto &plugin : craft.part("engine").installed_plugins()) {
    if (const auto *engine = dynamic_cast<const EnginePlugin *>(plugin.get())) {
      return *engine;
    }
  }
  FAIL("engine has no engine plugin");
  throw std::logic_error("unreachable");
}

// live NTO units reachable through any container on the tank.
double fuel_units_of(const Craft &craft) {
  for (const auto &plugin : craft.part("tank").installed_plugins()) {
    if (const auto *container = dynamic_cast<const ResourcePlugin *>(plugin.get())) {
      if (const auto *slot = container->find_slot("NTO")) {
        return slot->amount_units;
      }
    }
  }
  FAIL("tank has no NTO slot");
  throw std::logic_error("unreachable");
}

// independently computed point-mass properties for the primary fixture, given
// the post-drain fuel quantity on the tank.
struct Oracle {
  double total_mass_kg;
  Vec2d com;
  double moment_of_inertia_kg_m2;
};

Oracle mass_oracle(double fuel_units) {
  const double engine_mass = engine_dry_kg;
  const double tank_mass = tank_dry_kg + fuel_units * nto_density_kg_per_unit;
  const double pod_mass = pod_dry_kg;
  const double total = engine_mass + tank_mass + pod_mass;
  const Vec2d weighted =
      engine_mass * engine_position + tank_mass * tank_position + pod_mass * pod_position;
  const Vec2d com = weighted / total;
  const double inertia = engine_mass * (engine_position - com).dot(engine_position - com) +
                         tank_mass * (tank_position - com).dot(tank_position - com) +
                         pod_mass * (pod_position - com).dot(pod_position - com);
  return {total, com, inertia};
}

} // namespace

TEST_CASE("Headless burn integrates fresh mass, combined torque, and reseeds each step") {
  const auto context = primary_context();
  Craft craft(primary_config(), context, primary_types());

  // ramp the throttle to 0.5 over the first step with a 0.5/s rate, then hold.
  command_of(craft).set_throttle_input(0.5);
  command_of(craft).set_steering_input(0.25);

  const double steering = 0.25;
  const double dt = 1.0;
  const double mu = mu_hamswell;

  double fuel_before = initial_fuel_units;
  double prev_momentum = 0.0;
  double prev_angle = initial_attitude_rad;
  double prev_kick_magnitude = 0.0;

  for (int step_index = 0; step_index < 3; ++step_index) {
    if (step_index == 1) {
      // hold the throttle: a zero rate keeps 0.5, it does not cut it.
      command_of(craft).set_throttle_input(0.0);
    }
    const double now_ut = dt * (step_index + 1);

    // copy the prior seed and propagate it analytically to the upcoming epoch;
    // this is the coast state the kick is added onto.
    const auto seed_before = craft.orbit_state();
    const flop::orbit::UniversalPropagator coast(mu, seed_before);
    const auto coast_state = coast.state_at(now_ut);

    craft.step(now_ut, dt);

    // throttle integrates to 0.5 on the first step and holds afterwards.
    CHECK(craft.control().throttle() == Catch::Approx(0.5).epsilon(1e-9));
    CHECK(craft.control().steering() == Catch::Approx(steering).epsilon(1e-9));

    // this step consumes 0.5 units, including the first despite the pod running
    // its control phase after the engine's actuation phase would have appeared.
    const double fuel_after = fuel_before - 0.5;
    CHECK(fuel_units_of(craft) == Catch::Approx(fuel_after).epsilon(1e-9));

    // post-drain mass properties drive the integration this step.
    const Oracle oracle = mass_oracle(fuel_after);
    const auto mass = craft.mass_properties();
    CHECK(mass.total_mass_kg == Catch::Approx(oracle.total_mass_kg).epsilon(1e-9));
    CHECK(mass.center_of_mass_meters.approx(oracle.com, 1e-9));
    CHECK(
        mass.moment_of_inertia_kg_m2 == Catch::Approx(oracle.moment_of_inertia_kg_m2).epsilon(1e-9)
    );

    // craft-space thrust along +y, applied off-center at the engine.
    const Vec2d craft_force = 0.5 * engine_thrust_newtons * craft_thrust_unit();
    const Vec2d arm = engine_position - oracle.com;
    const double force_torque = arm.cross(craft_force);
    const double wheel_torque = wheel_torque_newton_meters * steering;
    CHECK(wheel_torque == Catch::Approx(10.0).epsilon(1e-9));
    const double net_torque = force_torque + wheel_torque;

    const double momentum = prev_momentum + net_torque * dt;
    const double omega = momentum / oracle.moment_of_inertia_kg_m2;
    const double angle = wrap_angle(prev_angle + omega * dt);
    CHECK(
        craft.newtonian().angular_momentum_kg_m2_per_s() == Catch::Approx(momentum).epsilon(1e-9)
    );
    CHECK(craft.newtonian().angular_velocity_rad_per_s() == Catch::Approx(omega).epsilon(1e-9));
    CHECK(craft.newtonian().angle_rad() == Catch::Approx(angle).epsilon(1e-9));

    // the velocity kick uses the updated attitude and the post-drain mass.
    const Vec2d kick = craft_force.rotate(angle) * dt / oracle.total_mass_kg;

    const auto seed_after = craft.orbit_state();
    CHECK(seed_after.epoch == Catch::Approx(now_ut));
    CHECK(seed_after.frame_id == seed_before.frame_id);
    CHECK(seed_after.state.position.approx(coast_state.position, 1e-9, 1e-6));
    CHECK((seed_after.state.velocity - coast_state.velocity).approx(kick, 1e-9, 1e-8));

    // equal thrust against a lightening craft gives a growing kick.
    const double kick_magnitude = kick.length();
    if (step_index > 0) {
      CHECK(kick_magnitude > prev_kick_magnitude);
    }

    fuel_before = fuel_after;
    prev_momentum = momentum;
    prev_angle = angle;
    prev_kick_magnitude = kick_magnitude;
  }
}

TEST_CASE("Headless control-phase ordering is independent of part ordering") {
  const auto context = primary_context();
  Craft engine_first(primary_config(), context, primary_types());
  Craft pod_first(pod_first_config(), context, primary_types());

  for (Craft *craft : {&engine_first, &pod_first}) {
    command_of(*craft).set_throttle_input(0.5);
    command_of(*craft).set_steering_input(0.25);
    craft->step(1.0, 1.0);
  }

  CHECK(engine_first.control().throttle() == Catch::Approx(pod_first.control().throttle()));
  CHECK(engine_first.control().steering() == Catch::Approx(pod_first.control().steering()));
  CHECK(fuel_units_of(engine_first) == Catch::Approx(fuel_units_of(pod_first)).epsilon(1e-12));

  const auto mass_a = engine_first.mass_properties();
  const auto mass_b = pod_first.mass_properties();
  CHECK(mass_a.total_mass_kg == Catch::Approx(mass_b.total_mass_kg).epsilon(1e-12));
  CHECK(mass_a.center_of_mass_meters.approx(mass_b.center_of_mass_meters, 1e-12));
  CHECK(
      mass_a.moment_of_inertia_kg_m2 == Catch::Approx(mass_b.moment_of_inertia_kg_m2).epsilon(1e-12)
  );

  CHECK(
      engine_first.newtonian().angle_rad() ==
      Catch::Approx(pod_first.newtonian().angle_rad()).epsilon(1e-12)
  );
  CHECK(
      engine_first.newtonian().angular_momentum_kg_m2_per_s() ==
      Catch::Approx(pod_first.newtonian().angular_momentum_kg_m2_per_s()).epsilon(1e-12)
  );
  CHECK(engine_first.orbit_state().epoch == pod_first.orbit_state().epoch);
  CHECK(engine_first.orbit_state().state.approx(pod_first.orbit_state().state, 1e-12, 1e-12));
}

TEST_CASE("Headless pure steering spins the craft without burning or reseeding") {
  const auto context = primary_context();

  for (const double steering : {0.25, -0.25}) {
    Craft craft(primary_config(), context, primary_types());
    command_of(craft).set_steering_input(steering);

    const auto seed_before = craft.orbit_state();
    const double fuel_before = fuel_units_of(craft);

    const double dt = 1.0;
    const double momentum_before = craft.newtonian().angular_momentum_kg_m2_per_s();
    craft.step(1.0, dt);

    // wheel-only torque: delta L = 40 * steering * dt = +/-10.
    const double expected_delta = wheel_torque_newton_meters * steering * dt;
    CHECK(
        craft.newtonian().angular_momentum_kg_m2_per_s() - momentum_before ==
        Catch::Approx(expected_delta).epsilon(1e-9)
    );
    CHECK(std::abs(expected_delta) == Catch::Approx(10.0).epsilon(1e-9));

    // no throttle: no drain and no reseed, so the seed is untouched.
    CHECK(fuel_units_of(craft) == fuel_before);
    CHECK(craft.orbit_state().epoch == seed_before.epoch);
    CHECK(craft.orbit_state().state == seed_before.state);

    // neutralize steering and step again: momentum holds (torque was cleared),
    // but attitude keeps advancing from the standing spin.
    command_of(craft).set_steering_input(0.0);
    const double momentum_latched = craft.newtonian().angular_momentum_kg_m2_per_s();
    const double angle_before = craft.newtonian().angle_rad();
    const double omega = craft.newtonian().angular_velocity_rad_per_s();
    craft.step(2.0, dt);
    CHECK(
        craft.newtonian().angular_momentum_kg_m2_per_s() ==
        Catch::Approx(momentum_latched).epsilon(1e-12)
    );
    CHECK(
        craft.newtonian().angle_rad() ==
        Catch::Approx(wrap_angle(angle_before + omega * dt)).epsilon(1e-9)
    );
    CHECK(craft.orbit_state().epoch == seed_before.epoch);
  }
}

TEST_CASE("Headless throttle cut stops the burn and holds the seed while coasting") {
  const auto context = primary_context();
  Craft craft(primary_config(), context, primary_types());

  // a powered step first.
  command_of(craft).set_throttle_input(0.5);
  command_of(craft).set_steering_input(0.0);
  craft.step(1.0, 1.0);
  REQUIRE(craft.orbit_state().epoch == Catch::Approx(1.0));
  const double fuel_after_burn = fuel_units_of(craft);

  // drive the throttle back to zero with a sufficient negative rate.
  command_of(craft).set_throttle_input(-1.0);
  craft.step(2.0, 1.0);
  REQUIRE(craft.control().throttle() == Catch::Approx(0.0));
  // this step still drew nothing (throttle only reaches zero via integration,
  // but the engine saw throttle 0 on this tick because control runs first).
  const double fuel_after_cut = fuel_units_of(craft);

  // from here the craft coasts: the seed must be preserved exactly.
  const auto coasting_seed = craft.orbit_state();
  const double fuel_coasting = fuel_units_of(craft);
  for (double ut = 3.0; ut <= 8.0; ut += 1.0) {
    craft.step(ut, 1.0);
    CHECK(craft.orbit_state().epoch == coasting_seed.epoch);
    CHECK(craft.orbit_state().state == coasting_seed.state);
    CHECK(craft.orbit_state().frame_id == coasting_seed.frame_id);
    CHECK(fuel_units_of(craft) == fuel_coasting);
  }
  CHECK(fuel_after_cut == Catch::Approx(fuel_after_burn).epsilon(1e-12));
}

TEST_CASE("Headless partial final burn scales thrust and latches flame-out on the next step") {
  const auto context = primary_context();
  Craft craft(primary_config(0.25), context, primary_types());

  // full throttle in one step; only 0.25 units are available.
  command_of(craft).set_throttle_input(1.0);
  command_of(craft).set_steering_input(0.0);

  const auto seed_before = craft.orbit_state();
  const flop::orbit::UniversalPropagator coast(mu_hamswell, seed_before);
  const auto coast_state = coast.state_at(1.0);

  craft.step(1.0, 1.0);

  // nominal full-throttle flow is 1 unit/s, only 0.25 available: factor 0.25.
  const double factor = 0.25;
  CHECK(craft.control().throttle() == Catch::Approx(1.0));
  CHECK(fuel_units_of(craft) == Catch::Approx(0.0).margin(1e-12));
  CHECK(fuel_units_of(craft) >= 0.0);

  // post-drain mass is dry mass only.
  const Oracle oracle = mass_oracle(0.0);
  CHECK(oracle.total_mass_kg == Catch::Approx(pod_dry_kg + tank_dry_kg + engine_dry_kg));
  const auto mass = craft.mass_properties();
  CHECK(mass.total_mass_kg == Catch::Approx(oracle.total_mass_kg).epsilon(1e-9));
  CHECK(mass.center_of_mass_meters.approx(oracle.com, 1e-9));

  // scaled thrust of 245 N and its torque drove the step.
  const Vec2d craft_force = factor * engine_thrust_newtons * craft_thrust_unit();
  CHECK((factor * engine_thrust_newtons) == Catch::Approx(245.0));
  const double net_torque = (engine_position - oracle.com).cross(craft_force);
  const double momentum = net_torque * 1.0;
  const double omega = momentum / oracle.moment_of_inertia_kg_m2;
  const double angle = wrap_angle(initial_attitude_rad + omega * 1.0);
  const Vec2d kick = craft_force.rotate(angle) / oracle.total_mass_kg;

  CHECK(craft.orbit_state().epoch == Catch::Approx(1.0));
  CHECK((craft.orbit_state().state.velocity - coast_state.velocity).approx(kick, 1e-9, 1e-8));

  // the engine stays enabled after a propellant-limited but nonzero burn.
  CHECK(engine_of(craft).enabled());

  // next positive-throttle step: now there is no fuel, so the engine disables,
  // nothing drains, and the seed is left exactly where the burn put it.
  const auto latched_seed = craft.orbit_state();
  command_of(craft).set_throttle_input(0.0); // hold full throttle.
  craft.step(2.0, 1.0);
  CHECK_FALSE(engine_of(craft).enabled());
  CHECK(fuel_units_of(craft) == Catch::Approx(0.0).margin(1e-12));
  CHECK(craft.orbit_state().epoch == latched_seed.epoch);
  CHECK(craft.orbit_state().state == latched_seed.state);

  // several further steps keep both mass and seed stable (spin may continue).
  const double stable_mass = craft.mass_properties().total_mass_kg;
  for (double ut = 3.0; ut <= 6.0; ut += 1.0) {
    craft.step(ut, 1.0);
    CHECK(craft.mass_properties().total_mass_kg == Catch::Approx(stable_mass).epsilon(1e-12));
    CHECK(craft.orbit_state().state == latched_seed.state);
  }
}

TEST_CASE("Headless mixed-density burn drains per the inherited mixture formula") {
  // two stage-flow propellants with densities 1 and 2 kg/unit, ratio 1:1.
  flop::testing::StubUniverseContext context(
      frames(),
      {{"NTO", "N2O4", 1.0, ResourceFlow::stage}, {"UDMH", "UDMH", 2.0, ResourceFlow::stage}}
  );

  CraftConfig config{"Mixed", orbit_seed(), {initial_attitude_rad, 0.0}, {}};
  config.plugins_resolved = true;
  const double nto_start = 10.0;
  const double udmh_start = 1.0 / 24.0;
  config.parts = {
      {"Engine",
       "engine",
       "Engine",
       engine_position,
       std::numbers::pi / 2.0,
       {{"fwd", "tank", "aft"}},
       {{"EnginePlugin",
         true,
         EngineConfig{
             engine_isp_sec, engine_thrust_newtons, {1, 0}, {{"NTO", 1.0}, {"UDMH", 1.0}}
         }}}},
      {"Tank",
       "tank",
       "Tank",
       tank_position,
       0.0,
       {{"aft", "engine", "fwd"}, {"fwd", "pod", "aft"}},
       {{"ResourceContainerPlugin",
         true,
         ResourceConfig{
             {{"NTO", nto_start}, {"UDMH", udmh_start}},
             {{"NTO", nto_start}, {"UDMH", udmh_start}},
             {}
         }}}},
      {"Pod",
       "pod",
       "Pod",
       pod_position,
       0.0,
       {{"aft", "tank", "fwd"}},
       {{"CommandPlugin", true, CommandConfig{}}}},
  };
  const std::vector<PartTypeConfig> types = primary_types();

  Craft craft(config, context, types);
  command_of(craft).set_throttle_input(1.0);

  const auto seed_before = craft.orbit_state();
  const flop::orbit::UniversalPropagator coast(mu_hamswell, seed_before);
  const auto coast_state = coast.state_at(1.0);

  craft.step(1.0, 1.0);

  // R = 1*1 + 1*2 = 3; c = dt*thrust/(isp*g0)/R = 980/(100*9.8)/3 = 1/3.
  // f = min(1, available_i * density_i / (c * ratio_i)).
  //   NTO:  10 * 1 / (1/3) = 30  -> not limiting.
  //   UDMH: (1/24) * 2 / (1/3) = 1/4 -> limiting, so f = 1/4.
  const double c = 1.0 / 3.0;
  const double f = 1.0 / 4.0;
  const double nto_drained = c * f * 1.0 / 1.0;  // 1/12.
  const double udmh_drained = c * f * 1.0 / 2.0; // 1/24.
  CHECK(nto_drained == Catch::Approx(1.0 / 12.0).epsilon(1e-12));
  CHECK(udmh_drained == Catch::Approx(1.0 / 24.0).epsilon(1e-12));

  double nto_live = 0.0;
  double udmh_live = 0.0;
  for (const auto &plugin : craft.part("tank").installed_plugins()) {
    if (const auto *container = dynamic_cast<const ResourcePlugin *>(plugin.get())) {
      nto_live = container->find_slot("NTO")->amount_units;
      udmh_live = container->find_slot("UDMH")->amount_units;
    }
  }
  CHECK(nto_live == Catch::Approx(nto_start - nto_drained).epsilon(1e-12));
  CHECK(udmh_live == Catch::Approx(0.0).margin(1e-12));

  // remaining wet mass from live quantities times densities.
  const double fuel_mass = nto_live * 1.0 + udmh_live * 2.0;
  const double expected_total = pod_dry_kg + tank_dry_kg + engine_dry_kg + fuel_mass;
  CHECK(craft.mass_properties().total_mass_kg == Catch::Approx(expected_total).epsilon(1e-9));

  // scaled impulse: effective thrust = thrust * f = 245 N.
  const Vec2d craft_force = f * engine_thrust_newtons * craft_thrust_unit();
  const double angle = craft.newtonian().angle_rad();
  const Vec2d kick = craft_force.rotate(angle) * 1.0 / expected_total;
  CHECK((craft.orbit_state().state.velocity - coast_state.velocity).approx(kick, 1e-9, 1e-8));

  // flame-out on the next step despite NTO leftovers (UDMH is gone).
  craft.step(2.0, 1.0);
  CHECK_FALSE(engine_of(craft).enabled());
  CHECK(nto_live > 0.0);
}
