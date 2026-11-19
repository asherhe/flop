#include "flop/craft/parts/part.hpp"

#include <utility>

#include "flop/craft/plugins/part_plugin.hpp"
#include "flop/craft/plugins/plugin_factory.hpp"

namespace flop::craft {

Part::Part() = default;
Part::Part(Part &&) noexcept = default;
Part &Part::operator=(Part &&) noexcept = default;
Part::~Part() = default;

void Part::install_plugins(const ResourceRegistry &registry, std::size_t part_index) {
  installed_plugins_.clear();
  installed_plugins_.reserve(plugins.size());
  for (const auto &plugin_config : plugins) {
    auto plugin = plugin::make_plugin(plugin_config, registry);
    plugin->set_part_index(part_index);
    installed_plugins_.push_back(std::move(plugin));
  }
}

void Part::step_phase(CraftContext &ctx, double dt_sec, plugin::TickPhase phase) {
  for (const auto &plugin : installed_plugins_) {
    if (plugin->enabled() && plugin->tick_phase() == phase) {
      plugin->tick(ctx, dt_sec);
    }
  }
}

} // namespace flop::craft
