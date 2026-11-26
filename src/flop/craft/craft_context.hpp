#pragma once

#include <span>

// self-contained accessor interface for the members of one craft that its
// resource, rigid-body, and (later) plugin behavior read while stepping. kept
// free of includes beyond <span> (only forward declarations otherwise) so that
// the components implementing per-step behavior can depend on this seam rather
// than on the concrete Craft, which lets them be exercised against a stand-in
// context in tests. flop::craft::Craft implements this interface; callers that
// use the returned references include the concrete headers themselves.

namespace flop::craft {

struct Part;
struct MassProperties;
class CraftControl;
class CraftResources;
class CraftNewtonian;

class CraftContext {
public:
  CraftContext() = default;
  CraftContext(const CraftContext &) = default;
  CraftContext(CraftContext &&) = default;
  CraftContext &operator=(const CraftContext &) = default;
  CraftContext &operator=(CraftContext &&) = default;
  virtual ~CraftContext() = default;

  [[nodiscard]] virtual std::span<const Part> parts() const = 0;
  [[nodiscard]] virtual std::span<Part> mutable_parts() = 0;

  // persistent control channel: the command plugin writes throttle/steering,
  // the actuators read them within the same step.
  [[nodiscard]] virtual const CraftControl &control() const = 0;
  [[nodiscard]] virtual CraftControl &control() = 0;

  [[nodiscard]] virtual const CraftResources &resources() const = 0;
  [[nodiscard]] virtual CraftResources &resources() = 0;

  [[nodiscard]] virtual const CraftNewtonian &newtonian() const = 0;
  [[nodiscard]] virtual CraftNewtonian &newtonian() = 0;

  // total mass, center of mass, and rotational inertia of the craft at the
  // instant of the call, aggregated from part dry mass and live resource
  // quantities. recomputed each call; a stepping component takes one snapshot
  // and reuses it for the whole step.
  [[nodiscard]] virtual MassProperties mass_properties() const = 0;
};

} // namespace flop::craft
