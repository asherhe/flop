#include "flop/view/lighting.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace flop::view {
namespace {

// placeholder until body occlusion is implemented
float sun_visibility() {
  return 1.0f;
}

} // namespace

Sunlight compute_sunlight(Vec2d to_sun) {
  Sunlight result;

  const auto x = static_cast<float>(to_sun.x);
  const auto y = static_cast<float>(to_sun.y);
  const float distance = std::hypot(x, y);
  if (!(distance > 0.0f) || !std::isfinite(distance))
    return result;

  result.direction = {x / distance, y / distance, 0, 0};

  // dividing before squaring avoids overflowing float at ordinary orbital distances
  constexpr float reference_distance = 9.219544e10f; // sqrt(8.5e21) metres
  const float ratio = reference_distance / distance;
  const float intensity = ratio * ratio * sun_visibility();
  if (!std::isfinite(intensity))
    return Sunlight{};
  result.color = {intensity, intensity, intensity, 0};
  return result;
}

SpriteSunlight to_sprite_local(const Sunlight &sunlight, double sprite_angle) {
  const auto angle = static_cast<float>(sprite_angle);
  if (!std::isfinite(angle))
    return {};
  const float c = std::cos(angle), s = std::sin(angle);
  const float x = sunlight.direction[0], y = sunlight.direction[1];
  return {
      {c * x + s * y, -s * x + c * y, sunlight.direction[2], sunlight.direction[3]}, sunlight.color
  };
}

PlanetShine compute_planet_shine(
    std::array<float, 2> body_to_sun,
    std::array<float, 2> body_to_craft,
    float body_radius,
    float authored_intensity
) {
  // based on the ksp planetshine mod
  //
  PlanetShine result;

  const float sun_distance = std::hypot(body_to_sun[0], body_to_sun[1]);
  const float distance = std::hypot(body_to_craft[0], body_to_craft[1]);
  if (!(sun_distance > 0) || !(distance > 0) || !(body_radius > 0) || !(authored_intensity > 0) ||
      !std::isfinite(sun_distance) || !std::isfinite(distance) || !std::isfinite(body_radius) ||
      !std::isfinite(authored_intensity))
    return result;

  // shrink the body slightly to avoid numerical issues
  const float radius = body_radius * 0.999f;

  // directions from the body center to the sun and craft
  const float sx = body_to_sun[0] / sun_distance, sy = body_to_sun[1] / sun_distance;
  const float ox = body_to_craft[0] / distance, oy = body_to_craft[1] / distance;

  // estimate the visible surface using similar triangles and clamp low altitudes
  const float visible = std::max(distance - radius, 1.0f) / distance;

  // fraction of the visible surface illuminated by the sun
  const float signed_angle = std::atan2(sx * oy - sy * ox, sx * ox + sy * oy);
  const float angle = std::abs(signed_angle);
  constexpr float pi = std::numbers::pi_v<float>;
  const float fraction = std::clamp((pi / 2 + 1 + visible - angle) / (2 * visible), 0.0f, 1.0f);

  // angle from the craft to the average center of the illuminated surface
  const float average = (pi / 2 * visible) * (1 - fraction * (1 - angle / pi));

  // diffuse reflection reduction includes unity's artistic 0.3 brightness boost
  const float angle_effect = std::clamp(1.3f - (angle - average) / (pi / 2), 0.0f, 1.0f);

  // direction from the craft to the average source of reflected light
  const float rotation = signed_angle > 0 ? -average : (signed_angle < 0 ? average : 0.0f);
  const float c = std::cos(rotation), s = std::sin(rotation);
  const float dx = radius * (c * ox - s * oy) - body_to_craft[0];
  const float dy = radius * (s * ox + c * oy) - body_to_craft[1];
  const float light_distance = std::hypot(dx, dy);
  if (!(light_distance > 0) || !std::isfinite(light_distance))
    return result;

  // body cross section is normalized to hamswell's radius and multiplied by
  // incident sunlight and inverse square falloff from the body to the craft
  const float scaled_radius = radius / 3e6f;
  const float sun_ratio = 9.219544e10f / sun_distance;
  const float distance_ratio = radius / distance;

  // unity's 0.5 factor is an artistic compatibility scale
  result.intensity = 0.5f * fraction * sun_ratio * sun_ratio * scaled_radius * scaled_radius *
                     angle_effect * distance_ratio * distance_ratio * authored_intensity;

  // estimate the body's apparent angular size and the illuminated portion
  const float ratio = std::min(radius / distance, 1.0f);
  const float angular_size = std::asin(ratio);
  const float light_size = angular_size * std::min(pi / 4, fraction * (1 - angle / pi));

  // modified diffuse reflectance uses spread + (1 - spread) * cos(theta)
  // choose spread so the response reaches zero at the edge of the light source
  const float sine = std::sin(light_size);
  result.spread = sine / (sine + 1);
  result.direction = {dx / light_distance, dy / light_distance};

  if (!std::isfinite(result.intensity) || !std::isfinite(result.spread))
    return {};
  return result;
}

} // namespace flop::view
