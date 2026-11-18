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
  return {
      "Test Craft",
      {"Hamswell", 1.73e10, 0.08, 1, 1, 0},
      {2.7, -1000},
      {{"Pod", "pod_1", "Pod", {0, 0}, 0, {}}}
  };
}

std::vector<flop::craft::PartTypeConfig> basic_types() {
  return {{"Pod", "Pod", 800, true, {{"bottom", {0, -1}}}}};
}

} // namespace

TEST_CASE("Craft owns its resolved orbital state and initial patch") {
  auto config = basic_config();
  const auto reference_frames = frames();
  flop::craft::Craft craft(config, reference_frames, basic_types());
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
  CHECK_THROWS_AS(
      flop::craft::Craft(config, reference_frames, basic_types()), std::invalid_argument
  );

  config = basic_config();
  config.orbit.specific_angular_momentum_m2_per_s = 0;
  CHECK_THROWS_AS(
      flop::craft::Craft(config, reference_frames, basic_types()), std::invalid_argument
  );
}

TEST_CASE("Craft owns installed parts and resolves reciprocal attachment nodes") {
  auto config = basic_config();
  config.parts = {
      {"Pod", "pod_1", "Pod", {0, 0}, 0, {{"bottom", "tank_2", "top"}}},
      {"Tank", "tank_2", "Tank", {0, -2}, 0, {{"top", "pod_1", "bottom"}}},
  };
  std::vector<flop::craft::PartTypeConfig> types{
      {"Pod", "Pod", 800, true, {{"bottom", {0, -1}}}},
      {"Tank", "Tank", 600, true, {{"top", {0, 1}}}},
  };
  const auto reference_frames = frames();
  const flop::craft::Craft craft(config, reference_frames, types);
  REQUIRE(craft.parts().size() == 2);
  CHECK(craft.part("tank_2").dry_mass_kg == 600);
  CHECK(craft.part("tank_2").position_meters.y == -2);
  CHECK(craft.part("pod_1").attachments.front().attach_part_id == "tank_2");

  config.parts[1].attachments.clear();
  CHECK_THROWS_AS(flop::craft::Craft(config, reference_frames, types), std::invalid_argument);
}
