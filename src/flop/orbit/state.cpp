#include "flop/orbit/state.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <numeric>
#include <stdexcept>

namespace flop::orbit {
namespace {

template <class Equation> double increasing_root(double lower, double upper, Equation equation) {
  for (int iteration = 0; iteration < 256; ++iteration) {
    const double middle = std::midpoint(lower, upper);
    const double value = equation(middle);
    if (value == 0 || middle == lower || middle == upper) {
      return middle;
    }
    if (value < 0) {
      lower = middle;
    } else {
      upper = middle;
    }
  }
  throw std::runtime_error("Epoch anomaly did not converge");
}

// preserve the small cubic term near periapsis when x and sin(x) nearly cancel.
double circular_anomaly_residual(double x) {
  if (std::abs(x) >= 0.1) {
    return x - std::sin(x);
  }
  const double square = x * x;
  return x * square * (1.0 / 6 - square * (1.0 / 120 - square * (1.0 / 5040 - square / 362880)));
}

double hyperbolic_anomaly_residual(double x) {
  if (std::abs(x) >= 0.1) {
    return std::sinh(x) - x;
  }
  const double square = x * x;
  return x * square * (1.0 / 6 + square * (1.0 / 120 + square * (1.0 / 5040 + square / 362880)));
}

// convert mean anomaly M at epoch to true anomaly nu, the angle from periapsis to
// the position in the orbit plane. the intermediate variable depends on the orbit shape:
//   e < 1: eccentric anomaly E, with M = E - e sin(E)
//   e = 1: barker's variable D, with M = D / 2 + D^3 / 6
//   e > 1: hyperbolic eccentric anomaly F, with M = e sinh(F) - F
// M is zero at periapsis for every shape. only the elliptic M is periodic.
double true_anomaly(double eccentricity, double mean_anomaly) {
  if (mean_anomaly == 0) {
    return 0;
  }
  if (eccentricity < 1) {
    // elliptical orbit: solve kepler's equation for E.
    // normalize to [-pi, pi]
    const double mean = std::remainder(mean_anomaly, 2 * std::numbers::pi);
    const double magnitude = std::abs(mean);
    const double upper = std::min(std::numbers::pi, magnitude / (1 - eccentricity));
    const double anomaly = std::copysign(
        increasing_root(
            0,
            upper,
            [=](double x) {
              return (1 - eccentricity) * x + eccentricity * circular_anomaly_residual(x) -
                     magnitude;
            }
        ),
        mean
    );
    // unity's CalcAnomaly used the relation in the other direction:
    //   tan(E/2) = tan(nu/2) * sqrt((1-e) / (1+e))
    // to recover nu, use atan2 with sine and cosine to avoid forming
    // a very large tangent near apoapsis, where the half-angle is close to +/-pi/2.
    return 2 * std::atan2(
                   std::sqrt(1 + eccentricity) * std::sin(anomaly / 2),
                   std::sqrt(1 - eccentricity) * std::cos(anomaly / 2)
               );
  }
  if (eccentricity > 1) {
    // hyperbolic orbit: solve kepler's equation for F. solve for its magnitude,
    // then restore the sign of M; the equation is odd in F.
    const double mean = std::abs(mean_anomaly);
    const double upper = std::min(std::asinh(mean / eccentricity) + 2, mean / (eccentricity - 1));
    const double anomaly = increasing_root(0, upper, [=](double x) {
      return ((eccentricity - 1) / eccentricity) * x + hyperbolic_anomaly_residual(x) -
             mean / eccentricity;
    });
    // the equation for hyperbolic eccentric anomaly is
    //   tanh(F/2) = tan(nu/2) * sqrt((e-1) / (e+1))
    // unity solved for F with asinh to avoid atanh near the edges of its domain:
    //   sinh(F/2) = sin(nu/2) * sqrt((e-1) / (1 + e cos(nu)))
    // that form still loses precision near the asymptotes, where 1 + e cos(nu)
    // approaches zero. here F is already known, so use the first relation to get nu.
    return std::copysign(
        2 * std::atan2(
                std::sqrt(eccentricity + 1) * std::tanh(anomaly / 2), std::sqrt(eccentricity - 1)
            ),
        mean_anomaly
    );
  }
  // parabolic orbit (e == 1): barker's variable is D = tan(nu/2).
  // solve D^3 + 3D = 6M using the hyperbolic triple-angle identity:
  //   D = 2 sinh(asinh(3M) / 3)
  // then recover the true anomaly nu = 2 atan(D).
  const double scaled = 3 * mean_anomaly;
  const double angle =
      std::isfinite(scaled)
          ? std::asinh(scaled)
          : std::copysign(std::log(std::abs(mean_anomaly)) + std::log(6.0), mean_anomaly);
  return 2 * std::atan(2 * std::sinh(angle / 3));
}

} // namespace

OrbitState
state_at_epoch_from_elements(double mu, double epoch, OrbitalElements elements, FrameId frame_id) {
  const auto [momentum, eccentricity, longitude, mean] = elements;
  if (!std::isfinite(mu) || mu <= 0 || !std::isfinite(epoch) || !std::isfinite(momentum) ||
      momentum == 0 || !std::isfinite(eccentricity) || eccentricity < 0 ||
      !std::isfinite(longitude) || !std::isfinite(mean)) {
    throw std::invalid_argument("Invalid orbital elements");
  }

  // all conics use the same state formulas once nu is known. the distance from
  // the center is r = p / (1 + e cos(nu)), where p = h^2 / mu.
  const double anomaly = true_anomaly(eccentricity, mean);
  const double sine = std::sin(anomaly);
  const double cosine = std::cos(anomaly);
  const double half_cosine = std::cos(anomaly / 2);
  const double denominator = (1 - eccentricity) + 2 * eccentricity * half_cosine * half_cosine;
  const double direction = std::copysign(1.0, momentum);
  const double radius = (momentum / mu) * momentum / denominator;
  const double speed_scale = mu / std::abs(momentum);
  // convert from perifocal space by mirroring y for clockwise orbits, then rotating
  // by the longitude of periapsis.
  const Vec2d position = (radius * Vec2d{cosine, direction * sine}).rotate(longitude);
  const Vec2d velocity =
      (speed_scale * Vec2d{-sine, direction * (eccentricity + cosine)}).rotate(longitude);
  if (radius <= 0 || !is_finite(position) || !is_finite(velocity)) {
    throw std::domain_error("Orbital elements exceed the representable domain");
  }
  return {epoch, {position, velocity}, frame_id};
}

OrbitGeometry geometry(double mu, const OrbitState &orbit) {
  const auto [position, velocity] = orbit.state;
  const double radius = position.length();
  if (!(mu > 0) || !std::isfinite(mu) || !(radius > 0) || !std::isfinite(radius) ||
      !is_finite(velocity)) {
    throw std::invalid_argument("Invalid orbital state");
  }

  const double momentum = position.cross(velocity);
  const double alpha = 2 / radius - velocity.dot(velocity) / mu;
  const double p = (momentum / mu) * momentum;
  const Vec2d eccentricity_vector = -(momentum / mu) * velocity.perpendicular() - position / radius;
  const double eccentricity = eccentricity_vector.length();
  const double infinity = std::numeric_limits<double>::infinity();
  return {
      momentum,
      eccentricity,
      p,
      p / (1 + eccentricity),
      alpha > 0 ? (1 + eccentricity) / alpha : infinity,
      alpha > 0 ? 2 * std::numbers::pi / (std::sqrt(mu * alpha) * alpha) : infinity
  };
}

} // namespace flop::orbit
