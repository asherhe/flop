#include "flop/craft/resources.hpp"

#include <algorithm>
#include <cmath>
#include <queue>
#include <stdexcept>
#include <unordered_set>
#include <variant>

#include "flop/craft/craft_context.hpp"
#include "flop/craft/parts/part_plugin_config.hpp"
#include "flop/craft/resource_registry.hpp"
#include "flop/craft/resource_type_config.hpp"

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

Reach reach_for_flow(ResourceFlow flow) {
  switch (flow) {
  case ResourceFlow::none:
    return Reach::none;
  case ResourceFlow::stage:
    return Reach::crossfeed;
  case ResourceFlow::craft:
    return Reach::craft;
  }
  throw std::invalid_argument("Unsupported resource flow mode");
}

// find the slot for a resource within a container, or nullptr if the container
// does not hold that resource.
const ResourceSlot *find_slot(const ContainerState &container, std::string_view resource_id) {
  const auto found = std::ranges::find(container.slots, resource_id, &ResourceSlot::resource_id);
  return found == container.slots.end() ? nullptr : &*found;
}

ResourceSlot *find_slot(ContainerState &container, std::string_view resource_id) {
  const auto found = std::ranges::find(container.slots, resource_id, &ResourceSlot::resource_id);
  return found == container.slots.end() ? nullptr : &*found;
}

ContainerState build_container(
    std::size_t part_index,
    const PartPluginConfig &plugin,
    const ResourceConfig &config,
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

CraftResources::CraftResources(std::span<const Part> parts, const world::UniverseContext &context) :
    registry_(&context.resources()) {
  const auto &registry = *registry_;
  for (std::size_t part_index = 0; part_index < parts.size(); ++part_index) {
    for (const auto &plugin : parts[part_index].plugins) {
      if (const auto *config = std::get_if<ResourceConfig>(&plugin.values)) {
        containers_.push_back(build_container(part_index, plugin, *config, registry));
      }
    }
  }
}

MassProperties CraftResources::mass_properties(const CraftContext &ctx) const {
  const auto parts = ctx.parts();
  std::vector<double> part_mass_kg(parts.size(), 0.0);
  for (std::size_t index = 0; index < parts.size(); ++index) {
    part_mass_kg[index] = parts[index].dry_mass_kg;
  }
  // add the live resource mass to each owning part. disabled containers still
  // hold their contents, so they are included here.
  for (const auto &container : containers_) {
    double container_mass_kg = 0.0;
    for (const auto &slot : container.slots) {
      container_mass_kg += slot.amount_units * registry_->at(slot.resource_id).kg_per_unit;
    }
    part_mass_kg[container.part_index] += container_mass_kg;
  }

  double total_mass_kg = 0.0;
  Vec2d weighted_position{};
  for (std::size_t index = 0; index < parts.size(); ++index) {
    total_mass_kg += part_mass_kg[index];
    weighted_position += part_mass_kg[index] * parts[index].position_meters;
  }
  if (total_mass_kg <= 0.0) {
    return {total_mass_kg, {}, 0.0};
  }
  const Vec2d center_of_mass = weighted_position / total_mass_kg;

  // calculate inertia using COM
  // TODO: account for moment of inertia of each part around its own COM
  double moment_of_inertia_kg_m2 = 0.0;
  for (std::size_t index = 0; index < parts.size(); ++index) {
    const Vec2d offset = parts[index].position_meters - center_of_mass;
    moment_of_inertia_kg_m2 += part_mass_kg[index] * offset.dot(offset);
  }
  return {total_mass_kg, center_of_mass, moment_of_inertia_kg_m2};
}

std::vector<std::size_t> CraftResources::reachable_containers(
    const CraftContext &ctx,
    std::size_t source_part_index,
    std::string_view resource_id,
    Reach reach
) const {
  const auto parts = ctx.parts();
  // containers_ indices whose owning part holds this resource and is enabled.
  const auto container_on_part = [&](std::size_t part_index) -> std::vector<std::size_t> {
    std::vector<std::size_t> found;
    for (std::size_t index = 0; index < containers_.size(); ++index) {
      if (containers_[index].part_index == part_index && containers_[index].enabled &&
          find_slot(containers_[index], resource_id) != nullptr) {
        found.push_back(index);
      }
    }
    return found;
  };

  std::vector<std::size_t> containers;
  switch (reach) {
  case Reach::none: {
    // can only be used by the part it's in.
    containers = container_on_part(source_part_index);
    break;
  }
  case Reach::crossfeed: {
    // breadth-first walk of the attachment graph, crossing only parts that have
    // crossfeed. the source part is always included. a visited set terminates
    // cycles in the attachment graph.
    std::queue<std::size_t> pending;
    pending.push(source_part_index);
    std::unordered_set<std::size_t> visited;
    while (!pending.empty()) {
      const std::size_t part_index = pending.front();
      pending.pop();
      if (!visited.insert(part_index).second) {
        continue;
      }
      for (const std::size_t index : container_on_part(part_index)) {
        containers.push_back(index);
      }
      for (const auto &attachment : parts[part_index].attachments) {
        const auto neighbor = std::ranges::find(parts, attachment.attach_part_id, &Part::id);
        if (neighbor == parts.end()) {
          continue;
        }
        if (neighbor->crossfeed) {
          pending.push(static_cast<std::size_t>(neighbor - parts.begin()));
        }
      }
    }
    break;
  }
  case Reach::craft: {
    // can be used by any part within the craft.
    for (std::size_t index = 0; index < containers_.size(); ++index) {
      if (containers_[index].enabled && find_slot(containers_[index], resource_id) != nullptr) {
        containers.push_back(index);
      }
    }
    break;
  }
  }
  return containers;
}

double CraftResources::read(
    const CraftContext &ctx, std::size_t source_part_index, std::string_view resource_id
) const {
  const auto reach = reach_for_flow(registry_->at(resource_id).flow);
  double available = 0.0;
  for (const std::size_t index : reachable_containers(ctx, source_part_index, resource_id, reach)) {
    available += find_slot(containers_[index], resource_id)->amount_units;
  }
  return available;
}

double CraftResources::drain(
    const CraftContext &ctx,
    std::size_t source_part_index,
    std::string_view resource_id,
    double requested_units
) {
  if (!(requested_units > 0.0)) {
    return 0.0;
  }
  const auto reach = reach_for_flow(registry_->at(resource_id).flow);
  auto reachable = reachable_containers(ctx, source_part_index, resource_id, reach);

  // group reachable containers by priority and drain higher priority first.
  std::ranges::sort(reachable, [&](std::size_t lhs, std::size_t rhs) {
    return find_slot(containers_[lhs], resource_id)->priority >
           find_slot(containers_[rhs], resource_id)->priority;
  });

  double remaining = requested_units;
  double delivered = 0.0;
  for (std::size_t group_begin = 0; group_begin < reachable.size();) {
    const int group_priority =
        find_slot(containers_[reachable[group_begin]], resource_id)->priority;
    std::size_t group_end = group_begin;
    double group_amount = 0.0;
    while (group_end < reachable.size() &&
           find_slot(containers_[reachable[group_end]], resource_id)->priority == group_priority) {
      group_amount += find_slot(containers_[reachable[group_end]], resource_id)->amount_units;
      ++group_end;
    }

    const double drain_amount = std::min(remaining, group_amount);
    if (group_amount > 0.0 && drain_amount > 0.0) {
      // fraction of the group's current contents to take from each container,
      // so that fuller tanks give proportionally more. the guard above ensures
      // the group holds something, so this never divides by zero.
      const double fraction = drain_amount / group_amount;
      for (std::size_t i = group_begin; i < group_end; ++i) {
        ResourceSlot *slot = find_slot(containers_[reachable[i]], resource_id);
        const double taken = std::min(slot->amount_units, slot->amount_units * fraction);
        slot->amount_units -= taken;
        delivered += taken;
      }
      remaining -= drain_amount;
    }

    if (!(remaining > 0.0)) {
      break;
    }
    group_begin = group_end;
  }
  return delivered;
}

} // namespace flop::craft
