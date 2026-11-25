#pragma once

#include <algorithm>
#include <cstddef>
#include <span>

#include "flop/config/decode/part.hpp"
#include "flop/config/decode/part_plugin.hpp"
#include "flop/config/load.hpp"
#include "flop/craft/config.hpp"

namespace flop::config {

namespace detail {

// a save instance seeds the craft from cartesian state. a template omits the
// orbit block entirely and leaves CraftConfig::orbit absent.
inline craft::CraftOrbitState decode_craft_orbit(const ConfigView &orbit) {
  return {
      orbit.required("parent").nonempty_string(),
      {decode_vec(orbit.required("pos")), decode_vec(orbit.required("vel"))},
      orbit.required("t0").as<double>(),
  };
}

} // namespace detail

template <> struct ConfigDecoder<craft::CraftConfig> {
  [[nodiscard]] static craft::CraftConfig decode(const ConfigView &document) {
    const auto rotation = document.required("rotation");
    craft::CraftConfig result{
        document.required("name").nonempty_string(),
        std::nullopt,
        {rotation.required("angle").as<double>(), rotation.required("momentum").as<double>()},
        {},
    };
    if (const auto orbit = document.optional("orbit")) {
      result.orbit = detail::decode_craft_orbit(*orbit);
    }
    for (const auto &part : document.required("parts").sequence())
      result.parts.push_back(ConfigDecoder<craft::PartConfig>::decode(part));
    return result;
  }
};

// resolve each part's plugin configuration against its part type: copy the
// type's plugin list onto the part, then apply the craft document's per-plugin
// overrides. `craft_view` is the ConfigView of one craft mapping (its `parts`
// sequence is read for override entries and error locations). mutates `config`
// in place and marks its plugins resolved.
inline void resolve_craft_plugins(
    craft::CraftConfig &config,
    const ConfigView &craft_view,
    std::span<const craft::PartTypeConfig> part_types
) {
  const auto parts = craft_view.required("parts");
  for (std::size_t index = 0; index < config.parts.size(); ++index) {
    auto &part = config.parts[index];
    const auto type = std::ranges::find(part_types, part.type_id, &craft::PartTypeConfig::type_id);
    if (type == part_types.end())
      parts.at(index).required("type").fail(
          parts.at(index).required("type").mark(), "Unknown part type"
      );
    part.plugins = type->plugins;
    for (const auto &entry : parts.at(index).required("plugins").entries()) {
      const auto plugin =
          std::ranges::find(part.plugins, entry.key, &craft::plugin::PartPluginConfig::name);
      if (plugin == part.plugins.end())
        entry.value.fail(entry.value.mark(), "Unknown plugin override");
      apply_part_plugin_fields(*plugin, entry.value);
    }
  }
  config.plugins_resolved = true;
}

// resolve part templates while the craft YAML is still available for overrides.
[[nodiscard]] inline craft::CraftConfig load_craft_config(
    const AssetResolver &resolver,
    const AssetPath &path,
    std::span<const craft::PartTypeConfig> part_types
) {
  const auto document = detail::read_config(resolver, path);
  const ConfigView root(document, path.address());
  root.validate_unique_keys();
  auto result = ConfigDecoder<craft::CraftConfig>::decode(root);
  resolve_craft_plugins(result, root, part_types);
  for (const auto &part : result.parts) {
    for (const auto &plugin : part.plugins) {
      const auto *sprite = std::get_if<craft::plugin::SpriteConfig>(&plugin.values);
      if (!sprite) continue;
      const auto validate = [&](const std::string &address, const craft::plugin::AssetSource &source) {
        if (address.empty()) return;
        try {
          static_cast<void>(resolver.resolve(AssetPath(address)));
        } catch (const std::exception &error) {
          YAML::Mark mark;
          mark.pos = source.line < 0 ? -1 : 0;
          mark.line = source.line;
          mark.column = source.column;
          throw ConfigError(source.address, source.key, mark, error.what());
        }
      };
      validate(sprite->sprite, sprite->sprite_source);
      validate(sprite->material.normal_map, sprite->material.normal_map_source);
      validate(sprite->material.emission_map, sprite->material.emission_map_source);
    }
  }
  return result;
}

} // namespace flop::config
