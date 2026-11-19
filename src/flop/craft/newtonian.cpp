#include "flop/craft/newtonian.hpp"

#include "flop/craft/craft_config.hpp"
#include "flop/craft/craft_context.hpp"
#include "flop/math/utils.hpp"

namespace flop::craft {

CraftNewtonian::CraftNewtonian(const CraftAttitudeConfig &attitude) :
    angle_rad_(wrap_angle(attitude.angle_rad)),
    angular_momentum_kg_m2_per_s_(attitude.angular_momentum_kg_m2_per_s) {}

void CraftNewtonian::apply_force(Vec2d force_newtons, Vec2d craft_position_meters) {
  accumulated_forces_.push_back({force_newtons, craft_position_meters});
}

void CraftNewtonian::apply_torque(double torque_newton_meters) {
  accumulated_torque_newton_meters_ += torque_newton_meters;
}

bool CraftNewtonian::step(const CraftContext &ctx, orbit::StateVectors &state, double dt_sec) {
  const auto mass = ctx.mass_properties();

  // resolve accumulated forces
  Vec2d net_force_N{};
  double net_torque_N_m = accumulated_torque_newton_meters_;
  for (const auto &applied : accumulated_forces_) {
    net_force_N += applied.force_newtons;
    const Vec2d arm = applied.craft_position_meters - mass.center_of_mass_meters;
    net_torque_N_m += arm.cross(applied.force_newtons);
  }

  // rotational integration
  if (mass.moment_of_inertia_kg_m2 > 0.0) {
    angular_momentum_kg_m2_per_s_ += net_torque_N_m * dt_sec;
    angular_velocity_rad_per_s_ = angular_momentum_kg_m2_per_s_ / mass.moment_of_inertia_kg_m2;
    angle_rad_ = wrap_angle(angle_rad_ + angular_velocity_rad_per_s_ * dt_sec);
  } else {
    angular_velocity_rad_per_s_ = 0.0;
  }

  // velocity integration
  // forces are accumulated in craft space so they need to be converted to world space
  const bool velocity_changed = mass.total_mass_kg > 0.0 && net_force_N != Vec2d{};
  if (velocity_changed) {
    const Vec2d frame_force_N = net_force_N.rotate(angle_rad_);
    state.velocity += frame_force_N / mass.total_mass_kg * dt_sec;
  }

  accumulated_forces_.clear();
  accumulated_torque_newton_meters_ = 0.0;
  return velocity_changed;
}

} // namespace flop::craft
