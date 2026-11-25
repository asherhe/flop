#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "flop/craft/plugins/part_plugin.hpp"
#include "flop/craft/plugins/config.hpp"
#include "flop/math/vec2.hpp"

namespace flop::craft {
class ResourceRegistry;
} // namespace flop::craft

namespace flop::craft::plugin {

// a rocket engine. while the throttle is open it burns its configured
// propellant mixture at the rate the rocket equation demands, scales thrust
// down to whatever propellant it can actually draw, drains that propellant from
// the parts it can reach, and applies the resulting thrust at its mounting
// point. running a propellant dry shuts the engine down for the rest of the
// flight.
class EnginePlugin : public PartPlugin {
public:
  EnginePlugin(EngineConfig config, const ResourceRegistry &registry, bool enabled);

  void tick(CraftContext &ctx, double dt_sec) override;

  [[nodiscard]] const EngineConfig &config() const noexcept { return config_; }

private:
  // one propellant in the mixture: its resource id, its relative ratio, and its
  // mass per unit, kept so the per-step draw and the limiting-propellant scaling
  // avoid repeated registry lookups.
  struct Propellant {
    std::string resource_id;
    double ratio;
    double kg_per_unit;
  };

  EngineConfig config_;
  std::vector<Propellant> propellants_;
  // total propellant mass consumed if the configured ratios are burned once, in
  // kg. zero when the engine has no usable propellant mixture.
  double prop_ratio_mass_{};
  // thrust direction in craft space, rotated from the part-space configuration
  // by the owning part's mounting angle. resolved on the first tick, once the
  // owning part is reachable through the context.
  std::optional<Vec2d> craft_thrust_direction_;
};

} // namespace flop::craft::plugin
