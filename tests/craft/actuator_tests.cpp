#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "flop/craft/control.hpp"
#include "flop/craft/newtonian.hpp"
#include "flop/craft/parts/part.hpp"
#include "flop/craft/plugins/plugins.hpp"
#include "flop/craft/plugins/engine.hpp"
#include "flop/craft/plugins/config.hpp"
#include "flop/craft/plugins/resource.hpp"
#include "flop/craft/resource_type_config.hpp"
#include "flop/craft/resources.hpp"
#include "flop/math/vec2.hpp"
#include "flop/orbit/state.hpp"

#include "support/stub_craft_context.hpp"
#include "support/stub_universe_context.hpp"

namespace {
using Catch::Approx;
using flop::Vec2d;
using flop::craft::CraftNewtonian;
using flop::craft::CraftResources;
using flop::craft::MassProperties;
using flop::craft::Part;
using flop::craft::ResourceFlow;
using flop::craft::plugin::EngineConfig;
using flop::craft::plugin::EnginePlugin;
using flop::craft::plugin::ReactionWheelConfig;
using flop::craft::plugin::ReactionWheelPlugin;
using flop::craft::plugin::ResourceConfig;
using flop::craft::plugin::ResourcePlugin;
using flop::testing::StubCraftContext;
using flop::testing::StubUniverseContext;

constexpr double standard_gravity = 9.8;

flop::orbit::RefFrameManager frames() {
  return flop::orbit::RefFrameManager(
      {{"Hamswell", 8.82e13, {}, {}, std::numeric_limits<double>::infinity()}}
  );
}

// a registry with two propellants of different densities, so a mixture test
// exercises the per-propellant mass accounting.
StubUniverseContext context() {
  return StubUniverseContext(
      frames(),
      {{"NTO", "N2O4", 1.0, ResourceFlow::stage}, {"Fuel", "Hydrazine", 2.0, ResourceFlow::stage}}
  );
}

// one part carrying an engine and a propellant tank, with the plugins installed
// and bound to part index zero.
std::vector<Part> engine_part(
    const StubUniverseContext &universe, EngineConfig engine, ResourceConfig tank,
    bool emitter = false
) {
  std::vector<Part> parts;
  Part part;
  part.type_id = "Engine";
  part.id = "engine_0";
  part.position_meters = {0, 0};
  part.plugins.push_back({"EnginePlugin", true, std::move(engine)});
  part.plugins.push_back({"ResourceContainerPlugin", true, std::move(tank)});
  if (emitter)
    part.plugins.push_back({"ParticlePlugin", true, flop::craft::plugin::ParticleConfig{}});
  parts.push_back(std::move(part));
  parts[0].install_plugins(universe.resources(), 0);
  return parts;
}

EnginePlugin &engine_of(std::vector<Part> &parts);

TEST_CASE("engine publishes actual propellant-limited emission") {
  const auto universe = context();
  const double prop_flow = 1000.0 / (200.0 * standard_gravity);
  auto parts = engine_part(
      universe, EngineConfig{200.0, 1000.0, {0, 1}, {{"NTO", 1.0}}},
      ResourceConfig{{{"NTO", 1000}}, {{"NTO", 0.25 * prop_flow}}, {}}, true
  );
  auto *emitter = dynamic_cast<flop::craft::plugin::ParticlePlugin *>(
      parts[0].find_plugin(flop::craft::PluginId("ParticlePlugin"))
  );
  REQUIRE(emitter);
  flop::craft::CraftControl control;
  control.set_throttle(1.0);
  CraftNewtonian newtonian;
  CraftResources resources(parts, universe);
  StubCraftContext ctx;
  ctx.with_parts(parts).with_mutable_parts(parts).with_control(control).with_resources(resources).with_newtonian(newtonian);
  engine_of(parts).tick(ctx, 1.0);
  CHECK(emitter->emission_strength() == Approx(0.25));
  emitter->reset_step();
  engine_of(parts).tick(ctx, 1.0);
  CHECK(emitter->emission_strength() == 0.0);
  CHECK_FALSE(engine_of(parts).enabled());
}

TEST_CASE("engine transitions clear controlled outputs without a simulation step") {
  const auto universe = context();
  auto parts = engine_part(universe, EngineConfig{}, ResourceConfig{}, true);
  flop::craft::plugin::LightConfig light_config;
  light_config.strength = 0.0;
  parts[0].plugins.push_back({"LightPlugin", true, light_config});
  parts[0].install_plugins(universe.resources(), 0);
  auto &engine = engine_of(parts);
  auto *emitter = parts[0].get_plugin<flop::craft::plugin::ParticlePlugin>();
  auto *light = parts[0].get_plugin<flop::craft::plugin::LightPlugin>();
  REQUIRE(emitter);
  REQUIRE(light);
  CHECK(light->strength() == 0.0);
  emitter->set_emission_strength(0.8);
  light->set_strength(0.8);
  engine.set_enabled(false);
  CHECK(emitter->emission_strength() == 0.0);
  CHECK(light->strength() == 0.0);
  emitter->set_emission_strength(0.6);
  light->set_strength(0.6);
  engine.set_enabled(false);
  CHECK(light->strength() == 0.6);
  engine.set_enabled(true);
  CHECK(emitter->emission_strength() == 0.0);
  CHECK(light->strength() == 0.0);
  light->set_strength(0.7);
  light->set_enabled(false);
  CHECK(light->strength() == 0.0);
  light->set_enabled(true);
  CHECK(light->strength() == 0.0);
}

TEST_CASE("engine light follows propellant-limited thrust and simulation time") {
  const auto universe = context();
  const double prop_flow = 1000.0 / (200.0 * standard_gravity);
  EngineConfig config{200.0, 1000.0, {0, 1}, {{"NTO", 1.0}}};
  config.flicker_speed = 2.0;
  config.flicker_min = 1.3;
  config.flicker_max = 1.3;
  auto parts = engine_part(universe, config,
      ResourceConfig{{{"NTO", 1000}}, {{"NTO", 0.25 * prop_flow}}, {}}, true);
  flop::craft::plugin::LightConfig light_config;
  light_config.strength = 0.0;
  parts[0].plugins.push_back({"LightPlugin", true, light_config});
  parts[0].install_plugins(universe.resources(), 0);
  auto *light = parts[0].get_plugin<flop::craft::plugin::LightPlugin>();
  auto *emitter = parts[0].get_plugin<flop::craft::plugin::ParticlePlugin>();
  REQUIRE(light);
  REQUIRE(emitter);
  flop::craft::CraftControl control;
  control.set_throttle(1.0);
  CraftNewtonian newtonian;
  CraftResources resources(parts, universe);
  StubCraftContext ctx;
  ctx.with_parts(parts).with_mutable_parts(parts).with_control(control)
      .with_resources(resources).with_newtonian(newtonian).with_time(0.0).with_runtime_id(7);
  engine_of(parts).tick(ctx, 1.0);
  CHECK(light->strength() == Approx(0.25 * 1.3));
  CHECK(emitter->emission_strength() == Approx(0.25));
  const auto retained = light->strength();
  CHECK(light->strength() == retained); // a frame with no tick keeps the last output
  engine_of(parts).reset_step();
  CHECK(light->strength() == 0.0);
  engine_of(parts).tick(ctx.with_time(0.5), 1.0);
  CHECK_FALSE(engine_of(parts).enabled());
  CHECK(light->strength() == 0.0);
}

TEST_CASE("engine flicker is deterministic and independent across craft") {
  const auto universe = context();
  EngineConfig config{200.0, 1000.0, {0, 1}, {{"NTO", 1.0}}};
  config.flicker_speed = 1.0;
  config.flicker_min = 0.0;
  config.flicker_max = 2.0;
  auto parts = engine_part(universe, config, ResourceConfig{{{"NTO", 1000}}, {{"NTO", 1000}}, {}});
  flop::craft::plugin::LightConfig light_config;
  light_config.strength = 0.0;
  parts[0].plugins.push_back({"LightPlugin", true, light_config});
  parts[0].install_plugins(universe.resources(), 0);
  auto *light = parts[0].get_plugin<flop::craft::plugin::LightPlugin>();
  REQUIRE(light);
  flop::craft::CraftControl control;
  control.set_throttle(1.0);
  CraftNewtonian newtonian;
  CraftResources resources(parts, universe);
  StubCraftContext ctx;
  ctx.with_parts(parts).with_mutable_parts(parts).with_control(control)
      .with_resources(resources).with_newtonian(newtonian).with_time(0.25).with_runtime_id(1);
  engine_of(parts).tick(ctx, 0.01);
  const double first = light->strength();
  engine_of(parts).tick(ctx, 0.01);
  CHECK(light->strength() == Approx(first));
  engine_of(parts).tick(ctx.with_runtime_id(2), 0.01);
  CHECK(light->strength() != Approx(first));
  engine_of(parts).tick(ctx.with_runtime_id(1).with_time(0.5), 0.01);
  CHECK(light->strength() != Approx(first));
}

TEST_CASE("part moves rebind installed plugins to their owner") {
  const auto universe = context();
  auto parts = engine_part(universe, EngineConfig{}, ResourceConfig{}, true);
  Part moved(std::move(parts[0]));
  auto *engine = moved.get_plugin<EnginePlugin>();
  auto *emitter = moved.get_plugin<flop::craft::plugin::ParticlePlugin>();
  REQUIRE(engine);
  REQUIRE(emitter);
  CHECK(&engine->part() == &moved);
  Part assigned;
  assigned = std::move(moved);
  CHECK(&engine->part() == &assigned);
  emitter->set_emission_strength(1.0);
  engine->set_enabled(false);
  CHECK(emitter->emission_strength() == 0.0);
}

EnginePlugin &engine_of(std::vector<Part> &parts) {
  if (auto *engine = parts[0].get_plugin<EnginePlugin>()) return *engine;
  FAIL("part has no engine plugin");
  throw std::logic_error("unreachable");
}

double propellant_units(const std::vector<Part> &parts, const std::string &resource_id) {
  for (const auto &plugin : parts[0].installed_plugins()) {
    if (const auto *tank = dynamic_cast<const ResourcePlugin *>(plugin.get())) {
      if (const auto *slot = tank->find_slot(resource_id)) {
        return slot->amount_units;
      }
    }
  }
  FAIL("part has no slot for resource");
  throw std::logic_error("unreachable");
}

} // namespace

TEST_CASE("ReactionWheelPlugin applies torque scaled by the steering command") {
  flop::craft::CraftControl control;
  CraftNewtonian newtonian;
  StubCraftContext ctx;
  ctx.with_control(control).with_newtonian(newtonian).with_mass_properties(
      MassProperties{100.0, {0, 0}, 40.0}
  );

  ReactionWheelPlugin wheel(ReactionWheelConfig{8.0}, true);

  control.set_steering(1.0);
  wheel.tick(ctx, 1.0);
  flop::orbit::StateVectors state{};
  // torque = 8 * 1 = 8 N*m; over I = 40, omega = L/I = 8*1/40 = 0.2 rad/s.
  CHECK_FALSE(newtonian.step(ctx, state, 1.0));
  CHECK(newtonian.angular_velocity_rad_per_s() == Approx(0.2));
}

TEST_CASE("ReactionWheelPlugin applies no torque when steering is neutral") {
  flop::craft::CraftControl control;
  CraftNewtonian newtonian;
  StubCraftContext ctx;
  ctx.with_control(control).with_newtonian(newtonian).with_mass_properties(
      MassProperties{100.0, {0, 0}, 40.0}
  );

  ReactionWheelPlugin wheel(ReactionWheelConfig{8.0}, true);
  control.set_steering(0.0);
  wheel.tick(ctx, 1.0);
  flop::orbit::StateVectors state{};
  CHECK_FALSE(newtonian.step(ctx, state, 1.0));
  CHECK(newtonian.angular_velocity_rad_per_s() == Approx(0.0));
}

TEST_CASE("EnginePlugin at full throttle thrusts and drains its propellant") {
  const auto universe = context();
  // thrust along +y, isp 200 s, one propellant (NTO, 1 kg/unit) with a large
  // tank so the burn is not propellant-limited.
  auto parts = engine_part(
      universe,
      EngineConfig{200.0, 1000.0, {0, 1}, {{"NTO", 1.0}}},
      ResourceConfig{{{"NTO", 1000}}, {{"NTO", 1000}}, {}}
  );

  flop::craft::CraftControl control;
  control.set_throttle(1.0);
  CraftNewtonian newtonian;
  CraftResources resources(parts, universe);
  StubCraftContext ctx;
  ctx.with_parts(parts).with_mutable_parts(parts)
      .with_control(control)
      .with_resources(resources)
      .with_newtonian(newtonian)
      .with_mass_properties(MassProperties{100.0, {0, 0}, 0.0});

  engine_of(parts).tick(ctx, 1.0);

  // prop_flow = dt * thrust / (isp * g) = 1 * 1000 / (200 * 9.8).
  const double prop_flow = 1000.0 / (200.0 * standard_gravity);
  CHECK(propellant_units(parts, "NTO") == Approx(1000.0 - prop_flow));

  // full propellant: thrust is unscaled and directed along +y in craft space.
  // with no attitude rotation and mass 100 kg, dv = 1000/100 * 1 = 10 m/s +y.
  flop::orbit::StateVectors state{};
  CHECK(newtonian.step(ctx, state, 1.0));
  CHECK(state.velocity.approx({0, 10}));
  CHECK(engine_of(parts).enabled());
}

TEST_CASE("EnginePlugin scales thrust to the limiting propellant") {
  const auto universe = context();
  const double prop_flow = 1000.0 / (200.0 * standard_gravity);
  // start with a fraction of the propellant one full step would consume.
  const double starting_units = 0.25 * prop_flow;
  auto parts = engine_part(
      universe,
      EngineConfig{200.0, 1000.0, {0, 1}, {{"NTO", 1.0}}},
      ResourceConfig{{{"NTO", 1000}}, {{"NTO", starting_units}}, {}}
  );

  flop::craft::CraftControl control;
  control.set_throttle(1.0);
  CraftNewtonian newtonian;
  CraftResources resources(parts, universe);
  StubCraftContext ctx;
  ctx.with_parts(parts).with_mutable_parts(parts).with_control(control).with_resources(resources).with_newtonian(newtonian);

  engine_of(parts).tick(ctx, 1.0);

  // the engine draws only what is available (prop_factor < 1), emptying the tank.
  CHECK(propellant_units(parts, "NTO") == Approx(0.0).margin(1e-9));
  // it still had propellant this step, so it stays enabled for the next.
  CHECK(engine_of(parts).enabled());
}

TEST_CASE("EnginePlugin flames out and disables when the tank is empty") {
  const auto universe = context();
  auto parts = engine_part(
      universe,
      EngineConfig{200.0, 1000.0, {0, 1}, {{"NTO", 1.0}}},
      ResourceConfig{{{"NTO", 1000}}, {{"NTO", 0}}, {}}
  );

  flop::craft::CraftControl control;
  control.set_throttle(1.0);
  CraftNewtonian newtonian;
  CraftResources resources(parts, universe);
  StubCraftContext ctx;
  ctx.with_parts(parts).with_mutable_parts(parts)
      .with_control(control)
      .with_resources(resources)
      .with_newtonian(newtonian)
      .with_mass_properties(MassProperties{100.0, {0, 0}, 0.0});

  engine_of(parts).tick(ctx, 1.0);

  // no propellant to draw: the engine shuts itself down and applies no thrust.
  CHECK_FALSE(engine_of(parts).enabled());
  flop::orbit::StateVectors state{};
  CHECK_FALSE(newtonian.step(ctx, state, 1.0));
}

TEST_CASE("EnginePlugin draws a propellant mixture in its configured ratio") {
  const auto universe = context();
  // equal ratios of NTO (1 kg/unit) and Fuel (2 kg/unit).
  auto parts = engine_part(
      universe,
      EngineConfig{200.0, 1000.0, {0, 1}, {{"NTO", 1.0}, {"Fuel", 1.0}}},
      ResourceConfig{{{"NTO", 1000}, {"Fuel", 1000}}, {{"NTO", 1000}, {"Fuel", 1000}}, {}}
  );

  flop::craft::CraftControl control;
  control.set_throttle(1.0);
  CraftNewtonian newtonian;
  CraftResources resources(parts, universe);
  StubCraftContext ctx;
  ctx.with_parts(parts).with_mutable_parts(parts).with_control(control).with_resources(resources).with_newtonian(newtonian);

  engine_of(parts).tick(ctx, 1.0);

  // prop_ratio_mass = 1*1 + 1*2 = 3 kg per unit of ratio burned.
  const double prop_flow = 1000.0 / (200.0 * standard_gravity);
  const double prop_coeff = prop_flow / 3.0;
  // units drained per propellant = prop_coeff * ratio / density.
  CHECK(propellant_units(parts, "NTO") == Approx(1000.0 - prop_coeff * 1.0 / 1.0));
  CHECK(propellant_units(parts, "Fuel") == Approx(1000.0 - prop_coeff * 1.0 / 2.0));
}
