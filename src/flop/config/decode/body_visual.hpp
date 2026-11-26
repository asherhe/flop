#pragma once

#include "flop/config/decode/color.hpp"

#include <string>
#include <utility>
#include <yaml-cpp/yaml.h>

#include "flop/config/body_visual_config.hpp"
#include "flop/config/load.hpp"

namespace flop::config {

namespace detail {

// render a material key as a value. currently MaterialConfig takes only strings for values
// (inherited from Unity). TODO: this is kinda ass, change it to read and store typed values more
// intelligently
inline std::string render_value(const ConfigView &value) {
  if (value.type() == YAML::NodeType::Sequence) {
    std::string text = "[";
    bool first = true;
    for (const auto &element : value.sequence()) {
      if (!first) {
        text += ", ";
      }
      first = false;
      text += render_value(element);
    }
    text += ']';
    return text;
  }
  return value.as<std::string>();
}

inline craft::plugin::MaterialConfig decode_visual_material(const ConfigView &value) {
  craft::plugin::MaterialConfig result;
  result.path = value.required("path").nonempty_string();
  if (const auto properties = value.optional("properties")) {
    for (const auto &entry : properties->entries()) {
      craft::plugin::MaterialPropertyConfig property;
      property.type = entry.value.required("type").nonempty_string();
      property.value = render_value(entry.value.required("value"));
      result.properties.emplace(entry.key, std::move(property));
    }
  }
  return result;
}

inline AtmosphereVisualConfig decode_atmosphere_visual(const ConfigView &atmosphere) {
  const auto height = atmosphere.required("height");
  const auto scale_height = atmosphere.required("scale_height");
  return {
      // authored in kilometres; the renderer works in metres.
      height.checked_finite(height.positive_double() * 1000),
      atmosphere.required("sea_level_pressure").positive_double(),
      scale_height.checked_finite(scale_height.positive_double() * 1000),
      decode_visual_material(atmosphere.required("material")),
  };
}

inline PlanetShineConfig decode_planet_shine(const ConfigView &shine) {
  return {
      decode_color(shine.required("color")),
      shine.required("intensity").as<double>(),
  };
}

} // namespace detail

template <> struct ConfigDecoder<BodyVisualConfig> {
  [[nodiscard]] static BodyVisualConfig decode(const ConfigView &document) {
    BodyVisualConfig result{
        document.required("name").nonempty_string(),
        document.required("color").nonempty_string(),
        detail::decode_visual_material(document.required("surface_material")),
        std::nullopt,
        std::nullopt,
    };
    // atmosphere is a map for a body with air, or `false` for one without.
    const auto atmosphere = document.required("atmosphere");
    if (atmosphere.type() == YAML::NodeType::Map) {
      result.atmosphere = detail::decode_atmosphere_visual(atmosphere);
    } else if (atmosphere.as<bool>()) {
      atmosphere.fail(atmosphere.mark(), "Atmosphere must be a mapping or false");
    }
    if (const auto shine = document.optional("planet_shine")) {
      result.planet_shine = detail::decode_planet_shine(*shine);
    }
    return result;
  }
};

} // namespace flop::config
