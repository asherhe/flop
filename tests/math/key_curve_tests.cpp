#include <limits>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "flop/utils/color.hpp"
#include "flop/utils/key_curve.hpp"

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
  const flop::KeyCurve<flop::Color> curve({{0, {1, 0, 0}}, {1, {0, 0, 1}}});
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

TEST_CASE("smooth key curves preserve the shape of uneven data") {
  const flop::KeyCurve<double, flop::SmoothInterpolation> curve(
      {{0, 0}, {0.02, 0.35}, {0.33, 0.96}, {0.76, 0.66}, {1, 0}}
  );
  const auto keys = curve.keys();
  for (std::size_t i = 0; i + 1 < keys.size(); ++i) {
    for (int step = 0; step <= 20; ++step) {
      const double time = keys[i].time + (keys[i + 1].time - keys[i].time) * step / 20.0;
      const double value = curve.evaluate(time);
      CHECK(value >= std::min(keys[i].value, keys[i + 1].value) - 1e-12);
      CHECK(value <= std::max(keys[i].value, keys[i + 1].value) + 1e-12);
    }
  }
  CHECK(curve.evaluate(-1) == 0);
  CHECK(curve.evaluate(0.33) == 0.96);
  CHECK(curve.evaluate(2) == 0);
  constexpr double epsilon = 1e-6;
  const double slope_before = (curve.evaluate(0.33) - curve.evaluate(0.33 - epsilon)) / epsilon;
  const double slope_after = (curve.evaluate(0.33 + epsilon) - curve.evaluate(0.33)) / epsilon;
  CHECK(slope_before == Catch::Approx(slope_after).margin(0.0001));
}

TEST_CASE("smooth curves handle constant, two-key, and flat segments") {
  const flop::KeyCurve<double, flop::SmoothInterpolation> one({{0, 3}});
  const flop::KeyCurve<double, flop::SmoothInterpolation> two({{0, 2}, {2, 6}});
  const flop::KeyCurve<double, flop::SmoothInterpolation> flat({{0, 1}, {1, 1}, {2, 3}});
  CHECK(one.evaluate(3) == 3);
  CHECK(two.evaluate(1) == Catch::Approx(4));
  CHECK(flat.evaluate(0.5) == Catch::Approx(1));
}
