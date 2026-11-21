#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "flop/world/universe.hpp"
#include "flop/world/universe_context.hpp"

namespace {
using Catch::Approx;
using Catch::Matchers::ContainsSubstring;
using flop::world::BodyConfig;
using flop::world::BodyOrbitConfig;
using flop::world::Universe;

const BodyConfig ROOT{"root", 10, 1, 100, {}};
const BodyConfig CHILD{"child", 2, 1, 50, BodyOrbitConfig{"root", 4, 0, 0, 0, 100}};
} // namespace

TEST_CASE("Universe owns finite UT, gravity, and a nonempty body hierarchy") {
  const Universe universe({ROOT});
  CHECK(universe.ut_sec() == 0);
  CHECK(universe.gravitational_constant() == 6.67430e-11);
  REQUIRE(universe.bodies().size() == 1);
  CHECK(universe.body("root").name() == "root");
  CHECK(universe.body("root").mass_kg() == Approx(100 / universe.gravitational_constant()));
  CHECK(universe.reference_frames().frame(universe.frame_id("root")).name == "root");

  const Universe other({ROOT}, -25, 7e-11);
  CHECK(other.ut_sec() == -25);
  CHECK(other.body("root").mass_kg() == Approx(100 / 7e-11));
  CHECK(
      other.body("root").gravitational_parameter() ==
      universe.body("root").gravitational_parameter()
  );
}

TEST_CASE("Universe initializes and owns the shared resource registry") {
  using flop::craft::ResourceFlow;
  std::vector<flop::craft::ResourceTypeConfig> resource_configs{
      {"NTO", "N2O4", 1.0, ResourceFlow::stage}
  };
  const Universe universe({ROOT}, resource_configs);
  resource_configs.front().name = "changed";

  CHECK(universe.resources().at("NTO").name == "N2O4");
  CHECK(universe.resources().at("NTO").kg_per_unit == 1.0);
  CHECK(universe.resources().at("NTO").flow == ResourceFlow::stage);
  CHECK(Universe({ROOT}).resources().resources().empty());
  CHECK_THROWS_WITH(
      Universe({ROOT}, {{"NTO", "N2O4", 0, ResourceFlow::stage}}),
      ContainsSubstring("Invalid resource type")
  );
}

TEST_CASE("Universe satisfies the UniverseContext interface") {
  using flop::craft::ResourceFlow;
  using flop::world::UniverseContext;
  const Universe universe({ROOT}, {{"NTO", "N2O4", 1.0, ResourceFlow::stage}});
  const UniverseContext &context = universe;
  CHECK(context.ut_sec() == universe.ut_sec());
  CHECK(context.resources().at("NTO").name == "N2O4");
  CHECK(&context.reference_frames() == &universe.reference_frames());
  CHECK(context.reference_frames().frame(universe.frame_id("root")).name == "root");
}

TEST_CASE("Universe resolves names independently of input order") {
  const Universe first({ROOT, CHILD});
  const Universe reversed({CHILD, ROOT});
  CHECK(first.body("child").name() == reversed.body("child").name());
  CHECK(first.reference_frames().frame(first.frame_id("child")).parent == first.frame_id("root"));
  CHECK(
      reversed.reference_frames().frame(reversed.frame_id("child")).parent ==
      reversed.frame_id("root")
  );
  CHECK(first.frame_id("child") != reversed.frame_id("child"));
  CHECK(
      first.body_state("child", 100).position.x ==
      Approx(reversed.body_state("child", 100).position.x)
  );
  CHECK_THROWS_WITH(first.body("missing"), ContainsSubstring("Unknown celestial body: missing"));
}

TEST_CASE("Universe body motion queries use the supplied UT") {
  const Universe universe({CHILD, ROOT});
  const double quarter_turn = 100 + 4 * std::numbers::pi / 10;
  const auto root = universe.body_state("root", quarter_turn);
  CHECK(root.position.length() == 0);
  CHECK(root.velocity.length() == 0);
  const auto child = universe.body_state("child", quarter_turn);
  CHECK(child.approx({{0, 4}, {-5, 0}}));
  CHECK(universe.ut_sec() == 0);
  CHECK_THROWS_AS(
      universe.body_state("child", std::numeric_limits<double>::infinity()), std::invalid_argument
  );
}

TEST_CASE("Universe rejects invalid body identities and topology") {
  CHECK_THROWS_WITH(Universe({}), ContainsSubstring("exactly one root"));
  auto child = CHILD;
  SECTION("duplicate names") {
    child.name = "root";
  }
  SECTION("empty name") {
    child.name.clear();
  }
  SECTION("unknown parent") {
    child.orbit->parent_name = "missing";
  }
  SECTION("self cycle") {
    child.orbit->parent_name = "child";
  }
  SECTION("second root") {
    child.orbit.reset();
  }
  CHECK_THROWS_AS(Universe({ROOT, child}), std::invalid_argument);
}

TEST_CASE("Universe reports missing parents and rejects multi-body cycles") {
  auto orphan = CHILD;
  orphan.orbit->parent_name = "missing";
  CHECK_THROWS_WITH(
      Universe({ROOT, orphan}),
      ContainsSubstring("Unknown parent celestial body 'missing' for 'child'")
  );

  auto first = CHILD;
  first.name = "first";
  first.orbit->parent_name = "second";
  auto second = CHILD;
  second.name = "second";
  second.orbit->parent_name = "first";
  CHECK_THROWS_WITH(Universe({ROOT, first, second}), ContainsSubstring("Cyclic frame hierarchy"));
}

TEST_CASE("Universe rejects invalid body physics and orbit data") {
  auto child = CHILD;
  SECTION("invalid physical value") {
    child.radius_meters = 0;
  }
  SECTION("unbound orbit") {
    child.orbit->eccentricity = 1;
  }
  SECTION("unrepresentable orbital seed") {
    child.orbit->semimajor_axis_meters = 1e300;
  }
  CHECK_THROWS(Universe({ROOT, child}));
}

TEST_CASE("Universe rejects invalid initial time and gravitational constant") {
  const double infinity = std::numeric_limits<double>::infinity();
  const double nan = std::numeric_limits<double>::quiet_NaN();
  CHECK_THROWS_AS(Universe({ROOT}, infinity), std::invalid_argument);
  CHECK_THROWS_AS(Universe({ROOT}, nan), std::invalid_argument);
  CHECK_THROWS_AS(Universe({ROOT}, 0, 0), std::invalid_argument);
  CHECK_THROWS_AS(Universe({ROOT}, 0, -1), std::invalid_argument);
  CHECK_THROWS_AS(Universe({ROOT}, 0, infinity), std::invalid_argument);
  CHECK_THROWS_AS(Universe({ROOT}, 0, nan), std::invalid_argument);
}

TEST_CASE("Universe ticks strictly forward in supplied simulation seconds") {
  Universe universe({ROOT}, 100);
  universe.tick(0.02);
  CHECK(universe.ut_sec() > 100);
  universe.tick(20);
  CHECK(universe.ut_sec() == Approx(120.02));
}

TEST_CASE("Universe rejects invalid ticks without changing UT") {
  Universe universe({ROOT}, 1);
  const double infinity = std::numeric_limits<double>::infinity();
  const double nan = std::numeric_limits<double>::quiet_NaN();
  CHECK_THROWS_AS(universe.tick(0), std::invalid_argument);
  CHECK_THROWS_AS(universe.tick(-1), std::invalid_argument);
  CHECK_THROWS_AS(universe.tick(infinity), std::invalid_argument);
  CHECK_THROWS_AS(universe.tick(nan), std::invalid_argument);
  CHECK_THROWS_AS(universe.tick(std::numeric_limits<double>::denorm_min()), std::invalid_argument);
  CHECK(universe.ut_sec() == 1);
  Universe near_limit({ROOT}, std::numeric_limits<double>::max());
  CHECK_THROWS_AS(near_limit.tick(std::numeric_limits<double>::max()), std::invalid_argument);
  CHECK(near_limit.ut_sec() == std::numeric_limits<double>::max());
}

TEST_CASE("Sun and Hamswell form a deterministic typed scenario") {
  const BodyConfig sun{"Sun", 6e8, 286, 614 * 3600, {}};
  const BodyConfig hamswell{
      "Hamswell", 3e6, 9.8, 24 * 3600, BodyOrbitConfig{"Sun", 9.2e10, 0.015, 0, 0, 0}
  };
  Universe universe({hamswell, sun});
  const auto at_epoch = universe.body_state("Hamswell", 0);
  CHECK(at_epoch.position.approx({9.2e10 * (1 - 0.015), 0}));
  CHECK(at_epoch.velocity.y > 0);
  const auto later = universe.body_state("Hamswell", 3600);
  CHECK(later.position.y > 0);
  CHECK(std::isfinite(later.position.x));
  universe.tick(3600);
  CHECK(universe.body_state("Hamswell", universe.ut_sec()).position.y == later.position.y);
}
