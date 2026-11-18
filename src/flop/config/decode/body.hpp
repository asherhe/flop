#pragma once

#include "flop/config/load.hpp"
#include "flop/world/body_config.hpp"

namespace flop::config {

template <> struct ConfigDecoder<world::BodyConfig> {
  [[nodiscard]] static world::BodyConfig decode(const ConfigView &document);
};

} // namespace flop::config
