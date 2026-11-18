#include "flop/orbit/state.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <numeric>
#include <stdexcept>

#include "flop/orbit/scale.hpp"

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

// the intermediate anomaly at epoch depends on the orbit shape:
//   e < 1: eccentric anomaly E, with M = E - e sin(E)
//   e = 1: barker's variable D, with M = D / 2 + D^3 / 6
//   e > 1: hyperbolic eccentric anomaly F, with M = e sinh(F) - F
// M is zero at periapsis for every shape. only the elliptic M is periodic.
double elliptic_true_anomaly(double eccentricity, double mean_anomaly) {
  // elliptical orbit: solve kepler's equation for E and normalize M to [-pi, pi].
  const double mean = std::remainder(mean_anomaly, 2 * std::numbers::pi);
  const double magnitude = std::abs(mean);
  const double upper = std::min(std::numbers::pi, magnitude / (1 - eccentricity));
  const double anomaly = std::copysign(
      increasing_root(
          0,
          upper,
          [=](double x) {
            return (1 - eccentricity) * x + eccentricity * circular_anomaly_residual(x) - magnitude;
          }
      ),
      mean
  );
  // unity's CalcAnomaly used the relation in the other direction:
  //   tan(E/2) = tan(nu/2) * sqrt((1-e) / (1+e))
  // to recover the true anomaly nu, use atan2 with sine and cosine to avoid forming
  // a very large tangent near apoapsis, where the half-angle is close to +/-pi/2.
  return 2 * std::atan2(
                 std::sqrt(1 + eccentricity) * std::sin(anomaly / 2),
                 std::sqrt(1 - eccentricity) * std::cos(anomaly / 2)
             );
}

StateVectors hyperbolic_state(double eccentricity, double mean_anomaly, double p, double speed) {
  // solve kepler's equation for the magnitude of F, then restore the sign of M.
  const double mean = std::abs(mean_anomaly) / eccentricity;
  const double e_minus_one = eccentricity - 1;
  const double upper = std::min(std::asinh(mean) + 2, std::abs(mean_anomaly) / e_minus_one);
  const double e_minus_one_over_e = e_minus_one / eccentricity;
  const double anomaly = increasing_root(0, upper, [=](double x) {
    return e_minus_one_over_e * x + hyperbolic_anomaly_residual(x) - mean;
  });

  // unity's CalcAnomaly used the relation
  //   tanh(F/2) = tan(nu/2) * sqrt((e-1) / (e+1)).
  // near the asymptote, reconstructing radius from nu loses precision because
  // 1 + e cos(nu) tends to zero. use F directly in the cartesian formulas.
  // for large F, recover sinh(F) from kepler's equation without exponentiating F.
  const double sine =
      std::copysign(anomaly < 1 ? std::sinh(anomaly) : mean + anomaly / eccentricity, mean_anomaly);
  const double cosine = std::hypot(1.0, sine);
  const double cosine_minus_one = sine * (sine / (cosine + 1));
  const double inverse_e = 1 / eccentricity;
  const double shape_over_e = std::sqrt(e_minus_one_over_e * (1 + inverse_e));
  const double periapsis = p / (eccentricity + 1);
  const double radius_factor = e_minus_one_over_e + cosine_minus_one;
  return {
      {periapsis - detail::multiply_divide(periapsis, cosine_minus_one, e_minus_one),
       detail::multiply_divide(p, sine, eccentricity * shape_over_e)},
      {-speed * (shape_over_e * (sine / radius_factor)),
       detail::multiply_divide(speed, e_minus_one * (1 + inverse_e), radius_factor / cosine)},
  };
}

StateVectors parabolic_state(double mean_anomaly, double p, double speed) {
  // barker's variable is D = tan(nu/2). use it directly in the state formulas.
  // solve D^3 + 3D = 6M using the hyperbolic triple-angle identity:
  //   D = 2 sinh(asinh(3M) / 3)
  const double scaled = 3 * mean_anomaly;
  const double angle =
      std::isfinite(scaled)
          ? std::asinh(scaled)
          : std::copysign(std::log(std::abs(mean_anomaly)) + std::log(6.0), mean_anomaly);
  const double d = 2 * std::sinh(angle / 3);
  const double square = d * d;
  return {{p * ((1 - square) / 2), p * d}, speed * (Vec2d{-d, 1} / ((1 + square) / 2))};
}

StateVectors elliptic_state(double eccentricity, double mean_anomaly, double p, double speed) {
  const double anomaly = elliptic_true_anomaly(eccentricity, mean_anomaly);
  const double sine = std::sin(anomaly);
  const double cosine = std::cos(anomaly);
  const double half_cosine = std::cos(anomaly / 2);
  const double denominator = (1 - eccentricity) + 2 * eccentricity * half_cosine * half_cosine;
  const double radius = p / denominator;
  return {radius * Vec2d{cosine, sine}, speed * Vec2d{-sine, eccentricity + cosine}};
}

} // namespace

OrbitState state_at_epoch_from_elements(
    double mu, double epoch, OrbitalElements elements, FrameId frame_id
) {
  const auto [momentum, eccentricity, longitude, mean] = elements;
  if (!std::isfinite(mu) || mu <= 0 || !std::isfinite(epoch) || !std::isfinite(momentum) ||
      momentum == 0 || !std::isfinite(eccentricity) || eccentricity < 0 ||
      !std::isfinite(longitude) || !std::isfinite(mean)) {
    throw std::invalid_argument("Invalid orbital elements");
  }

  const double p = detail::multiply_divide(momentum, momentum, mu);
  const double speed = mu / std::abs(momentum);
  if (!(p > 0) || !std::isfinite(p) || !std::isfinite(speed)) {
    throw std::domain_error("Orbital elements exceed the representable domain");
  }
  StateVectors perifocal;
  if (eccentricity < 1) {
    perifocal = elliptic_state(eccentricity, mean, p, speed);
  } else if (eccentricity > 1) {
    perifocal = hyperbolic_state(eccentricity, mean, p, speed);
  } else {
    perifocal = parabolic_state(mean, p, speed);
  }

  const double direction = std::copysign(1.0, momentum);
  // convert from perifocal space by mirroring y for clockwise orbits, then rotating
  // by the longitude of periapsis.
  const Vec2d position =
      Vec2d{perifocal.position.x, direction * perifocal.position.y}.rotate(longitude);
  const Vec2d velocity =
      Vec2d{perifocal.velocity.x, direction * perifocal.velocity.y}.rotate(longitude);
  if ((position.x == 0 && position.y == 0) || !is_finite(position) || !is_finite(velocity)) {
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

  const Vec2d radial = position / radius;
  const double momentum = radius * radial.cross(velocity);
  if (!std::isfinite(momentum)) {
    throw std::domain_error("Orbital geometry exceeds the representable domain");
  }
  const double alpha = 2 - detail::relative_speed_squared(velocity, radius, mu);
  const double p = detail::multiply_divide(momentum, momentum, mu);
  const Vec2d eccentricity_vector =
      Vec2d{
          detail::multiply_divide(momentum, velocity.y, mu),
          detail::multiply_divide(-momentum, velocity.x, mu)
      } -
      radial;
  const double eccentricity = eccentricity_vector.length();
  const double infinity = std::numeric_limits<double>::infinity();
  if (!std::isfinite(p) || !std::isfinite(eccentricity)) {
    throw std::domain_error("Orbital geometry exceeds the representable domain");
  }
  const double periapsis = p / (1 + eccentricity);
  if (alpha <= 0) {
    return {momentum, eccentricity, p, periapsis, infinity, infinity};
  }
  const double apoapsis = detail::multiply_divide(radius, 1 + eccentricity, alpha);
  const double period = detail::multiply_divide(
      radius, 2 * std::numbers::pi / (std::sqrt(alpha) * alpha), detail::circular_speed(mu, radius)
  );
  if (!std::isfinite(apoapsis) || !std::isfinite(period) || period <= 0) {
    throw std::domain_error("Orbital geometry exceeds the representable domain");
  }
  return {momentum, eccentricity, p, periapsis, apoapsis, period};
}

} // namespace flop::orbit
