#include "flop/craft/parts/part.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "flop/craft/plugins/part_plugin.hpp"
#include "flop/craft/plugins/plugin_factory.hpp"

namespace flop::craft {

Part::Part() = default;
Part::Part(Part &&other) noexcept : Part() { *this = std::move(other); }
Part &Part::operator=(Part &&other) noexcept {
  if (this == &other) return *this;
  type_id = std::move(other.type_id);
  id = std::move(other.id);
  name = std::move(other.name);
  position_meters = other.position_meters;
  angle_rad = other.angle_rad;
  dry_mass_kg = other.dry_mass_kg;
  crossfeed = other.crossfeed;
  attachment_nodes = std::move(other.attachment_nodes);
  attachments = std::move(other.attachments);
  plugins = std::move(other.plugins);
  installed_plugins_ = std::move(other.installed_plugins_);
  for (const auto &plugin : installed_plugins_) plugin->bind_part(*this);
  return *this;
}
Part::~Part() = default;

void Part::install_plugins(const ResourceRegistry &registry, std::size_t part_index) {
  installed_plugins_.clear();
  installed_plugins_.reserve(plugins.size());
  for (const auto &plugin_config : plugins) {
    // the authored id defaults to the plugin name, which keeps the current
    // one-instance-per-type assets working while allowing an explicit id later.
    PluginId plugin_id(plugin_config.id.empty() ? plugin_config.name : plugin_config.id);
    if (plugin_id.empty()) {
      throw std::invalid_argument("Part '" + id + "' has a plugin with an empty id");
    }
    if (std::ranges::any_of(installed_plugins_, [&](const auto &installed) {
          return installed->id() == plugin_id;
        })) {
      throw std::invalid_argument(
          "Part '" + id + "' has duplicate plugin id '" + plugin_id.value() + "'"
      );
    }
    auto plugin = plugin::make_plugin(plugin_config, registry);
    plugin->bind_part(*this);
    plugin->set_part_index(part_index);
    plugin->set_id(std::move(plugin_id));
    installed_plugins_.push_back(std::move(plugin));
  }
}

const plugin::PartPlugin *Part::find_plugin(const PluginId &plugin_id) const {
  const auto found = std::ranges::find_if(installed_plugins_, [&](const auto &installed) {
    return installed->id() == plugin_id;
  });
  return found == installed_plugins_.end() ? nullptr : found->get();
}

plugin::PartPlugin *Part::find_plugin(const PluginId &plugin_id) {
  const auto found = std::ranges::find_if(installed_plugins_, [&](const auto &installed) {
    return installed->id() == plugin_id;
  });
  return found == installed_plugins_.end() ? nullptr : found->get();
}

void Part::step_phase(CraftContext &ctx, double dt_sec, plugin::TickPhase phase) {
  for (const auto &plugin : installed_plugins_) {
    if (plugin->enabled() && plugin->tick_phase() == phase) {
      plugin->tick(ctx, dt_sec);
    }
  }
}

} // namespace flop::craft
