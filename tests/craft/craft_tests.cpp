#include <limits>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "flop/craft/craft.hpp"

namespace {

flop::orbit::RefFrameManager frames() {
  return flop::orbit::RefFrameManager(
      {{"Hamswell", 8.82e13, {}, {}, std::numeric_limits<double>::infinity()}}
  );
}

flop::craft::CraftConfig basic_config() {
  return {"Test Craft", {"Hamswell", 1.73e10, 0.08, 1, 1, 0}, {2.7, -1000}};
}

} // namespace

TEST_CASE("Craft owns its resolved orbital state and initial patch") {
  auto config = basic_config();
  const auto reference_frames = frames();
  flop::craft::Craft craft(config, reference_frames);
  config.name = "Changed";

  CHECK(craft.name() == "Test Craft");
  CHECK(craft.orbit_state().frame_id == reference_frames.frame_id("Hamswell"));
  CHECK(craft.orbit_state().epoch == 0);
  REQUIRE(craft.patches().patches.size() == 1);
  CHECK(craft.patches().patches.front().orbit.frame_id == craft.orbit_state().frame_id);

  craft.predict_patches(reference_frames, 0, 60);
  CHECK(craft.patches().searched_through == 60);
  CHECK(craft.patches().patch_at(60) != nullptr);
}

TEST_CASE("Craft validates parent reference and typed orbital inputs") {
  const auto reference_frames = frames();
  auto config = basic_config();
  config.orbit.parent_name = "missing";
  CHECK_THROWS_AS(flop::craft::Craft(config, reference_frames), std::invalid_argument);

  config = basic_config();
  config.orbit.specific_angular_momentum_m2_per_s = 0;
  CHECK_THROWS_AS(flop::craft::Craft(config, reference_frames), std::invalid_argument);
}
