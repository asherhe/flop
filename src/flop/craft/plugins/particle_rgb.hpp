#pragma once

namespace flop::craft::plugin {

// normalized srgb components as authored in a particle gradient
struct ParticleRgb {
  double r{}, g{}, b{};

  friend constexpr ParticleRgb operator+(ParticleRgb a, ParticleRgb b) {
    return {a.r + b.r, a.g + b.g, a.b + b.b};
  }

  friend constexpr ParticleRgb operator*(ParticleRgb color, double factor) {
    return {color.r * factor, color.g * factor, color.b * factor};
  }
};

} // namespace flop::craft::plugin
