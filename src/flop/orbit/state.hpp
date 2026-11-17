#pragma once

#include "flop/math/vec2.hpp"
#include "flop/orbit/frame_id.hpp"

namespace flop::orbit {

// angular inputs to trigonometric functions use radians.

// cartesian position and velocity in the same reference frame at a single time
struct StateVectors {
  Vec2d position;
  Vec2d velocity;
};

// represents a particular orbit around a reference center, given its cartesian
// state at epoch. propagation takes the center's gravitational parameter mu separately.
struct OrbitState {
  // time at which the position and velocity apply
  double epoch;
  // state vectors at epoch
  StateVectors state;
  // frame in which the state vectors are expressed
  FrameId frame_id{};
};

// elements used to construct a cartesian orbit state at epoch
struct OrbitalElements {
  // specific angular momentum around the reference center. positive is counterclockwise.
  double specific_angular_momentum;
  // orbit shape: 0 is circular, 0 < e < 1 is elliptic, 1 is parabolic, and e > 1 is hyperbolic.
  double eccentricity;
  // angle in radians from the +x axis to the periapsis of the orbit
  double longitude_of_periapsis;
  // mean anomaly at epoch: radians for ellipses, dimensionless for hyperbolas and parabolas
  // the parabolic reference convention is M = D / 2 + D^3 / 6
  double mean_anomaly;
};

// derived orbital parameters computed by geometry() from a cartesian seed and
// the gravitational parameter. these describe the conic and its period.
struct OrbitGeometry {
  // specific angular momentum around the reference center. positive is counterclockwise.
  double specific_angular_momentum;
  // eccentricity, which describes the shape of the orbit
  double eccentricity;
  // semilatus rectum p = h^2 / mu, the distance at true anomaly +/-pi/2
  double semilatus_rectum;
  // periapsis distance from the central body
  double periapsis;
  // apoapsis distance from the central body, or infinity for an unbound orbit
  double apoapsis;
  // time for one revolution, or infinity for an unbound orbit
  double period;
};

// construct an orbit from orbital elements using kepler's equation for e < 1 or e > 1,
// and barker's equation for exactly e == 1
[[nodiscard]] OrbitState state_at_epoch_from_elements(
    double mu, double epoch, OrbitalElements elements, FrameId frame_id = {}
);
// calculate the derived orbital parameters for this seed and gravitational parameter
[[nodiscard]] OrbitGeometry geometry(double mu, const OrbitState &orbit);

} // namespace flop::orbit
