#include <limits>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "flop/craft/plugins/particle_rgb.hpp"
#include "flop/math/key_curve.hpp"

TEST_CASE("key curves sort keys and interpolate with clamped endpoints") {
  const flop::KeyCurve<double> curve({{1, 4}, {0, 0}, {0.5, 1}});
  CHECK(curve.evaluate(-1) == 0);
  CHECK(curve.evaluate(0.5) == 1);
  CHECK(curve.evaluate(0.25) == Catch::Approx(0.5));
  CHECK(curve.evaluate(2) == 4);
  CHECK(flop::KeyCurve<double>({{0.3, 7}}).evaluate(0.8) == 7);
  CHECK_THROWS(flop::KeyCurve<double>({{0, 1}, {0, 2}}));
  CHECK_THROWS(flop::KeyCurve<double>({{std::numeric_limits<double>::infinity(), 1}}));
  CHECK_THROWS(flop::KeyCurve<double>().evaluate(0));
  CHECK_THROWS(curve.evaluate(std::numeric_limits<double>::quiet_NaN()));
}

TEST_CASE("key curves interpolate particle color components") {
  using flop::craft::plugin::ParticleRgb;
  const flop::KeyCurve<ParticleRgb> curve({{0, {1, 0, 0}}, {1, {0, 0, 1}}});
  const auto color = curve.evaluate(0.25);
  CHECK(color.r == Catch::Approx(0.75));
  CHECK(color.g == 0);
  CHECK(color.b == Catch::Approx(0.25));
}

TEST_CASE("key curves accept stateful interpolation policies") {
  struct ConstantPolicy {
    double value;
    double operator()(std::span<const flop::CurveKey<double>>, double) const { return value; }
  };
  const flop::KeyCurve<double, ConstantPolicy> curve({{0, 0}, {1, 1}}, {0.75});
  CHECK(curve.evaluate(0.5) == 0.75);
  CHECK(curve.evaluate(0) == 0);
}
