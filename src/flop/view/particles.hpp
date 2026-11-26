#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <random>
#include <vector>

#include "flop/craft/plugins/config.hpp"
#include "flop/math/vec2.hpp"

namespace flop::view {

struct ParticleEmitter {
  Vec2d position_meters;
  Vec2d velocity_m_per_s;
  double angle_rad{};
  double strength{};
  std::shared_ptr<const craft::plugin::ParticleConfig> config;
  double remainder{};
};

struct Particle {
  Vec2d position_meters;
  Vec2d velocity_m_per_s;
  double age_sec{};
  double lifetime_sec{};
  double start_size_meters{};
  double angle_rad{};
  std::shared_ptr<const craft::plugin::ParticleConfig> config;
};

// transient visual state in unrotating axes relative to the current focus.
class ParticleSystem {
public:
  static constexpr std::size_t capacity = 8192;
  explicit ParticleSystem(unsigned seed = 0);
  void clear();
  void advance(double dt_sec, Vec2d focus_displacement_meters);
  void emit(ParticleEmitter &emitter, double dt_sec);
  [[nodiscard]] const std::vector<Particle> &particles() const noexcept { return particles_; }

private:
  std::vector<Particle> particles_;
  std::mt19937 random_;
  [[nodiscard]] double sample_random(const std::array<double, 2> &range);
};

} // namespace flop::view
