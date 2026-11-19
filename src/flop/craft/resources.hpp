#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "flop/craft/parts/part.hpp"
#include "flop/math/vec2.hpp"
#include "flop/world/universe_context.hpp"

namespace flop::craft {

class ResourceRegistry;

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

// describes the permitted degree of resource flow within a craft.
//   - None can only be used by the part it's in (e.g. solid fuel, ablator)
//   - Crossfeed can be used across parts connected by crossfeed (liquid fuel/oxidizer)
//   - Craft can be used by any part within the craft (e.g. electricity).
enum class Reach : std::uint8_t { none, crossfeed, craft };

// total mass and center of mass of an assembled craft at one instant.
struct MassProperties {
  double total_mass_kg;
  // mass-weighted average part position, in craft construction space. (0,0) when total_mass_kg == 0
  Vec2d center_of_mass_meters;
  // rotational inertia about the center of mass, in kg*m^2, treating each part (dry mass plus its
  // resources) as a point mass. zero when total_mass_kg == 0
  // TODO: include inertia of each individual part in this computation
  double moment_of_inertia_kg_m2;
};

// internal table of resource quantities in a Craft
class CraftResources {
public:
  CraftResources() = default;

  // scan parts for ResourceContainerPlugins to initialize resource tracking
  CraftResources(std::span<const Part> parts, const world::UniverseContext &context);

  [[nodiscard]] std::span<const ContainerState> containers() const noexcept { return containers_; }

  // total mass, center of mass, and rotational inertia from installed dry mass
  // plus the live resource quantities. disabled containers still contribute
  // their contents. computed on demand each call; there is no cache yet. if
  // profiling later shows this on a hot path, drain operations can mark a dirty
  // flag instead.
  [[nodiscard]] MassProperties mass_properties(std::span<const Part> parts) const;

  // the amount of resource available to a part, summed over every enabled
  // container the part can reach for that resource. the reach mode is taken
  // from the resource's registry flow. does not mutate any container.
  [[nodiscard]] double read(
      std::span<const Part> parts, std::size_t source_part_index, std::string_view resource_id
  ) const;

  // drains a resource, choosing containers by priority and the resource's flow
  // mode. higher priority is drained first; within one priority group each
  // container gives up the same fraction of its current contents. returns the
  // actual units delivered, in [0, requested]. amounts stay within
  // [0, capacity] and no division is performed against an empty group.
  double drain(
      std::span<const Part> parts,
      std::size_t source_part_index,
      std::string_view resource_id,
      double requested_units
  );

private:
  // get all reachable containers for a given resource, based on Reach rules
  // returns indices into containers_. only enabled containers are reachable; a disabled container
  // holds its contents but cannot be drained
  [[nodiscard]] std::vector<std::size_t> reachable_containers(
      std::span<const Part> parts,
      std::size_t source_part_index,
      std::string_view resource_id,
      Reach reach
  ) const;

  // universal resource registry, used for kg_per_unit.
  // null only for a default-constructed instance.
  const ResourceRegistry *registry_{nullptr};
  std::vector<ContainerState> containers_;
};

} // namespace flop::craft
