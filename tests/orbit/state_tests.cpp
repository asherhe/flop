#include <catch2/catch_test_macros.hpp>

#include "flop/orbit/state.hpp"

using flop::Vec2d;
using flop::orbit::StateVectors;

TEST_CASE("StateVectors approx accepts equal states", "[orbit]") {
  const StateVectors state{{4.0, 0.0}, {-5.0, 2.0}};
  CHECK(state.approx(state));
}

TEST_CASE("StateVectors approx checks position and velocity together", "[orbit]") {
  const StateVectors expected{{0.0, 4.0}, {-5.0, 0.0}};
  CHECK(expected.approx({{0.0, 4.0 + 1e-13}, {-5.0, 1e-13}}));
  CHECK_FALSE(expected.approx({{0.0, 4.001}, {-5.0, 0.0}}));
  CHECK_FALSE(expected.approx({{0.0, 4.0}, {-5.001, 0.0}}));
  CHECK_FALSE(expected.approx({{0.0, 4.0}, {-5.0, 1e-6}}));
}

TEST_CASE("StateVectors approx takes caller tolerances in place of the defaults", "[orbit]") {
  const StateVectors expected{{1e6, 0.0}, {0.0, 1.0}};
  const StateVectors drifted{{1e6 + 1e-2, 0.0}, {0.0, 1.0}};
  CHECK_FALSE(expected.approx(drifted));
  CHECK(expected.approx(drifted, 1e-7));
}

TEST_CASE("StateVectors exact equality is unchanged by approx", "[orbit]") {
  const StateVectors state{{1.0, 2.0}, {3.0, 4.0}};
  const StateVectors nearby{{1.0 + 1e-13, 2.0}, {3.0, 4.0}};
  CHECK(state.approx(nearby));
  CHECK(state != nearby);
}
