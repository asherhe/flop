#pragma once

#include <algorithm>
#include <cmath>

#include "flop/math/vec2.hpp"

namespace flop::orbit::detail {

// keep power-of-two exponents separate until the final result is known.
inline double multiply_divide(double lhs, double rhs, double divisor) {
  int lhs_exponent{};
  int rhs_exponent{};
  int divisor_exponent{};
  const double lhs_fraction = std::frexp(lhs, &lhs_exponent);
  const double rhs_fraction = std::frexp(rhs, &rhs_exponent);
  const double divisor_fraction = std::frexp(divisor, &divisor_exponent);
  return std::scalbn(
      lhs_fraction * rhs_fraction / divisor_fraction, lhs_exponent + rhs_exponent - divisor_exponent
  );
}

// sqrt(mu / radius), including when the squared speed cannot be represented.
inline double circular_speed(double mu, double radius) {
  int mu_exponent{};
  int radius_exponent{};
  double mu_fraction = std::frexp(mu, &mu_exponent);
  const double radius_fraction = std::frexp(radius, &radius_exponent);
  int exponent = mu_exponent - radius_exponent;
  if (exponent % 2 != 0) {
    mu_fraction *= 2;
    --exponent;
  }
  return std::scalbn(std::sqrt(mu_fraction / radius_fraction), exponent / 2);
}

// |velocity|^2 * radius / mu, without squaring the dimensional velocity or
// rounding through a square root when determining whether the conic is bound.
inline double relative_speed_squared(Vec2d velocity, double radius, double mu) {
  int velocity_exponent{};
  int radius_exponent{};
  int mu_exponent{};
  std::frexp(std::max(std::abs(velocity.x), std::abs(velocity.y)), &velocity_exponent);
  const Vec2d scaled{
      std::scalbn(velocity.x, -velocity_exponent),
      std::scalbn(velocity.y, -velocity_exponent),
  };
  const double radius_fraction = std::frexp(radius, &radius_exponent);
  const double mu_fraction = std::frexp(mu, &mu_exponent);
  return std::scalbn(
      scaled.dot(scaled) * radius_fraction / mu_fraction,
      2 * velocity_exponent + radius_exponent - mu_exponent
  );
}

} // namespace flop::orbit::detail
