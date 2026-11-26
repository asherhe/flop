#include <memory>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "flop/view/particles.hpp"

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
  CHECK(emitter.remainder == Catch::Approx(0.5));
  particles.advance(0.5, {1, 0});
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
