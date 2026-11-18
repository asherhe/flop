#pragma once

#include "flop/config/load.hpp"
#include "flop/craft/part_plugin_config.hpp"

namespace flop::config {

[[nodiscard]] craft::PartPluginConfig decode_part_plugin(std::string name, const ConfigView &value);
void apply_part_plugin_fields(craft::PartPluginConfig &config, const ConfigView &value);

} // namespace flop::config
