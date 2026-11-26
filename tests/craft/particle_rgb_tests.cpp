#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "flop/craft/plugins/particle_rgb.hpp"

TEST_CASE("particle rgb arithmetic preserves component values") {
  using flop::craft::plugin::ParticleRgb;
  const auto result = ParticleRgb{0.2, 0.4, 0.6} * 0.25 + ParticleRgb{0.8, 0.4, 0.0} * 0.75;
  CHECK(result.r == Catch::Approx(0.65));
  CHECK(result.g == Catch::Approx(0.4));
  CHECK(result.b == Catch::Approx(0.15));
}
