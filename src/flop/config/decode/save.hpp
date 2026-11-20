#pragma once

#include <span>
#include <vector>

#include "flop/config/decode/craft.hpp"
#include "flop/config/load.hpp"
#include "flop/craft/config.hpp"
#include "flop/craft/parts/config.hpp"

namespace flop::config {

// A save is a snapshot of mutable game state. For now that is the set of placed
// craft; the fixed universe (bodies, resources, part types) is supplied by the
// game build, not the save. Each craft entry states its full configuration,
// including a cartesian `orbit` seed and live resource balances, so loading a
// save reconstructs the exact instances without consulting any template.
//
// Returns the fully plugin-resolved craft configs in document order. Each craft
// must carry an `orbit` block: a save holds placed instances, never templates.
[[nodiscard]] inline std::vector<craft::CraftConfig> load_save(
    const AssetResolver &resolver,
    const AssetPath &path,
    std::span<const craft::PartTypeConfig> part_types
) {
  const auto document = detail::read_config(resolver, path);
  const ConfigView root(document, path.address());
  root.validate_unique_keys();

  const auto craft_field = root.required("craft");
  std::vector<craft::CraftConfig> result;
  for (const auto &craft_view : craft_field.sequence()) {
    craft_view.validate_unique_keys();
    auto config = ConfigDecoder<craft::CraftConfig>::decode(craft_view);
    if (!config.orbit.has_value()) {
      craft_view.fail(
          craft_view.mark(), "A saved craft must state its orbit; templates cannot be placed"
      );
    }
    resolve_craft_plugins(config, craft_view, part_types);
    result.push_back(std::move(config));
  }
  return result;
}

} // namespace flop::config
