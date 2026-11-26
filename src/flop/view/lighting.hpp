#pragma once

#include <array>

#include "flop/math/vec2.hpp"

namespace flop::view {

// direction points toward the sun in sprite-local xy with zero z
// color is linear rgb with intensity included and the fourth component reserved as zero
// zero color means unavailable sunlight and direction defaults to +z
struct SpriteSunlight {
  std::array<float, 4> direction{0, 0, 1, 0};
  std::array<float, 4> color{};
};

// direction points toward the sun in world xy and color is linear rgb
struct Sunlight {
  std::array<float, 4> direction{0, 0, 1, 0};
  std::array<float, 4> color{};
};

struct PlanetShine {
  std::array<float, 2> direction{};
  float intensity{};
  float spread{};
};

// positions are relative vectors in metres and lighting calculations use floats
// returns no shine when inputs cannot describe an illuminated body
PlanetShine compute_planet_shine(
    std::array<float, 2> body_to_sun,
    std::array<float, 2> body_to_craft,
    float body_radius,
    float authored_intensity
);

Sunlight compute_sunlight(Vec2d to_sun);
SpriteSunlight to_sprite_local(const Sunlight &sunlight, double sprite_angle);

} // namespace flop::view
