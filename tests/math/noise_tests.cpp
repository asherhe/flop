#include <cmath>
#include <limits>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "flop/utils/noise.hpp"

TEST_CASE("seeded gradient noise is bounded and continuous in one to three dimensions") {
  using Catch::Approx;
  CHECK(flop::perlin_noise(0.0, 42) == 0.5);
  CHECK(flop::perlin_noise(-2.0, 3.0, 42) == 0.5);
  CHECK(flop::perlin_noise(-2.0, 3.0, 4.0, 42) == 0.5);
  CHECK(flop::perlin_noise(0.25, 42) == Approx(0.44989947150735293));
  CHECK(flop::perlin_noise(-0.25, 0.4, 42) == Approx(0.429829484375));
  CHECK(flop::perlin_noise(-0.25, 0.4, 1.2, 42) == Approx(0.615345148435));
  CHECK(flop::perlin_noise(-1.0 - 1e-7, 42) == Approx(flop::perlin_noise(-1.0 + 1e-7, 42)).margin(1e-6));
  CHECK(flop::perlin_noise(0.25, 42) == flop::perlin_noise(0.25, 42));
  CHECK(flop::perlin_noise(0.25, 42) != flop::perlin_noise(0.25, 43));
  CHECK(flop::perlin_noise(std::numeric_limits<double>::infinity(), 42) == 0.5);
  for (int i = -30; i <= 30; ++i) {
    const double x = i * 0.13;
    const auto one = flop::perlin_noise(x, 91);
    const auto two = flop::perlin_noise(x, -0.4, 91);
    const auto three = flop::perlin_noise(x, -0.4, 0.8, 91);
    CHECK(one >= 0.0);
    CHECK(one <= 1.0);
    CHECK(two >= 0.0);
    CHECK(two <= 1.0);
    CHECK(three >= 0.0);
    CHECK(three <= 1.0);
  }
}
