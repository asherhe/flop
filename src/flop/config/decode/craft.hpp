#pragma once

#include <span>

#include "flop/config/load.hpp"
#include "flop/craft/craft_config.hpp"

namespace flop::config {

template <> struct ConfigDecoder<craft::CraftConfig> {
  [[nodiscard]] static craft::CraftConfig decode(const ConfigView &document);
};

// resolve part templates while the craft YAML is still available for overrides.
[[nodiscard]] craft::CraftConfig load_craft_config(
    const AssetResolver &resolver,
    const AssetPath &path,
    std::span<const craft::PartTypeConfig> part_types
);

} // namespace flop::config
