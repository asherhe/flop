#include <limits>
#include <stdexcept>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "flop/world/universe.hpp"

namespace {

using flop::world::Universe;

} // namespace

TEST_CASE("Universe owns finite UT and a positive gravitational constant") {
  Universe universe;
  CHECK(universe.ut_sec() == 0);
  CHECK(universe.gravitational_constant() == 6.67430e-11);

  const Universe other(-25, 7e-11);
  CHECK(other.ut_sec() == -25);
  CHECK(other.gravitational_constant() == 7e-11);
}

TEST_CASE("Universe rejects invalid initial time and gravitational constant") {
  const double infinity = std::numeric_limits<double>::infinity();
  const double nan = std::numeric_limits<double>::quiet_NaN();
  CHECK_THROWS_AS(Universe(infinity), std::invalid_argument);
  CHECK_THROWS_AS(Universe(nan), std::invalid_argument);
  CHECK_THROWS_AS(Universe(0, 0), std::invalid_argument);
  CHECK_THROWS_AS(Universe(0, -1), std::invalid_argument);
  CHECK_THROWS_AS(Universe(0, infinity), std::invalid_argument);
  CHECK_THROWS_AS(Universe(0, nan), std::invalid_argument);
}

TEST_CASE("Universe ticks strictly forward in supplied simulation seconds") {
  Universe universe(100);
  universe.tick(0.02);
  CHECK(universe.ut_sec() > 100);
  universe.tick(20);
  CHECK(universe.ut_sec() == Catch::Approx(120.02));
}

TEST_CASE("Universe rejects invalid ticks without changing UT") {
  Universe universe(1);
  const double infinity = std::numeric_limits<double>::infinity();
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double too_small = std::numeric_limits<double>::denorm_min();
  CHECK_THROWS_AS(universe.tick(0), std::invalid_argument);
  CHECK_THROWS_AS(universe.tick(-1), std::invalid_argument);
  CHECK_THROWS_AS(universe.tick(infinity), std::invalid_argument);
  CHECK_THROWS_AS(universe.tick(nan), std::invalid_argument);
  CHECK_THROWS_AS(universe.tick(too_small), std::invalid_argument);
  CHECK(universe.ut_sec() == 1);

  Universe near_limit(std::numeric_limits<double>::max());
  CHECK_THROWS_AS(near_limit.tick(std::numeric_limits<double>::max()), std::invalid_argument);
  CHECK(near_limit.ut_sec() == std::numeric_limits<double>::max());
}
