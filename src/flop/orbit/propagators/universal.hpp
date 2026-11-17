#pragma once

#include <optional>

#include "flop/orbit/state.hpp"

namespace flop::orbit {

// orbital propagation using the universal variable formulation
class UniversalPropagator {
public:
  UniversalPropagator(double mu, const OrbitState &orbit);

  // get position or velocity at a given time relative to the reference center
  [[nodiscard]] Vec2d position_at(double ut) const;
  [[nodiscard]] Vec2d velocity_at(double ut) const;

  // get both state vectors with a single universal anomaly solve
  [[nodiscard]] StateVectors state_at(double ut) const;

private:
  // propagated position in normalized units and the terms needed to compute its velocity
  struct PositionSample {
    Vec2d normalized_position;
    // universal anomaly and its factors chi^2 * C(z) and z * S(z)
    double chi;
    double square_c;
    double z_s;
  };

  // an empty sample means the reduced elapsed time is zero
  [[nodiscard]] std::optional<PositionSample> sample_position(double ut) const;
  [[nodiscard]] Vec2d position_from_sample(const PositionSample &sample) const;
  [[nodiscard]] Vec2d velocity_from_sample(const PositionSample &sample) const;

  OrbitState orbit_;
  double radius_at_epoch_;
  double speed_scale_;
  double time_scale_;
  Vec2d initial_position_;
  Vec2d initial_velocity_;
  // reciprocal semimajor axis in normalized units, used to determine orbit shape
  double alpha_;
  double radial_speed_;
  double period_{};
  double period_tail_{};
};

} // namespace flop::orbit
