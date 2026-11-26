#include <filesystem>
#include <memory>
#include <numbers>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "flop/app/game_scene.hpp"
#include "flop/view/particle_scene.hpp"
#include "flop/view/particles.hpp"
#include "flop/world/control_manager.hpp"

TEST_CASE("particles reuse capacity and compensate for focus motion") {
  auto config = std::make_shared<flop::craft::plugin::ParticleConfig>();
  config->lifetime_sec = {1, 1};
  config->rate_per_sec = {10, 10};
  config->start_size_meters = {1, 1};
  config->start_speed_m_per_s = {0, 0};
  flop::view::ParticleSystem particles(1);
  flop::view::ParticleEmitter emitter{{0, 0}, {4, 0}, 0, 1, config};
  particles.emit(emitter, 0.25);
  REQUIRE(particles.particles().size() == 2);
  flop::view::ParticleSystem repeat(1);
  flop::view::ParticleEmitter same_emitter{{0, 0}, {4, 0}, 0, 1, config};
  repeat.emit(same_emitter, 0.25);
  const double angle = particles.particles()[0].angle_rad;
  CHECK(angle >= 0);
  CHECK(angle < 2.0 * std::numbers::pi);
  CHECK(repeat.particles()[0].angle_rad == angle);
  CHECK(emitter.remainder == Catch::Approx(0.5));
  particles.advance(0.5, {1, 0});
  CHECK(particles.particles()[0].angle_rad == angle);
  CHECK(particles.particles()[0].position_meters.x == Catch::Approx(1));
  particles.emit(emitter, 0.05);
  CHECK(particles.particles().size() == 3);
  particles.advance(1, {});
  CHECK(particles.particles().empty());
  config->rate_per_sec = {100000, 100000};
  particles.emit(emitter, 1);
  CHECK(particles.particles().size() == flop::view::ParticleSystem::capacity);
  particles.clear();
  CHECK(particles.particles().empty());
}

TEST_CASE("line emitter emits along its rotated local positive y axis") {
  auto config = std::make_shared<flop::craft::plugin::ParticleConfig>();
  config->lifetime_sec = {1, 1};
  config->rate_per_sec = {1, 1};
  config->start_speed_m_per_s = {4, 4};
  flop::view::ParticleSystem particles(1);
  flop::view::ParticleEmitter emitter{{}, {}, std::numbers::pi / 2, 1, config};
  particles.emit(emitter, 1);
  REQUIRE(particles.particles().size() == 1);
  CHECK(particles.particles()[0].velocity_m_per_s.x == Catch::Approx(-4));
  CHECK(particles.particles()[0].velocity_m_per_s.y == Catch::Approx(0).margin(1e-12));
}

TEST_CASE("shipped engine emits particles on world steps and focus changes clear them") {
  const flop::config::AssetResolver resolver(std::filesystem::path(FLOP_TEST_ASSET_ROOT));
  auto scene = flop::app::load_game_scene(resolver);
  auto &world = *scene.universe;
  const flop::view::Focus focus = scene.craft.front();
  world.controls().submit(flop::world::ControlManager::SelectCraft{scene.craft.front()});
  world.controls().submit(flop::world::ControlManager::SetThrottle{1.0});
  (void)world.controls().apply_commands();
  flop::view::ParticleScene particles;
  particles.begin_step(world, focus);
  world.tick(0.02);
  particles.end_step(world, focus, 0.02);
  REQUIRE_FALSE(particles.system().particles().empty());
  const auto count = particles.system().particles().size();
  flop::view::FrameDrawData frame;
  particles.append_draw_data(frame);
  CHECK(frame.particles.size() == count);
  CHECK(frame.particles[0].angle_rad == Catch::Approx(particles.system().particles()[0].angle_rad));
  particles.set_focus(std::string("Sun"));
  CHECK(particles.system().particles().empty());
}
