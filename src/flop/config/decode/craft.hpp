#pragma once

#include "flop/config/load.hpp"
#include "flop/craft/craft_config.hpp"

namespace flop::config {

template <> struct ConfigDecoder<craft::CraftConfig> {
  [[nodiscard]] static craft::CraftConfig decode(const ConfigView &document);
};

} // namespace flop::config
