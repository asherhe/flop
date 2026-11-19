#include "flop/craft/plugins/resource_plugin.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

#include "flop/craft/plugins/config.hpp"
#include "flop/craft/resource_registry.hpp"

namespace flop::craft::plugin {
namespace {

double lookup_or_zero(const std::map<std::string, double> &values, const std::string &key) {
  const auto found = values.find(key);
  return found == values.end() ? 0.0 : found->second;
}

int lookup_or_zero(const std::map<std::string, int> &values, const std::string &key) {
  const auto found = values.find(key);
  return found == values.end() ? 0 : found->second;
}

} // namespace

ResourcePlugin::ResourcePlugin(
    const ResourceConfig &config, const ResourceRegistry &registry, bool enabled
) : PartPlugin(enabled) {
  slots_.reserve(config.max_amount_units.size());
  // std::map iterates in sorted key order, giving a deterministic slot layout.
  for (const auto &[resource_id, max_amount] : config.max_amount_units) {
    // throws if the resource id is not registered; the value is unused here.
    static_cast<void>(registry.at(resource_id));
    if (!std::isfinite(max_amount) || max_amount < 0) {
      throw std::invalid_argument("Resource container capacity must be finite and non-negative");
    }
    const double amount = lookup_or_zero(config.amount_units, resource_id);
    if (!std::isfinite(amount)) {
      throw std::invalid_argument("Resource container amount must be finite");
    }
    const double clamped = std::clamp(amount, 0.0, max_amount);
    const int priority = lookup_or_zero(config.priority, resource_id);
    slots_.push_back({resource_id, clamped, max_amount, priority});
  }
}

void ResourcePlugin::tick(CraftContext & /*ctx*/, double /*dt_sec*/) {}

const ResourceSlot *ResourcePlugin::find_slot(std::string_view resource_id) const {
  const auto found = std::ranges::find(slots_, resource_id, &ResourceSlot::resource_id);
  return found == slots_.end() ? nullptr : &*found;
}

ResourceSlot *ResourcePlugin::find_slot(std::string_view resource_id) {
  const auto found = std::ranges::find(slots_, resource_id, &ResourceSlot::resource_id);
  return found == slots_.end() ? nullptr : &*found;
}

} // namespace flop::craft::plugin
