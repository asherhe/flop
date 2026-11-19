#pragma once

#include "flop/config/load.hpp"
#include "flop/craft/parts/part_config.hpp"

namespace flop::config {

template <> struct ConfigDecoder<craft::PartConfig> {
  [[nodiscard]] static craft::PartConfig decode(const ConfigView &document);
};

template <> struct ConfigDecoder<craft::PartTypeConfig> {
  [[nodiscard]] static craft::PartTypeConfig decode(const ConfigView &document);
};

// the authored part type is its asset folder name, not a duplicated YAML field.
[[nodiscard]] craft::PartTypeConfig load_part_type(
    const AssetResolver &resolver, std::string_view type_id
);

} // namespace flop::config
