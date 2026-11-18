#include "flop/config/decode/part_plugin.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace flop::config {
namespace {

enum class ReadMode : std::uint8_t { create, override };
enum class Required : std::uint8_t { no, on_create };

// Track the fields read by one mapping so unknown keys need no separate allowlist.
class MapReader {
public:
  MapReader(const ConfigView &value, ReadMode mode) : value_(value), mode_(mode) {}

  template <class T, class Decode>
  void assign(
      std::string_view key, T &destination, Decode decode, Required required = Required::no
  ) {
    keys_.push_back(key);
    const auto field = mode_ == ReadMode::create && required == Required::on_create
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
  ReadMode mode_;
  std::vector<std::string_view> keys_;
};

template <class T> std::map<std::string, T> decode_map(const ConfigView &field) {
  std::map<std::string, T> result;
  for (const auto &entry : field.entries())
    result.emplace(entry.key, entry.value.as<T>());
  return result;
}

Vec2d decode_vec(const ConfigView &field) {
  return {field.required("x").as<double>(), field.required("y").as<double>()};
}

craft::MaterialPropertyConfig decode_material_property(const ConfigView &value) {
  craft::MaterialPropertyConfig result;
  MapReader reader(value, ReadMode::create);
  reader.assign("type", result.type, &ConfigView::nonempty_string, Required::on_create);
  reader.assign("value", result.value, &ConfigView::nonempty_string, Required::on_create);
  reader.finish();
  return result;
}

craft::MaterialConfig decode_material(const ConfigView &value) {
  craft::MaterialConfig result;
  MapReader reader(value, ReadMode::create);
  reader.assign("path", result.path, &ConfigView::nonempty_string, Required::on_create);
  reader.assign("properties", result.properties, [](const ConfigView &field) {
    std::map<std::string, craft::MaterialPropertyConfig> properties;
    for (const auto &entry : field.entries())
      properties.emplace(entry.key, decode_material_property(entry.value));
    return properties;
  });
  reader.finish();
  return result;
}

void apply_fields(craft::CommandConfig &command, MapReader &reader) {
  reader.assign("auto_steerable", command.auto_steerable, &ConfigView::as<bool>);
  reader.assign("Kp", command.kp, &ConfigView::as<double>);
  reader.assign("Ki", command.ki, &ConfigView::as<double>);
  reader.assign("Kd", command.kd, &ConfigView::as<double>);
}

void apply_fields(craft::ReactionWheelConfig &wheel, MapReader &reader) {
  reader.assign(
      "torque",
      wheel.torque_newton_meters,
      [](const ConfigView &field) { return field.checked_finite(field.positive_double() * 1000); },
      Required::on_create
  );
}

void apply_fields(craft::EngineConfig &engine, MapReader &reader) {
  reader.assign("isp", engine.isp_sec, &ConfigView::positive_double, Required::on_create);
  reader.assign(
      "thrust",
      engine.thrust_newtons,
      [](const ConfigView &field) { return field.checked_finite(field.positive_double() * 1000); },
      Required::on_create
  );
  reader.assign("thrust_dir", engine.thrust_direction, decode_vec, Required::on_create);
  reader.assign(
      "propellant_ratio", engine.propellant_ratio, decode_map<double>, Required::on_create
  );
  reader.ignore_mapping("plume");
  reader.ignore_mapping("light");
}

void apply_fields(craft::ResourceContainerConfig &container, MapReader &reader) {
  reader.assign("max_amount", container.max_amount_units, decode_map<double>, Required::on_create);
  reader.assign("amount", container.amount_units, decode_map<double>);
  reader.assign("priority", container.priority, decode_map<int>);
}

void apply_fields(craft::SpriteRendererConfig &sprite, MapReader &reader) {
  reader.assign("sprite", sprite.sprite, &ConfigView::nonempty_string, Required::on_create);
  reader.assign("material", sprite.material, decode_material, Required::on_create);
}

void apply_fields(craft::LightConfig &light, MapReader &reader) {
  reader.assign("light_position", light.light_position_meters, decode_vec, Required::on_create);
  reader.assign("color", light.color, &ConfigView::nonempty_string, Required::on_create);
  reader.assign("intensity", light.intensity, &ConfigView::as<double>, Required::on_create);
  reader.assign(
      "inner_radius", light.inner_radius_meters, &ConfigView::as<double>, Required::on_create
  );
  reader.assign(
      "outer_radius", light.outer_radius_meters, &ConfigView::as<double>, Required::on_create
  );
  reader.assign("falloff", light.falloff, &ConfigView::as<double>, Required::on_create);
}

void apply_fields(craft::PartPluginConfig &config, const ConfigView &value, ReadMode mode) {
  MapReader reader(value, mode);
  reader.assign("enabled", config.enabled, &ConfigView::as<bool>);
  std::visit([&reader](auto &values) { apply_fields(values, reader); }, config.values);
  reader.finish();
}

} // namespace

craft::PartPluginConfig decode_part_plugin(std::string name, const ConfigView &value) {
  craft::PartPluginConfig result{std::move(name)};
  if (result.name == "CommandPlugin") {
    result.values = craft::CommandConfig{};
  } else if (result.name == "ReactionWheelPlugin") {
    result.values = craft::ReactionWheelConfig{};
  } else if (result.name == "EnginePlugin") {
    result.values = craft::EngineConfig{};
  } else if (result.name == "ResourceContainerPlugin") {
    result.values = craft::ResourceContainerConfig{};
  } else if (result.name == "SpriteRendererPlugin") {
    result.values = craft::SpriteRendererConfig{};
  } else if (result.name == "LightPlugin") {
    result.values = craft::LightConfig{};
  } else {
    value.fail(value.mark(), "Unknown part plugin");
  }
  apply_fields(result, value, ReadMode::create);
  return result;
}

void apply_part_plugin_fields(craft::PartPluginConfig &config, const ConfigView &value) {
  apply_fields(config, value, ReadMode::override);
}

} // namespace flop::config
