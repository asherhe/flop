#pragma once

#include <span>
#include <string>
#include <vector>

#include "flop/craft/part.hpp"
#include "flop/world/universe_context.hpp"

namespace flop::craft {

// mutable runtime state of one resource type inside one container.
struct ResourceSlot {
  std::string resource_id;
  double amount_units;
  double max_amount_units;
  int priority;
};

// mutable runtime state of a single installed resource container plugin.
struct ContainerState {
  // index of the owning part within the craft's parts span.
  std::size_t part_index;
  // whether the owning plugin is enabled. a disabled container still holds its
  // contents (they count toward mass) but is not drawn from.
  bool enabled;
  std::vector<ResourceSlot> slots;
};

// owns the mutable resource quantities for one craft, built from the resolved
// ResourceContainerConfig of each installed part. config amounts are starting
// values; the live amounts here are what change as resources are drained.
class CraftResources {
public:
  // an empty resource set; used before a craft's parts are assembled.
  CraftResources() = default;

  // scans parts for ResourceContainerConfig plugins and builds one container
  // state per plugin. validates resource ids against the registry, requires
  // finite capacities and amounts, and clamps each starting amount into
  // [0, capacity].
  CraftResources(std::span<const Part> parts, const world::UniverseContext &context);

  [[nodiscard]] std::span<const ContainerState> containers() const noexcept { return containers_; }

private:
  std::vector<ContainerState> containers_;
};

} // namespace flop::craft
