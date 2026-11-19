#include "flop/craft/parts/part.hpp"

#include "flop/craft/plugins/part_plugin.hpp"
#include "flop/craft/plugins/part_plugin_factory.hpp"

namespace flop::craft {

Part::Part() = default;
Part::Part(Part &&) noexcept = default;
Part &Part::operator=(Part &&) noexcept = default;
Part::~Part() = default;

void Part::install_plugins(const ResourceRegistry &registry) {
  installed_plugins_.clear();
  installed_plugins_.reserve(plugins.size());
  for (const auto &plugin_config : plugins) {
    installed_plugins_.push_back(plugin::make_plugin(plugin_config, registry));
  }
}

void Part::step(CraftContext &ctx, double dt_sec) {
  for (const auto &plugin : installed_plugins_) {
    if (plugin->enabled()) {
      plugin->tick(ctx, dt_sec);
    }
  }
}

} // namespace flop::craft
