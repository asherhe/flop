#include "flop/config/decode/craft.hpp"

#include <algorithm>

#include "flop/config/decode/part.hpp"
#include "flop/config/decode/part_plugin.hpp"

namespace flop::config {
namespace {

craft::CraftOrbitConfig decode_orbit(const ConfigView &orbit) {
  const auto h = orbit.required("h");
  const auto specific_angular_momentum = h.as<double>();
  if (specific_angular_momentum == 0) {
    h.fail(h.mark(), "Specific angular momentum cannot be zero");
  }
  const auto e = orbit.required("e");
  const auto eccentricity = e.as<double>();
  if (eccentricity < 0) {
    e.fail(e.mark(), "Eccentricity cannot be negative");
  }
  return {
      orbit.required("parent").nonempty_string(),
      specific_angular_momentum,
      eccentricity,
      orbit.required("omega").as<double>(),
      orbit.required("M0").as<double>(),
      orbit.required("t0").as<double>(),
  };
}

} // namespace

craft::CraftConfig ConfigDecoder<craft::CraftConfig>::decode(const ConfigView &document) {
  const auto rotation = document.required("rotation");
  craft::CraftConfig result{
      document.required("name").nonempty_string(),
      decode_orbit(document.required("orbit")),
      {rotation.required("angle").as<double>(), rotation.required("momentum").as<double>()},
      {},
  };
  for (const auto &part : document.required("parts").sequence())
    result.parts.push_back(ConfigDecoder<craft::PartConfig>::decode(part));
  return result;
}

craft::CraftConfig load_craft_config(
    const AssetResolver &resolver,
    const AssetPath &path,
    std::span<const craft::PartTypeConfig> part_types
) {
  const auto document = detail::read_config(resolver, path);
  const ConfigView root(document, path.address());
  root.validate_unique_keys();
  auto result = ConfigDecoder<craft::CraftConfig>::decode(root);
  const auto parts = root.required("parts");
  for (std::size_t index = 0; index < result.parts.size(); ++index) {
    auto &part = result.parts[index];
    const auto type = std::ranges::find(part_types, part.type_id, &craft::PartTypeConfig::type_id);
    if (type == part_types.end())
      parts.at(index).required("type").fail(
          parts.at(index).required("type").mark(), "Unknown part type"
      );
    part.plugins = type->plugins;
    for (const auto &entry : parts.at(index).required("plugins").entries()) {
      const auto plugin =
          std::ranges::find(part.plugins, entry.key, &craft::PartPluginConfig::name);
      if (plugin == part.plugins.end())
        entry.value.fail(entry.value.mark(), "Unknown plugin override");
      apply_part_plugin_fields(*plugin, entry.value);
    }
  }
  result.plugins_resolved = true;
  return result;
}

} // namespace flop::config
