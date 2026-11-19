#include "flop/craft/plugins/engine_plugin.hpp"

#include <algorithm>

#include "flop/craft/control.hpp"
#include "flop/craft/craft_context.hpp"
#include "flop/craft/newtonian.hpp"
#include "flop/craft/parts/part.hpp"
#include "flop/craft/resource_registry.hpp"
#include "flop/craft/resources.hpp"

namespace flop::craft::plugin {
namespace {

// standard gravity used to turn specific impulse (seconds) into exhaust
// velocity for the mass-flow rate, matching the value the engine model is tuned
// against.
constexpr double standard_gravity_m_per_s2 = 9.8;

} // namespace

EnginePlugin::EnginePlugin(EngineConfig config, const ResourceRegistry &registry, bool enabled) :
    PartPlugin(enabled), config_(std::move(config)) {
  propellants_.reserve(config_.propellant_ratio.size());
  for (const auto &[resource_id, ratio] : config_.propellant_ratio) {
    const double kg_per_unit = registry.at(resource_id).kg_per_unit;
    propellants_.push_back({resource_id, ratio, kg_per_unit});
    prop_ratio_mass_ += ratio * kg_per_unit;
  }
}

void EnginePlugin::tick(CraftContext &ctx, double dt_sec) {
  const double throttle = ctx.control().throttle();
  if (!(throttle > 0.0)) {
    return;
  }
  // an engine with no isp or no usable propellant mixture can produce no
  // meaningful flow; treat it as flamed out rather than dividing by zero.
  if (!(config_.isp_sec > 0.0) || !(prop_ratio_mass_ > 0.0)) {
    set_enabled(false);
    return;
  }

  const Part &part = ctx.parts()[part_index()];
  if (!craft_thrust_direction_.has_value()) {
    craft_thrust_direction_ = config_.thrust_direction.rotate(part.angle_rad);
  }

  double thrust = config_.thrust_newtons * throttle;
  // propellant mass the engine wants to consume this step, from the rocket
  // equation, then the mass per unit of configured ratio.
  const double prop_flow = dt_sec * thrust / (config_.isp_sec * standard_gravity_m_per_s2);
  double prop_coeff = prop_flow / prop_ratio_mass_;

  // scale thrust by the propellant the engine can actually draw: the most
  // limiting propellant caps the whole burn.
  double prop_factor = 1.0;
  for (const auto &propellant : propellants_) {
    const double required = prop_coeff * propellant.ratio;
    if (!(required > 0.0)) {
      continue;
    }
    const double available =
        ctx.resources().read(ctx, part_index(), propellant.resource_id) * propellant.kg_per_unit;
    prop_factor = std::min(prop_factor, available / required);
  }
  prop_factor = std::max(prop_factor, 0.0);
  thrust *= prop_factor;
  prop_coeff *= prop_factor;

  if (!(prop_factor > 0.0)) {
    // no propellant left to draw: shut the engine down.
    set_enabled(false);
    return;
  }

  for (const auto &propellant : propellants_) {
    const double units = prop_coeff * propellant.ratio / propellant.kg_per_unit;
    ctx.resources().drain(ctx, part_index(), propellant.resource_id, units);
  }
  ctx.newtonian().apply_force(thrust * *craft_thrust_direction_, part.position_meters);
}

} // namespace flop::craft::plugin
