#include <catch2/catch_test_macros.hpp>

#include "flop/math/vec2.hpp"

TEST_CASE("Dot product uses both vector components", "[math]") {
  const flop::Vec2d a{2.0, 3.0};
  const flop::Vec2d b{5.0, 7.0};
  REQUIRE(a.dot(b) == 31.0);
}
