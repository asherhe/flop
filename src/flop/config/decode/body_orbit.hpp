#pragma once

#include "flop/config/load.hpp"
#include "flop/world/body_config.hpp"

namespace flop::config {

template <> struct ConfigDecoder<world::BodyOrbitConfig> {
  [[nodiscard]] static world::BodyOrbitConfig decode(const ConfigView &document);
};

} // namespace flop::config
