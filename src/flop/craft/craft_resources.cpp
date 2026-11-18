#include "flop/craft/craft_resources.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <variant>

#include "flop/craft/part_plugin_config.hpp"
#include "flop/craft/resource_registry.hpp"

namespace flop::craft {
namespace {

double lookup_or_zero(const std::map<std::string, double> &values, const std::string &key) {
  const auto found = values.find(key);
  return found == values.end() ? 0.0 : found->second;
}

int lookup_or_zero(const std::map<std::string, int> &values, const std::string &key) {
  const auto found = values.find(key);
  return found == values.end() ? 0 : found->second;
}

ContainerState build_container(
    std::size_t part_index,
    const PartPluginConfig &plugin,
    const ResourceContainerConfig &config,
    const ResourceRegistry &registry
) {
  ContainerState state{part_index, plugin.enabled, {}};
  state.slots.reserve(config.max_amount_units.size());
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
    state.slots.push_back({resource_id, clamped, max_amount, priority});
  }
  return state;
}

} // namespace

CraftResources::CraftResources(std::span<const Part> parts, const world::UniverseContext &context) {
  const auto &registry = context.resources();
  for (std::size_t part_index = 0; part_index < parts.size(); ++part_index) {
    for (const auto &plugin : parts[part_index].plugins) {
      if (const auto *config = std::get_if<ResourceContainerConfig>(&plugin.values)) {
        containers_.push_back(build_container(part_index, plugin, *config, registry));
      }
    }
  }
}

} // namespace flop::craft
