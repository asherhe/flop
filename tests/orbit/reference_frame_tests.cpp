#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <stdexcept>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "flop/orbit/ref_frame.hpp"

namespace {

using Catch::Approx;
using flop::orbit::FrameId;
using flop::orbit::OrbitState;
using flop::orbit::RefFrameDefinition;
using flop::orbit::RefFrameManager;

constexpr double ROOT_MU = 100;
const RefFrameDefinition ROOT{"root", ROOT_MU, {}, {}, std::numeric_limits<double>::infinity()};
const RefFrameDefinition REGION{"region", 4, "root", OrbitState{100, {{4, 0}, {0, 5}}}, 1};

} // namespace

TEST_CASE("Moving frame offsets use the parent's gravity and the requested UT") {
  const RefFrameManager frames({REGION, ROOT});
  const auto root_id = frames.frame_id("root");
  const auto region_id = frames.frame_id("region");
  const double quarter_turn = 100 + 4 * std::numbers::pi / 10;
  const auto forward = frames.offset(region_id, root_id, quarter_turn);
  const auto reverse = frames.offset(root_id, region_id, quarter_turn);
  CHECK(forward.position.x == Approx(0).margin(1e-12));
  CHECK(forward.position.y == Approx(4));
  CHECK(forward.velocity.x == Approx(-5));
  CHECK(forward.velocity.y == Approx(0).margin(1e-12));
  CHECK((reverse.position + forward.position).length() == 0);
  CHECK((reverse.velocity + forward.velocity).length() == 0);

  const auto same = frames.offset(region_id, region_id, quarter_turn);
  CHECK(same.position.length() == 0);
  CHECK(same.velocity.length() == 0);
}

TEST_CASE("Frame offsets cancel a distant shared ancestor before summing") {
  constexpr double DISTANT = 1e17;
  const OrbitState outer{0, {{DISTANT, 0}, {0, std::sqrt(ROOT_MU / DISTANT)}}};
  const RefFrameManager frames({
      {"local", 1, "region", OrbitState{0, {{1, 0}, {0, 10}}}, 0.01},
      {"sibling", 1, "region", OrbitState{0, {{2, 0}, {0, 7}}}, 0.01},
      ROOT,
      {"region", 100, "root", outer, 1000},
  });
  const auto local_id = frames.frame_id("local");
  const auto region_id = frames.frame_id("region");
  const auto sibling_id = frames.frame_id("sibling");
  const auto relative_to_parent = frames.offset(local_id, region_id, 0);
  CHECK(relative_to_parent.position.x == 1);
  CHECK(relative_to_parent.velocity.y == 10);

  const auto relative_to_sibling = frames.offset(local_id, sibling_id, 0);
  CHECK(relative_to_sibling.position.x == -1);
  CHECK(relative_to_sibling.velocity.y == 3);
}

TEST_CASE("Frame managers assign IDs and resolve parent names in any input order") {
  const RefFrameManager original({ROOT, REGION});
  auto region = REGION;
  region.orbit_about_parent = original.frame(original.frame_id("region")).orbit_about_parent;
  const RefFrameManager frames({region, ROOT});
  const auto root_id = frames.frame_id("root");
  const auto region_id = frames.frame_id("region");
  CHECK(root_id != region_id);
  CHECK(root_id != FrameId{});
  CHECK(region_id != FrameId{});
  REQUIRE(frames.frames().size() == 2);
  CHECK(frames.frame(root_id).name == "root");
  CHECK_FALSE(frames.frame(root_id).parent.has_value());
  const auto &region_frame = frames.frame(region_id);
  CHECK(region_frame.id == region_id);
  CHECK(region_frame.name == "region");
  CHECK(region_frame.parent == root_id);
  REQUIRE(region_frame.orbit_about_parent.has_value());
  CHECK(region_frame.orbit_about_parent->frame_id == root_id);
  REQUIRE(region.orbit_about_parent.has_value());
  CHECK(region.orbit_about_parent->frame_id == original.frame_id("root"));
}

TEST_CASE("Frame hierarchies reject invalid topology and child orbits") {
  RefFrameDefinition child = REGION;
  SECTION("multiple roots") {
    child.parent_name.reset();
    child.orbit_about_parent.reset();
  }
  SECTION("duplicate names") {
    child.name = "root";
  }
  SECTION("empty name") {
    child.name.clear();
  }
  SECTION("unknown parent") {
    child.parent_name = "missing";
  }
  SECTION("cycle") {
    child.parent_name = child.name;
  }
  SECTION("missing orbit") {
    child.orbit_about_parent.reset();
  }
  SECTION("invalid frame gravity") {
    child.gravitational_parameter = 0;
  }
  SECTION("invalid sphere of influence") {
    child.sphere_of_influence_radius = 0;
  }
  SECTION("invalid orbit seed") {
    child.orbit_about_parent->state.position = {};
  }
  CHECK_THROWS_AS(RefFrameManager({ROOT, child}), std::invalid_argument);
}

TEST_CASE("Frame queries reject unknown IDs and nonfinite UT") {
  const RefFrameManager frames({ROOT, REGION});
  const auto root_id = frames.frame_id("root");
  const FrameId unknown_id{std::numeric_limits<std::uint32_t>::max()};
  CHECK_THROWS_AS(frames.frame_id("missing"), std::invalid_argument);
  CHECK_THROWS_AS(frames.frame(FrameId{}), std::invalid_argument);
  CHECK_THROWS_AS(frames.frame(unknown_id), std::invalid_argument);
  CHECK_THROWS_AS(frames.offset(root_id, unknown_id, 0), std::invalid_argument);
  CHECK_THROWS_AS(
      frames.offset(root_id, root_id, std::numeric_limits<double>::infinity()),
      std::invalid_argument
  );
}
