#include <filesystem>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "flop/app/game_scene.hpp"
#include "flop/view/frame.hpp"

TEST_CASE("Frame extraction tracks camera and enabled visual plugins without advancing the world") {
  auto scene = flop::app::load_game_scene(flop::config::AssetResolver(std::filesystem::path(FLOP_TEST_ASSET_ROOT)));
  auto &world = *scene.universe;
  flop::view::CameraState camera;
  camera.focus = scene.craft.front();
  const auto first = camera.build_frame(world, scene.body_visuals);
  REQUIRE(first.sprites.size() == 3);
  CHECK(first.sprites[0].visual.size_meters.y == 2.0);
  CHECK(first.sprites[1].visual.size_meters.y == 2.0);
  CHECK(first.sprites[2].visual.size_meters.y == 1.5);
  CHECK(first.sprites[2].visual.pivot.x == 0.5);
  const auto &craft = world.craft(scene.craft.front());
  const auto &part = craft.parts().front();
  const auto expected = (part.position_meters - craft.mass_properties().center_of_mass_meters)
                            .rotate(craft.newtonian().angle_rad());
  CHECK(first.sprites.front().camera_relative_meters.approx(expected));
  camera.update({10, 0, 0}, 1280, 720, 2560, 1440);
  const auto panned = camera.build_frame(world, scene.body_visuals);
  CHECK(panned.sprites.front().center_pixels.x == Catch::Approx(first.sprites.front().center_pixels.x + 640 + 20));
  CHECK(world.ut_sec() == first.ut_sec);
  const auto &sprite = first.sprites.front();
  world.controls().submit(flop::world::ControlManager::SetPluginEnabled{{sprite.craft, sprite.part, sprite.plugin}, false});
  (void)world.controls().apply_commands();
  CHECK(camera.build_frame(world, scene.body_visuals).sprites.size() == 2);
}

TEST_CASE("Focus relative projection preserves metre offsets at 1e11 metres") {
  using namespace flop;
  world::Universe world({{"Root", 3e6, 9.8, 86400, {}}});
  craft::plugin::SpriteConfig sprite{"sprite.png", {}};
  craft::plugin::LightConfig light{{1, 0}, "white", 1, 1, 2, 1};
  craft::CraftConfig config{"Test", craft::CraftOrbitState{"Root", {{1e11, 0}, {0, 1}}, 0},
      {0, 0}, {{"Pod", "a", "A", {0, 0}, 0, {}, {{"Sprite", true, sprite}, {"Light", true, light}}},
               {"Pod", "b", "B", {2, 0}, 0, {}, {{"Sprite", true, sprite}}}}, true};
  const auto id = world.add_craft(config, {{"Pod", "Pod", 1, true, {}}});
  view::CameraState camera;
  camera.focus = id;
  const auto frame = camera.build_frame(world, {{"Root", "white", {}, {}, {}}});
  REQUIRE(frame.sprites.size() == 2);
  REQUIRE(frame.lights.size() == 1);
  CHECK(frame.sprites[1].center_pixels.x - frame.sprites[0].center_pixels.x == Catch::Approx(100));
  CHECK(frame.lights[0].camera_relative_meters.x == Catch::Approx(0));
}
