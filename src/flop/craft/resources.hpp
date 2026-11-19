#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "flop/craft/parts/part.hpp"
#include "flop/math/vec2.hpp"
#include "flop/world/universe_context.hpp"

namespace flop::craft {

class ResourceRegistry;
namespace plugin {
class ResourcePlugin;
} // namespace plugin
class CraftContext;

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

// read-only interface over the resource containers a craft's parts own. holds
// no container storage of its own: each ResourcePlugin owns its tank slots, and
// this aggregates across parts reached through the craft context.
class CraftResources {
public:
  CraftResources() = default;

  CraftResources(std::span<const Part> parts, const world::UniverseContext &context);

  // total mass, center of mass, and rotational inertia from installed dry mass
  // plus the live resource quantities. disabled containers still contribute
  // their contents. computed on demand each call; there is no cache yet. if
  // profiling later shows this on a hot path, drain operations can mark a dirty
  // flag instead.
  [[nodiscard]] MassProperties mass_properties(const CraftContext &ctx) const;

  // the amount of resource available to a part, summed over every enabled
  // container the part can reach for that resource. the reach mode is taken
  // from the resource's registry flow. does not mutate any container.
  [[nodiscard]] double read(
      const CraftContext &ctx, std::size_t source_part_index, std::string_view resource_id
  ) const;

  // drains a resource, choosing containers by priority and the resource's flow
  // mode. higher priority is drained first; within one priority group each
  // container gives up the same fraction of its current contents. returns the
  // actual units delivered, in [0, requested]. amounts stay within
  // [0, capacity] and no division is performed against an empty group.
  double drain(
      const CraftContext &ctx,
      std::size_t source_part_index,
      std::string_view resource_id,
      double requested_units
  );

private:
  // the enabled resource containers that can supply a resource to the source
  // part, following the resource's reach rules. a disabled container holds its
  // contents (they count toward mass) but is not reachable for read or drain.
  [[nodiscard]] std::vector<plugin::ResourcePlugin *> reachable_containers(
      const CraftContext &ctx,
      std::size_t source_part_index,
      std::string_view resource_id,
      Reach reach
  ) const;

  // universal resource registry, used for kg_per_unit.
  // null only for a default-constructed instance.
  const ResourceRegistry *registry_{nullptr};
};

} // namespace flop::craft
