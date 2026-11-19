#pragma once

#include <cmath>

namespace flop {

struct Vec2d {
  double x{};
  double y{};

  // exact component-wise equality, not a tolerant compare; use Approx or an epsilon where rounding
  // is a concern.
  friend constexpr bool operator==(Vec2d, Vec2d) = default;

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

  Vec2d rotate(double angle_radians) const {
    const double sine = std::sin(angle_radians);
    const double cosine = std::cos(angle_radians);
    return {cosine * x - sine * y, sine * x + cosine * y};
  }
};

inline bool is_finite(Vec2d value) {
  return std::isfinite(value.x) && std::isfinite(value.y);
}

} // namespace flop
