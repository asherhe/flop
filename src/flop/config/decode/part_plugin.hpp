#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "flop/config/load.hpp"
#include "flop/config/decode/color.hpp"
#include "flop/config/decode/key_curve.hpp"
#include "flop/config/decode/range.hpp"
#include "flop/craft/plugins/config.hpp"

namespace flop::config {

[[nodiscard]] craft::plugin::PartPluginConfig decode_part_plugin(std::string name, const ConfigView &value);
void apply_part_plugin_fields(craft::plugin::PartPluginConfig &config, const ConfigView &value);

namespace detail {

enum class PluginReadMode : std::uint8_t { create, override };
enum class PluginRequired : std::uint8_t { no, on_create };

// Track the fields read by one mapping so unknown keys need no separate allowlist.
class MapReader {
public:
  MapReader(const ConfigView &value, PluginReadMode mode) : value_(value), mode_(mode) {}

  template <class T, class Decode>
  void assign(
      std::string_view key,
      T &destination,
      Decode decode,
      PluginRequired required = PluginRequired::no
  ) {
    keys_.push_back(key);
    const auto field = mode_ == PluginReadMode::create && required == PluginRequired::on_create
                           ? std::optional<ConfigView>(value_.required(key))
                           : value_.optional(key);
    if (field)
      destination = std::invoke(decode, *field);
  }

  // Effect data is authored in part files but has no typed runtime config yet.
  void ignore_mapping(std::string_view key) {
    keys_.push_back(key);
    if (const auto field = value_.optional(key))
      static_cast<void>(field->entries());
  }

  void finish() const {
    for (const auto &entry : value_.entries()) {
      if (std::ranges::find(keys_, entry.key) == keys_.end())
        entry.value.fail(entry.value.mark(), "Unknown plugin field");
    }
  }

private:
  const ConfigView &value_;
  PluginReadMode mode_;
  std::vector<std::string_view> keys_;
};

template <class T> std::map<std::string, T> decode_map(const ConfigView &field) {
  std::map<std::string, T> result;
  for (const auto &entry : field.entries())
    result.emplace(entry.key, entry.value.as<T>());
  return result;
}

inline Vec2d decode_vec(const ConfigView &field) {
  return {field.required("x").as<double>(), field.required("y").as<double>()};
}

inline craft::plugin::AssetSource asset_source(const ConfigView &field) {
  return {field.source_address(), field.key_path(), field.mark().line, field.mark().column};
}

inline craft::plugin::SpriteMaterialConfig decode_material(const ConfigView &value) {
  craft::plugin::SpriteMaterialConfig result;
  MapReader reader(value, PluginReadMode::create);
  reader.assign("shader", result.shader, [](const ConfigView &field) {
    auto id = field.nonempty_string();
    if (id != "craft/sprite" && id != "craft/lit_sprite") field.fail(field.mark(), "Unknown sprite shader");
    return id;
  }, PluginRequired::on_create);
  auto properties = result;
  reader.assign("properties", properties, [](const ConfigView &field) {
    craft::plugin::SpriteMaterialConfig properties;
    MapReader property_reader(field, PluginReadMode::create);
    property_reader.assign("normal_map", properties.normal_map, [&](const ConfigView &value) {
      properties.normal_map_source = asset_source(value);
      return value.asset_path().address();
    });
    property_reader.assign("emission_map", properties.emission_map, [&](const ConfigView &value) {
      properties.emission_map_source = asset_source(value);
      return value.asset_path().address();
    });
    property_reader.finish();
    return properties;
  });
  result.normal_map = std::move(properties.normal_map);
  result.emission_map = std::move(properties.emission_map);
  result.normal_map_source = std::move(properties.normal_map_source);
  result.emission_map_source = std::move(properties.emission_map_source);
  reader.finish();
  return result;
}

inline void apply_fields(craft::plugin::CommandConfig &command, MapReader &reader) {
  reader.assign("auto_steerable", command.auto_steerable, &ConfigView::as<bool>);
  reader.assign("Kp", command.kp, &ConfigView::as<double>);
  reader.assign("Ki", command.ki, &ConfigView::as<double>);
  reader.assign("Kd", command.kd, &ConfigView::as<double>);
}

inline void apply_fields(craft::plugin::ReactionWheelConfig &wheel, MapReader &reader) {
  reader.assign(
      "torque",
      wheel.torque_newton_meters,
      [](const ConfigView &field) { return field.checked_finite(field.positive_double() * 1000); },
      PluginRequired::on_create
  );
}

inline void apply_fields(craft::plugin::EngineConfig &engine, MapReader &reader) {
  reader.assign("isp", engine.isp_sec, &ConfigView::positive_double, PluginRequired::on_create);
  reader.assign(
      "thrust",
      engine.thrust_newtons,
      [](const ConfigView &field) { return field.checked_finite(field.positive_double() * 1000); },
      PluginRequired::on_create
  );
  reader.assign("thrust_dir", engine.thrust_direction, decode_vec, PluginRequired::on_create);
  reader.assign(
      "propellant_ratio", engine.propellant_ratio, decode_map<double>, PluginRequired::on_create
  );
  reader.ignore_mapping("plume");
  const auto nonnegative = [](const ConfigView &field) {
    const auto value = field.as<double>();
    if (value < 0.0) field.fail(field.mark(), "Expected a nonnegative value");
    return value;
  };
  reader.assign("flicker_speed", engine.flicker_speed, nonnegative);
  reader.assign("flicker_min", engine.flicker_min, nonnegative);
  reader.assign("flicker_max", engine.flicker_max, nonnegative);
}

inline void apply_fields(craft::plugin::ResourceConfig &container, MapReader &reader) {
  reader.assign(
      "max_amount", container.max_amount_units, decode_map<double>, PluginRequired::on_create
  );
  reader.assign("amount", container.amount_units, decode_map<double>);
  reader.assign("priority", container.priority, decode_map<int>);
}

inline void apply_fields(craft::plugin::SpriteConfig &sprite, MapReader &reader) {
  reader.assign("sprite", sprite.sprite, [&](const ConfigView &field) {
    sprite.sprite_source = asset_source(field);
    return field.asset_path().address();
  }, PluginRequired::on_create);
  reader.assign("material", sprite.material, decode_material, PluginRequired::on_create);
  reader.assign("size_meters", sprite.size_meters, [](const ConfigView &field) {
    return Vec2d{field.required("x").positive_double(), field.required("y").positive_double()};
  }, PluginRequired::on_create);
  reader.assign("pivot", sprite.pivot, [](const ConfigView &field) {
    const auto component = [](const ConfigView &value) {
      const double result = value.as<double>();
      if (result < 0.0 || result > 1.0)
        value.fail(value.mark(), "Pivot must be between 0 and 1");
      return result;
    };
    return Vec2d{component(field.required("x")), component(field.required("y"))};
  }, PluginRequired::on_create);
}

inline void apply_fields(craft::plugin::LightConfig &light, MapReader &reader) {
  const auto nonnegative = [](const ConfigView &field) {
    const auto value = field.as<double>();
    if (value < 0.0) field.fail(field.mark(), "Expected a nonnegative value");
    return value;
  };
  reader.assign(
      "light_position", light.light_position_meters, decode_vec, PluginRequired::on_create
  );
  reader.assign("color", light.color, [](const ConfigView &field) {
    (void)ConfigDecoder<Color>::decode(field);
    return field.nonempty_string();
  }, PluginRequired::on_create);
  reader.assign("intensity", light.intensity, nonnegative, PluginRequired::on_create);
  reader.assign(
      "inner_radius", light.inner_radius_meters, nonnegative, PluginRequired::on_create
  );
  reader.assign(
      "outer_radius", light.outer_radius_meters, nonnegative, PluginRequired::on_create
  );
  reader.assign("falloff", light.falloff, [](const ConfigView &field) {
    const auto value = field.as<double>();
    if (value < 0.0 || value > 1.0) field.fail(field.mark(), "Expected a value between 0 and 1");
    return value;
  }, PluginRequired::on_create);
  reader.assign("strength", light.strength, [](const ConfigView &field) {
    const auto value = field.checked_finite(field.as<double>());
    if (value < 0.0) field.fail(field.mark(), "Expected a nonnegative strength");
    return value;
  });
}

inline std::array<double, 2> decode_particle_range(const ConfigView &field) {
  const auto range = decode_ordered_range(field);
  if (range[0] < 0.0) field.fail(field.mark(), "Expected a nonnegative range");
  return range;
}

inline void apply_fields(craft::plugin::ParticleConfig &particle, MapReader &reader) {
  reader.assign("line_pos", particle.line_position_meters, decode_vec, PluginRequired::on_create);
  reader.assign("line_width", particle.line_width_meters, &ConfigView::positive_double, PluginRequired::on_create);
  reader.assign("line_angle", particle.line_angle_rad, [](const ConfigView &field) {
    return field.checked_finite(field.as<double>() * std::numbers::pi / 180.0);
  });
  reader.assign("spread_angle", particle.spread_angle_rad, [](const ConfigView &field) {
    const double angle = field.as<double>();
    if (angle < 0 || angle > 180) field.fail(field.mark(), "Spread angle must be between 0 and 180 degrees");
    return angle * std::numbers::pi / 180.0;
  }, PluginRequired::on_create);
  reader.assign("lifetime", particle.lifetime_sec, [](const ConfigView &field) {
    const auto value = decode_particle_range(field);
    if (!(value[0] > 0)) field.fail(field.mark(), "Particle lifetime must be positive");
    return value;
  }, PluginRequired::on_create);
  reader.assign("rate", particle.rate_per_sec, decode_particle_range, PluginRequired::on_create);
  auto start = std::pair{particle.start_size_meters, particle.start_speed_m_per_s};
  reader.assign("start", start, [](const ConfigView &field) {
    return std::pair{decode_particle_range(field.required("size")),
                     decode_particle_range(field.required("speed"))};
  }, PluginRequired::on_create);
  particle.start_size_meters = start.first;
  particle.start_speed_m_per_s = start.second;
  auto life = particle;
  reader.assign("life", life, [](const ConfigView &field) {
    craft::plugin::ParticleConfig result;
    const auto color = field.required("color");
    const auto normalized = [](double key, const ConfigView &source) {
      if (key < 0.0 || key > 1.0) source.fail(source.mark(), "Curve key must be between 0 and 1");
    };
    result.color = decode_key_curve<Color>(color.required("color_keys"), &ConfigDecoder<Color>::decode, normalized);
    result.alpha = decode_key_curve<double>(color.required("alpha_keys"), &ConfigView::as<double>, normalized);
    result.size = decode_key_curve<double, SmoothInterpolation>(field.required("size"), &ConfigView::as<double>, normalized);
    return result;
  }, PluginRequired::on_create);
  particle.color = std::move(life.color);
  particle.alpha = std::move(life.alpha);
  particle.size = std::move(life.size);
}

inline void apply_fields(
    craft::plugin::PartPluginConfig &config, const ConfigView &value, PluginReadMode mode
) {
  MapReader reader(value, mode);
  reader.assign("enabled", config.enabled, &ConfigView::as<bool>);
  std::visit([&reader](auto &values) { apply_fields(values, reader); }, config.values);
  reader.finish();
  if (const auto *light = std::get_if<craft::plugin::LightConfig>(&config.values);
      light && light->inner_radius_meters > light->outer_radius_meters) {
    const auto field = value.optional("inner_radius").value_or(
        value.optional("outer_radius").value_or(value));
    field.fail(field.mark(), "Inner radius must not exceed outer radius");
  }
  if (const auto *engine = std::get_if<craft::plugin::EngineConfig>(&config.values);
      engine && engine->flicker_min > engine->flicker_max) {
    const auto field = value.optional("flicker_min").value_or(
        value.optional("flicker_max").value_or(value));
    field.fail(field.mark(), "Flicker minimum must not exceed maximum");
  }
}

} // namespace detail

inline craft::plugin::PartPluginConfig decode_part_plugin(std::string name, const ConfigView &value) {
  craft::plugin::PartPluginConfig result{std::move(name)};
  if (result.name == "CommandPlugin") {
    result.values = craft::plugin::CommandConfig{};
  } else if (result.name == "ReactionWheelPlugin") {
    result.values = craft::plugin::ReactionWheelConfig{};
  } else if (result.name == "EnginePlugin") {
    result.values = craft::plugin::EngineConfig{};
  } else if (result.name == "ResourceContainerPlugin") {
    result.values = craft::plugin::ResourceConfig{};
  } else if (result.name == "SpritePlugin") {
    result.values = craft::plugin::SpriteConfig{};
  } else if (result.name == "LightPlugin") {
    result.values = craft::plugin::LightConfig{};
  } else if (result.name == "ParticlePlugin") {
    result.values = craft::plugin::ParticleConfig{};
  } else {
    value.fail(value.mark(), "Unknown part plugin");
  }
  detail::apply_fields(result, value, detail::PluginReadMode::create);
  return result;
}

inline void apply_part_plugin_fields(craft::plugin::PartPluginConfig &config, const ConfigView &value) {
  detail::apply_fields(config, value, detail::PluginReadMode::override);
}

} // namespace flop::config
