#include "flop/view/particles.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace flop::view {

ParticleSystem::ParticleSystem(unsigned seed) : random_(seed) {
  particles_.reserve(capacity);
}

void ParticleSystem::clear() {
  particles_.clear();
}

double ParticleSystem::sample_random(const std::array<double, 2> &range) {
  return std::uniform_real_distribution<double>(range[0], range[1])(random_);
}

void ParticleSystem::advance(double dt_sec, Vec2d focus_displacement_meters) {
  if (!(dt_sec > 0.0) || !std::isfinite(dt_sec))
    return;
  for (auto &particle : particles_) {
    // particles do not yet follow gravity or physical reference frame changes.
    particle.position_meters += particle.velocity_m_per_s * dt_sec - focus_displacement_meters;
    particle.age_sec += dt_sec;
  }
  std::erase_if(particles_, [](const auto &particle) {
    return particle.age_sec >= particle.lifetime_sec;
  });
}

void ParticleSystem::emit(ParticleEmitter &emitter, double dt_sec) {
  if (!emitter.config || !(dt_sec > 0.0) || !std::isfinite(dt_sec) || !(emitter.strength > 0.0))
    return;
  const auto &config = *emitter.config;
  const double expected =
      sample_random(config.rate_per_sec) * std::min(emitter.strength, 1.0) * dt_sec +
      emitter.remainder;
  if (!std::isfinite(expected)) {
    emitter.remainder = 0.0;
    return;
  }
  const double whole = std::floor(expected);
  emitter.remainder = expected - whole;
  const auto count = static_cast<std::size_t>(std::min(whole, static_cast<double>(capacity)));
  for (std::size_t i = 0; i < count && particles_.size() < capacity; ++i) {
    const double life = sample_random(config.lifetime_sec);
    if (!(life > 0.0))
      continue;
    const double offset =
        sample_random({-0.5 * config.line_width_meters, 0.5 * config.line_width_meters});
    const double angle =
        emitter.angle_rad + sample_random({-config.spread_angle_rad, config.spread_angle_rad});
    particles_.push_back(
        {emitter.position_meters + Vec2d{offset, 0}.rotate(emitter.angle_rad),
         emitter.velocity_m_per_s +
             Vec2d{0, sample_random(config.start_speed_m_per_s)}.rotate(angle),
         0.0,
         life,
         sample_random(config.start_size_meters),
         sample_random({0.0, 2.0 * std::numbers::pi}),
         emitter.config}
    );
  }
}

} // namespace flop::view
