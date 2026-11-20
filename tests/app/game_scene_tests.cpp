// Scene loading: the hard-coded bodies/resources/part types plus the craft from
// the save assemble into one populated Universe with matching body visuals.
// This is the SDL-free assembly the App drives; it is exercised headless here.

#include <filesystem>

#include <catch2/catch_test_macros.hpp>

#include "flop/app/game_scene.hpp"
#include "flop/config/asset_path.hpp"

namespace {
using flop::app::load_game_scene;
using flop::config::AssetResolver;
} // namespace

TEST_CASE("The first scene assembles bodies, visuals, and the saved craft") {
  const AssetResolver resolver(std::filesystem::path(FLOP_TEST_ASSET_ROOT));
  const auto scene = load_game_scene(resolver);

  REQUIRE(scene.universe != nullptr);
  // Sun + Hamswell are the hard-coded bodies.
  CHECK(scene.universe->bodies().size() == 2);
  CHECK(scene.universe->body("Sun").name() == "Sun");
  CHECK(scene.universe->body("Hamswell").name() == "Hamswell");

  // one visual config per body, in the same order.
  REQUIRE(scene.body_visuals.size() == 2);
  CHECK(scene.body_visuals[0].name == "Sun");
  CHECK(scene.body_visuals[1].name == "Hamswell");
  CHECK(scene.body_visuals[1].atmosphere.has_value());

  // the save's single craft was placed and selectable by its returned id.
  REQUIRE(scene.craft.size() == 1);
  CHECK(scene.universe->craft_count() == 1);
  CHECK(scene.universe->craft(scene.craft.front()).name() == "Orbital Test Craft");
  CHECK(scene.universe->craft(scene.craft.front()).parts().size() == 3);
}
