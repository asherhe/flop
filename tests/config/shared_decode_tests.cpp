#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <yaml-cpp/yaml.h>

#include "flop/config/decode/key_curve.hpp"
#include "flop/config/decode/range.hpp"

TEST_CASE("shared range decoder accepts negative ordered values") {
  using namespace flop::config;
  const auto values = decode_ordered_range(ConfigView(YAML::Load("[-4, -1]"), "fixture.yaml"));
  CHECK(values[0] == -4);
  CHECK(values[1] == -1);
  CHECK_THROWS(decode_ordered_range(ConfigView(YAML::Load("[1, -1]"), "fixture.yaml")));
  CHECK_THROWS(decode_ordered_range(ConfigView(YAML::Load("[1]"), "fixture.yaml")));
}

TEST_CASE("shared curve decoder permits nonnormalized times") {
  using namespace flop::config;
  const auto curve = decode_key_curve<double>(
      ConfigView(YAML::Load("{-2: 0, 2: 4}"), "fixture.yaml"), &ConfigView::as<double>,
      [](double, const ConfigView &) {});
  CHECK(curve.evaluate(0) == Catch::Approx(2));
  CHECK_THROWS(decode_key_curve<double>(ConfigView(YAML::Load("{0: 1, 0.0: 2}"), "fixture.yaml"),
      &ConfigView::as<double>, [](double, const ConfigView &) {}));
}
