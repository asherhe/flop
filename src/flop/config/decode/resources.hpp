#pragma once

#include <vector>

#include "flop/config/load.hpp"
#include "flop/craft/resource_config.hpp"

namespace flop::config {

template <> struct ConfigDecoder<std::vector<craft::ResourceConfig>> {
  [[nodiscard]] static std::vector<craft::ResourceConfig> decode(const ConfigView &document);
};

} // namespace flop::config
