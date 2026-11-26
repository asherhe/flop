#pragma once

#include <optional>
#include <string>

#include "flop/craft/plugins/config.hpp"
#include "flop/utils/color.hpp"

namespace flop::config {

// the atmosphere a body may carry, as authored. the physical fields drive the
// simulation-adjacent gameplay (where one exists); the scattering material is a
// compatibility selector plus the property map the renderer reads. absent for a
// body with `atmosphere: false`.
struct AtmosphereVisualConfig {
  double height_meters{};
  double sea_level_pressure_atm{};
  double scale_height_meters{};
  craft::plugin::MaterialConfig material;
};

// a soft fill light a body casts on nearby craft (planet shine). absent when a
// body authors no planet_shine block.
struct PlanetShineConfig {
  Color color;
  double intensity{};
};

// the visual side of a celestial body, kept separate from world::BodyConfig so
// the orbital world never depends on rendering. Decoded from the same body YAML
// at the file boundary: the base color, the surface material (albedo, specular,
// normal, night-lights as authored Unity property keys), the optional
// atmosphere, and the optional planet shine. Unity-only display hints such as
// map_icon and custom_display_prefabs are intentionally not decoded here.
struct BodyVisualConfig {
  std::string name;
  std::string color;
  craft::plugin::MaterialConfig surface_material;
  std::optional<AtmosphereVisualConfig> atmosphere;
  std::optional<PlanetShineConfig> planet_shine;
};

} // namespace flop::config
