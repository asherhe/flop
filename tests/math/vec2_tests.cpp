#include <limits>
#include <numbers>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "flop/math/vec2.hpp"

namespace {

using Catch::Matchers::WithinAbs;
using flop::Vec2d;

void check_components(Vec2d actual, Vec2d expected) {
  CHECK(actual.x == expected.x);
  CHECK(actual.y == expected.y);
}

} // namespace

TEST_CASE("A default-constructed vector has zero components", "[math]") {
  const Vec2d value;
  check_components(value, {0.0, 0.0});
}

TEST_CASE("Vector addition and subtraction operate componentwise", "[math]") {
  const Vec2d a{2.0, -4.0};
  const Vec2d b{-6.0, 1.0};
  check_components(a + b, {-4.0, -3.0});
  check_components(a - b, {8.0, -5.0});
  check_components(-a, {-2.0, 4.0});

  Vec2d sum = a;
  CHECK(&(sum += b) == &sum);
  check_components(sum, {-4.0, -3.0});

  Vec2d difference = a;
  CHECK(&(difference -= b) == &difference);
  check_components(difference, {8.0, -5.0});
}

TEST_CASE("Scalar arithmetic scales both vector components", "[math]") {
  const Vec2d value{2.0, -4.0};
  check_components(value * -3.0, {-6.0, 12.0});
  check_components(-3.0 * value, {-6.0, 12.0});
  check_components(value / 2.0, {1.0, -2.0});
}

TEST_CASE("Dot product uses both vector components", "[math]") {
  constexpr Vec2d A{2.0, 3.0};
  constexpr Vec2d B{5.0, 7.0};
  STATIC_REQUIRE(A.dot(B) == 31.0);
}

TEST_CASE("Cross product gives the signed determinant", "[math]") {
  const Vec2d a{2.0, 3.0};
  const Vec2d b{5.0, 7.0};
  CHECK(a.cross(b) == -1.0);
  CHECK(b.cross(a) == 1.0);
  CHECK(a.cross(a) == 0.0);
}

TEST_CASE("Perpendicular turns a vector counterclockwise by a right angle", "[math]") {
  check_components(Vec2d{2.0, -3.0}.perpendicular(), {3.0, 2.0});
}

TEST_CASE("Vector length is the Euclidean norm", "[math]") {
  const Vec2d value{-3.0, 4.0};
  const Vec2d large{3e200, 4e200};
  CHECK(value.length() == 5.0);
  CHECK(Vec2d{}.length() == 0.0);
  CHECK_THAT(large.length(), WithinAbs(5e200, 1e187));
}

TEST_CASE("Both vector components must be finite", "[math]") {
  const double infinity = std::numeric_limits<double>::infinity();
  const double nan = std::numeric_limits<double>::quiet_NaN();
  CHECK(flop::is_finite({-2.0, 0.0}));
  CHECK_FALSE(flop::is_finite({infinity, 0.0}));
  CHECK_FALSE(flop::is_finite({0.0, -infinity}));
  CHECK_FALSE(flop::is_finite({nan, 0.0}));
  CHECK_FALSE(flop::is_finite({0.0, nan}));
}

TEST_CASE("Vector rotation follows the sign of the angle in radians", "[math]") {
  constexpr double RIGHT_ANGLE = std::numbers::pi / 2.0;
  constexpr double TOLERANCE = 1e-12;
  const Vec2d value{2.0, 3.0};
  check_components(value.rotate(0.0), value);

  const auto counterclockwise = value.rotate(RIGHT_ANGLE);
  CHECK_THAT(counterclockwise.x, WithinAbs(-3.0, TOLERANCE));
  CHECK_THAT(counterclockwise.y, WithinAbs(2.0, TOLERANCE));

  const auto clockwise = value.rotate(-RIGHT_ANGLE);
  CHECK_THAT(clockwise.x, WithinAbs(3.0, TOLERANCE));
  CHECK_THAT(clockwise.y, WithinAbs(-2.0, TOLERANCE));
}
