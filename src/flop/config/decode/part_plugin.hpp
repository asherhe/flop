#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "flop/config/load.hpp"
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

inline craft::plugin::SpriteMaterialConfig decode_material(const ConfigView &value) {
  craft::plugin::SpriteMaterialConfig result;
  MapReader reader(value, PluginReadMode::create);
  reader.assign("shader", result.shader, [](const ConfigView &field) {
    auto id = field.nonempty_string();
    if (id != "craft/sprite") field.fail(field.mark(), "Unknown sprite shader");
    return id;
  }, PluginRequired::on_create);
  auto properties = result;
  reader.assign("properties", properties, [](const ConfigView &field) {
    craft::plugin::SpriteMaterialConfig properties;
    MapReader property_reader(field, PluginReadMode::create);
    property_reader.assign("normal_map", properties.normal_map, &ConfigView::nonempty_string);
    property_reader.assign("emission_map", properties.emission_map, &ConfigView::nonempty_string);
    property_reader.finish();
    return properties;
  });
  result.normal_map = std::move(properties.normal_map);
  result.emission_map = std::move(properties.emission_map);
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
  reader.ignore_mapping("light");
}

inline void apply_fields(craft::plugin::ResourceConfig &container, MapReader &reader) {
  reader.assign(
      "max_amount", container.max_amount_units, decode_map<double>, PluginRequired::on_create
  );
  reader.assign("amount", container.amount_units, decode_map<double>);
  reader.assign("priority", container.priority, decode_map<int>);
}

inline void apply_fields(craft::plugin::SpriteConfig &sprite, MapReader &reader) {
  reader.assign("sprite", sprite.sprite, &ConfigView::nonempty_string, PluginRequired::on_create);
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
  reader.assign(
      "light_position", light.light_position_meters, decode_vec, PluginRequired::on_create
  );
  reader.assign("color", light.color, &ConfigView::nonempty_string, PluginRequired::on_create);
  reader.assign("intensity", light.intensity, &ConfigView::as<double>, PluginRequired::on_create);
  reader.assign(
      "inner_radius", light.inner_radius_meters, &ConfigView::as<double>, PluginRequired::on_create
  );
  reader.assign(
      "outer_radius", light.outer_radius_meters, &ConfigView::as<double>, PluginRequired::on_create
  );
  reader.assign("falloff", light.falloff, &ConfigView::as<double>, PluginRequired::on_create);
}

inline void apply_fields(
    craft::plugin::PartPluginConfig &config, const ConfigView &value, PluginReadMode mode
) {
  MapReader reader(value, mode);
  reader.assign("enabled", config.enabled, &ConfigView::as<bool>);
  std::visit([&reader](auto &values) { apply_fields(values, reader); }, config.values);
  reader.finish();
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
