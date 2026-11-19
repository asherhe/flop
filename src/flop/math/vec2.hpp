#pragma once

#include <algorithm>
#include <cmath>

namespace flop {

struct Vec2d {
  double x{};
  double y{};

  // exact component-wise equality, not a tolerant compare; use approx where rounding is a concern.
  friend constexpr bool operator==(Vec2d, Vec2d) = default;

  // tolerant component-wise compare. each component passes when
  // |a - b| <= max(rel_tolerance * max(|a|, |b|), abs_floor). tolerances are dimensionless and
  // should be non-negative. a NaN component never passes, and equal infinities do not pass
  // because their difference is NaN.
  bool approx(Vec2d other, double rel_tolerance = 1e-9, double abs_floor = 1e-12) const {
    const auto component_close = [=](double a, double b) {
      const double tolerance =
          std::max(rel_tolerance * std::max(std::abs(a), std::abs(b)), abs_floor);
      return std::abs(a - b) <= tolerance;
    };
    return component_close(x, other.x) && component_close(y, other.y);
  }

  constexpr Vec2d operator-() const { return {-x, -y}; }

  constexpr Vec2d &operator+=(Vec2d other) {
    x += other.x;
    y += other.y;
    return *this;
  }

  constexpr Vec2d &operator-=(Vec2d other) {
    x -= other.x;
    y -= other.y;
    return *this;
  }

  friend constexpr Vec2d operator+(Vec2d lhs, Vec2d rhs) { return lhs += rhs; }

  friend constexpr Vec2d operator-(Vec2d lhs, Vec2d rhs) { return lhs -= rhs; }

  friend constexpr Vec2d operator*(Vec2d value, double scalar) {
    return {value.x * scalar, value.y * scalar};
  }

  friend constexpr Vec2d operator*(double scalar, Vec2d value) { return value * scalar; }

  friend constexpr Vec2d operator/(Vec2d value, double scalar) {
    return {value.x / scalar, value.y / scalar};
  }

  constexpr double dot(const Vec2d &other) const noexcept { return x * other.x + y * other.y; }

  constexpr double cross(const Vec2d &other) const { return x * other.y - y * other.x; }

  constexpr Vec2d perpendicular() const { return {-y, x}; }

  double length() const { return std::hypot(x, y); }

  Vec2d rotate(double angle_rad) const {
    const double sine = std::sin(angle_rad);
    const double cosine = std::cos(angle_rad);
    return {cosine * x - sine * y, sine * x + cosine * y};
  }
};

inline bool is_finite(Vec2d value) {
  return std::isfinite(value.x) && std::isfinite(value.y);
}

} // namespace flop
