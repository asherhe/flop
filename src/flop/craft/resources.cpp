#include "flop/craft/resources.hpp"

#include <algorithm>
#include <queue>
#include <stdexcept>
#include <unordered_set>

#include "flop/craft/craft_context.hpp"
#include "flop/craft/plugins/resource.hpp"
#include "flop/craft/resource_registry.hpp"
#include "flop/craft/resource_type_config.hpp"

namespace flop::craft {
namespace {

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

// the resource containers installed on one part, in installation order.
template <class PartT, class PluginT> std::vector<PluginT *> containers_on(PartT &part) {
  std::vector<PluginT *> containers;
  for (const auto &plugin : part.installed_plugins()) {
    if (auto *container = dynamic_cast<PluginT *>(plugin.get())) {
      containers.push_back(container);
    }
  }
  return containers;
}

} // namespace

CraftResources::CraftResources(
    std::span<const Part> /*parts*/, const world::UniverseContext &context
) : registry_(&context.resources()) {}

MassProperties CraftResources::mass_properties(const CraftContext &ctx) const {
  const auto parts = ctx.parts();
  std::vector<double> part_mass_kg(parts.size(), 0.0);
  for (std::size_t index = 0; index < parts.size(); ++index) {
    part_mass_kg[index] = parts[index].dry_mass_kg;
    // add the live resource mass held on this part. disabled containers still
    // hold their contents, so they are included here.
    for (const auto *container : containers_on<const Part, const plugin::ResourcePlugin>(parts[index])) {
      for (const auto &slot : container->slots()) {
        part_mass_kg[index] += slot.amount_units * registry_->at(slot.resource_id).kg_per_unit;
      }
    }
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

std::vector<plugin::ResourcePlugin *> CraftResources::reachable_containers(
    const CraftContext &ctx,
    std::size_t source_part_index,
    std::string_view resource_id,
    Reach reach
) const {
  const auto parts = ctx.parts();
  // the enabled containers on one part that hold this resource.
  const auto containers_on_part = [&](std::size_t part_index) {
    std::vector<plugin::ResourcePlugin *> found;
    for (auto *container : containers_on<const Part, plugin::ResourcePlugin>(parts[part_index])) {
      if (container->enabled() && container->find_slot(resource_id) != nullptr) {
        found.push_back(container);
      }
    }
    return found;
  };

  std::vector<plugin::ResourcePlugin *> containers;
  switch (reach) {
  case Reach::none: {
    // can only be used by the part it's in.
    containers = containers_on_part(source_part_index);
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
      for (auto *container : containers_on_part(part_index)) {
        containers.push_back(container);
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
    for (std::size_t part_index = 0; part_index < parts.size(); ++part_index) {
      for (auto *container : containers_on_part(part_index)) {
        containers.push_back(container);
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
  for (auto *container : reachable_containers(ctx, source_part_index, resource_id, reach)) {
    available += container->find_slot(resource_id)->amount_units;
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
  std::ranges::sort(reachable, [&](const plugin::ResourcePlugin *lhs, const plugin::ResourcePlugin *rhs) {
    return lhs->find_slot(resource_id)->priority > rhs->find_slot(resource_id)->priority;
  });

  double remaining = requested_units;
  double delivered = 0.0;
  for (std::size_t group_begin = 0; group_begin < reachable.size();) {
    const int group_priority = reachable[group_begin]->find_slot(resource_id)->priority;
    std::size_t group_end = group_begin;
    double group_amount = 0.0;
    while (group_end < reachable.size() &&
           reachable[group_end]->find_slot(resource_id)->priority == group_priority) {
      group_amount += reachable[group_end]->find_slot(resource_id)->amount_units;
      ++group_end;
    }

    const double drain_amount = std::min(remaining, group_amount);
    if (group_amount > 0.0 && drain_amount > 0.0) {
      // fraction of the group's current contents to take from each container,
      // so that fuller tanks give proportionally more. the guard above ensures
      // the group holds something, so this never divides by zero.
      const double fraction = drain_amount / group_amount;
      for (std::size_t i = group_begin; i < group_end; ++i) {
        plugin::ResourceSlot *slot = reachable[i]->find_slot(resource_id);
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
