#pragma once

#include <vector>

#include "flop/craft/resources.hpp"
#include "flop/math/vec2.hpp"
#include "flop/orbit/state.hpp"

namespace flop::craft {

struct CraftAttitudeConfig;

// linear and rotational rigid-body state for one craft, integrated from forces
// and torques accumulated over a step.
//
// forces and torques are gathered during a step through apply_force and
// apply_torque, then actuated once by step(), which is given a single mass
// snapshot taken at a defined point in the step. the snapshot supplies the
// total mass used for linear acceleration, the center of mass used as the arm
// origin for force-induced torque, and the moment of inertia used for angular
// acceleration. nothing here subscribes to mass changes; each step reads the
// snapshot fresh.
class CraftNewtonian {
public:
  CraftNewtonian() = default;
  explicit CraftNewtonian(const CraftAttitudeConfig &attitude);

  // angle of the craft's local +x axis counterclockwise from the frame +x axis.
  [[nodiscard]] double angle_radians() const noexcept { return angle_radians_; }
  // spin angular momentum about the center of mass, in kg*m^2/s.
  [[nodiscard]] double angular_momentum_kg_m2_per_s() const noexcept {
    return angular_momentum_kg_m2_per_s_;
  }
  // angular velocity about the center of mass, in rad/s. zero when the moment of
  // inertia from the last step was zero.
  [[nodiscard]] double angular_velocity_rad_per_s() const noexcept {
    return angular_velocity_rad_per_s_;
  }

  // accumulate a force, in newtons, applied at a point in craft space (meters).
  // the point is kept so the torque arm can be resolved against the step's
  // center-of-mass snapshot.
  void apply_force(Vec2d force_newtons, Vec2d craft_position_meters);
  // accumulate a pure torque about the center of mass, in newton-meters.
  void apply_torque(double torque_newton_meters);

  // apply accumulated forces and torques, integrate motion, and clear
  // accumulators. returns true iff the linear velocity was changed this step
  // (a nonzero net force applied with positive mass); a coasting craft returns
  // false, and a changed attitude alone does not count. the caller uses this to
  // decide whether to re-seed the orbit.
  bool step(const MassProperties &mass, orbit::StateVectors &state, double dt_sec);

private:
  struct AppliedForce {
    Vec2d force_newtons;
    Vec2d craft_position_meters;
  };

  double angle_radians_{};
  double angular_momentum_kg_m2_per_s_{};
  double angular_velocity_rad_per_s_{};
  std::vector<AppliedForce> accumulated_forces_;
  double accumulated_torque_newton_meters_{};
};

} // namespace flop::craft
