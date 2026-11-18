#include "flop/orbit/propagators/universal.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <stdexcept>

#include "flop/orbit/scale.hpp"

namespace flop::orbit {
namespace {

// values of the stumpff functions C(z) and S(z) at one argument z
struct Stumpff {
  double c;
  double s;
};

Stumpff stumpff(double z) {
  // use a series near z = 0 to avoid floating-point cancellation in the stumpff functions.
  if (std::abs(z) < 0.1) {
    const double c =
        0.5 -
        z * (1.0 / 24 - z * (1.0 / 720 - z * (1.0 / 40320 - z * (1.0 / 3628800 - z / 479001600))));
    const double s =
        1.0 / 6 -
        z * (1.0 / 120 -
             z * (1.0 / 5040 - z * (1.0 / 362880 - z * (1.0 / 39916800 - z / 6227020800.0))));
    return {c, s};
  }

  const double root = std::sqrt(std::abs(z));
  if (z > 0) {
    const double half_sine = std::sin(root / 2);
    return {2 * half_sine * half_sine / z, (1 - std::sin(root) / root) / z};
  }
  const double half_sine = std::sinh(root / 2);
  return {2 * half_sine * half_sine / -z, (std::sinh(root) / root - 1) / -z};
}

// coefficients of the universal kepler equation in normalized units for one time query
struct KeplerEquation {
  double alpha;
  double radial_speed;
  double elapsed;

  // residual of universal kepler's equation at the universal anomaly chi
  double residual(double chi) const {
    const double square = chi * chi;
    const auto [c, s] = stumpff(alpha * square);
    return radial_speed * square * c + (1 - alpha) * square * chi * s + chi - elapsed;
  }
};

// get the value of the universal anomaly chi for the elapsed time.
// unity's AnomCoeff expressed the dimensional universal anomaly as the change in E,
// D, or F multiplied by sqrt(a), sqrt(p), or sqrt(-a), respectively, where a is the
// signed semimajor axis and p is the semilatus rectum. this solver uses chi normalized
// by sqrt(radius at epoch) and solves for it directly for every conic.
double solve_anomaly(const KeplerEquation &equation) {
  double lower = 0;
  double upper = std::min(equation.elapsed, 1.0);
  // on an escape trajectory the solution grows logarithmically with time.
  if (equation.alpha < -1) {
    upper = std::max(upper, std::log1p(equation.elapsed) / std::sqrt(-equation.alpha));
  }
  for (int expansion = 0; expansion < 1024 && equation.residual(upper) < 0; ++expansion) {
    lower = upper;
    upper *= 2;
  }
  if (!std::isfinite(upper) || equation.residual(upper) < 0) {
    throw std::runtime_error("Could not bracket universal anomaly");
  }
  for (int iteration = 0; iteration < 128; ++iteration) {
    const double middle = std::midpoint(lower, upper);
    const double residual = equation.residual(middle);
    if (residual == 0 || middle == lower || middle == upper) {
      return middle;
    }
    // a hyperbolic trial can overflow beyond the root, including inf - inf.
    if (residual < 0) {
      lower = middle;
    } else {
      upper = middle;
    }
  }
  throw std::runtime_error("Universal anomaly did not converge");
}

} // namespace

UniversalPropagator::UniversalPropagator(double mu, const OrbitState &orbit) :
    orbit_(orbit), radius_at_epoch_(orbit.state.position.length()) {
  const auto [position, velocity] = orbit.state;
  if (!std::isfinite(mu) || mu <= 0 || !is_finite(position) || radius_at_epoch_ <= 0 ||
      !is_finite(velocity) || !std::isfinite(orbit.epoch)) {
    throw std::invalid_argument("Invalid universal propagation seed");
  }

  if (!std::isfinite(radius_at_epoch_)) {
    throw std::domain_error("Propagation seed exceeds the representable domain");
  }
  speed_scale_ = detail::circular_speed(mu, radius_at_epoch_);
  time_scale_ = radius_at_epoch_ / speed_scale_;
  if (!std::isfinite(time_scale_) || time_scale_ <= 0) {
    throw std::domain_error("Propagation seed exceeds the representable domain");
  }

  initial_position_ = position / radius_at_epoch_;
  initial_velocity_ = velocity / speed_scale_;
  alpha_ = 2 - detail::relative_speed_squared(velocity, radius_at_epoch_, mu);
  radial_speed_ = initial_position_.dot(initial_velocity_);
  if (!std::isfinite(alpha_)) {
    throw std::domain_error("Propagation seed exceeds the representable domain");
  }

  if (alpha_ > 0) {
    const double orbital_scale = (time_scale_ / std::sqrt(alpha_)) / alpha_;
    const double period = (2 * std::numbers::pi) * orbital_scale;
    if (std::isfinite(period) && period > 0) {
      // the low part of 2*pi, omitted by the nearest double. combine it with the
      // rounding error of the multiply to recover the period lost per revolution.
      constexpr double TURN_TAIL = 2.4492935982947064e-16;
      period_ = period;
      period_tail_ =
          std::fma(2 * std::numbers::pi, orbital_scale, -period) + TURN_TAIL * orbital_scale;
    }
  }
}

std::optional<UniversalPropagator::PositionSample> UniversalPropagator::sample_position(
    double ut
) const {
  if (!std::isfinite(ut)) {
    throw std::invalid_argument("Invalid propagation time");
  }

  const double time_since_epoch = ut - orbit_.epoch;
  double phase_time = time_since_epoch;
  if (period_ > 0) {
    phase_time = std::remainder(time_since_epoch, period_);
    const double revolutions = std::round(time_since_epoch / period_);
    if (std::abs(revolutions) < 0x1p52) {
      phase_time = std::fma(-revolutions, period_tail_, phase_time);
    }
  }
  const double elapsed = phase_time / time_scale_;
  if (!std::isfinite(elapsed)) {
    throw std::domain_error("Propagation interval exceeds the representable domain");
  }
  if (elapsed == 0) {
    return std::nullopt;
  }

  const double direction = std::copysign(1.0, elapsed);
  const double chi =
      direction * solve_anomaly({alpha_, direction * radial_speed_, std::abs(elapsed)});
  const double square = chi * chi;
  const double z = alpha_ * square;
  const auto [c, s] = stumpff(z);
  const double square_c = square * c;
  const double z_s = z * s;
  // substituting kepler's equation for elapsed avoids cancellation in g.
  const double g = radial_speed_ * square * c + chi * (1 - z_s);
  const Vec2d position = (1 - square_c) * initial_position_ + g * initial_velocity_;
  if ((position.x == 0 && position.y == 0) || !is_finite(position)) {
    throw std::domain_error("Propagation reached a singular or unrepresentable state");
  }
  return PositionSample{position, chi, square_c, z_s};
}

Vec2d UniversalPropagator::position_from_sample(const PositionSample &sample) const {
  const Vec2d position = radius_at_epoch_ * sample.normalized_position;
  if (!is_finite(position)) {
    throw std::domain_error("Propagation reached a singular or unrepresentable state");
  }
  return position;
}

Vec2d UniversalPropagator::velocity_from_sample(const PositionSample &sample) const {
  // velocity uses the propagated radius in normalized units.
  const double radius = sample.normalized_position.length();
  const double f_dot = sample.chi * (sample.z_s - 1) / radius;
  const double g_dot = 1 - sample.square_c / radius;
  const Vec2d velocity = speed_scale_ * (f_dot * initial_position_ + g_dot * initial_velocity_);
  if (!is_finite(velocity)) {
    throw std::domain_error("Propagation reached a singular or unrepresentable state");
  }
  return velocity;
}

Vec2d UniversalPropagator::position_at(double ut) const {
  const auto sample = sample_position(ut);
  return sample ? position_from_sample(*sample) : orbit_.state.position;
}

Vec2d UniversalPropagator::velocity_at(double ut) const {
  const auto sample = sample_position(ut);
  return sample ? velocity_from_sample(*sample) : orbit_.state.velocity;
}

StateVectors UniversalPropagator::state_at(double ut) const {
  const auto sample = sample_position(ut);
  return sample ? StateVectors{position_from_sample(*sample), velocity_from_sample(*sample)}
                : orbit_.state;
}

} // namespace flop::orbit
