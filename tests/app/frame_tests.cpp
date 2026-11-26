#include <filesystem>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "flop/app/game_scene.hpp"
#include "flop/view/frame.hpp"

TEST_CASE("World sunlight is shared across sprite rotations") {
  const auto sun = flop::view::compute_sunlight({9.2e10, 0});
  const auto local = flop::view::to_sprite_local(sun, 1.5707963267948966);
  CHECK(sun.direction[0] == Catch::Approx(1));
  CHECK(sun.color[0] == Catch::Approx(8.5e21 / (9.2e10 * 9.2e10)).epsilon(1e-5));
  CHECK(local.direction[0] == Catch::Approx(0).margin(1e-6));
  CHECK(local.direction[1] == Catch::Approx(-1));
  CHECK(local.color == sun.color);
  const auto absent = flop::view::compute_sunlight({});
  CHECK(absent.color[0] == 0);
  CHECK(flop::view::to_sprite_local(absent, 1).direction[2] == 1);
}

TEST_CASE("Planet shine follows illuminated body geometry") {
  using flop::view::compute_planet_shine;
  const auto day = compute_planet_shine({9.2e10f, 0}, {4e6f, 0}, 3e6f, 1.0f);
  const auto night = compute_planet_shine({9.2e10f, 0}, {-4e6f, 0}, 3e6f, 1.0f);
  const auto far = compute_planet_shine({9.2e10f, 0}, {12e6f, 0}, 3e6f, 1.0f);
  CHECK(day.intensity > 0);
  CHECK(day.direction[0] == Catch::Approx(-1));
  CHECK(day.direction[1] == Catch::Approx(0).margin(1e-6));
  CHECK(day.spread > 0);
  CHECK(night.intensity == 0);
  CHECK(far.intensity < day.intensity);
  CHECK(compute_planet_shine({0, 0}, {4e6f, 0}, 3e6f, 1).intensity == 0);
  CHECK(compute_planet_shine({9.2e10f, 0}, {0, 0}, 3e6f, 1).intensity == 0);
  CHECK(compute_planet_shine({9.2e10f, 0}, {4e6f, 0}, 3e6f, 0).intensity == 0);
}

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
  sprite.size_meters = {2, 4};
  sprite.pivot = {0, 0};
  craft::plugin::LightConfig light{{1, 0}, "white", 1, 1, 2, 1};
  craft::CraftConfig config{"Test", craft::CraftOrbitState{"Root", {{1e11, 0}, {0, 1}}, 0},
      {0, 0}, {{"Pod", "a", "A", {0, 0}, 0, {}, {{"Sprite", true, sprite}, {"Light", true, light}}},
               {"Pod", "b", "B", {2, 0}, 1.5707963267948966, {}, {{"Sprite", true, sprite}}}}, true};
  const auto id = world.add_craft(config, {{"Pod", "Pod", 1, true, {}}});
  view::CameraState camera;
  camera.focus = id;
  const auto frame = camera.build_frame(world, {{"Root", "white", {}, {}, {}}});
  REQUIRE(frame.sprites.size() == 2);
  REQUIRE(frame.lights.size() == 1);
  CHECK(frame.sprites[0].sun.color[0] == 0.0f); // no body named Sun
  CHECK(frame.sprites[0].sun.direction[2] == 1.0f);
  CHECK(frame.sprites[1].center_pixels.x - frame.sprites[0].center_pixels.x == Catch::Approx(100));
  const auto &corners = frame.sprites[0].corners_pixels;
  CHECK(corners[0].x == Catch::Approx(frame.sprites[0].center_pixels.x));
  CHECK(corners[0].y == Catch::Approx(frame.sprites[0].center_pixels.y));
  CHECK(corners[1].x - corners[0].x == Catch::Approx(100));
  CHECK(corners[3].y - corners[0].y == Catch::Approx(-200));
  const auto &rotated = frame.sprites[1].corners_pixels;
  CHECK(rotated[1].x - rotated[0].x == Catch::Approx(0).margin(0.001));
  CHECK(rotated[1].y - rotated[0].y == Catch::Approx(-100));
  CHECK(frame.lights[0].camera_relative_meters.x == Catch::Approx(0));
}

TEST_CASE("Sprite sunlight is local to each part and independent of the camera") {
  using namespace flop;
  world::Universe world({{"Sun", 3e6, 9.8, 86400, {}}});
  craft::plugin::SpriteConfig sprite{"sprite.png", {}};
  sprite.size_meters = {2, 2};
  sprite.pivot = {0.5, 0.5};
  craft::CraftConfig config{"Test", craft::CraftOrbitState{"Sun", {{9.2e10, 0}, {0, 1}}, 0},
      {0, 0}, {{"Pod", "a", "A", {0, 0}, 0, {}, {{"Sprite", true, sprite}}},
               {"Pod", "b", "B", {2, 0}, 1.5707963267948966, {}, {{"Sprite", true, sprite}}}}, true};
  const auto id = world.add_craft(config, {{"Pod", "Pod", 1, true, {}}});
  view::CameraState camera;
  camera.focus = id;
  const auto frame = camera.build_frame(world, {{"Sun", "white", {}, {}, {}}});
  REQUIRE(frame.sprites.size() == 2);
  const auto &a = frame.sprites[0].sun;
  const auto &b = frame.sprites[1].sun;
  CHECK(a.color[0] == Catch::Approx(8.5e21 / (9.2e10 * 9.2e10)).epsilon(1e-5));
  CHECK(a.color[1] == a.color[0]);
  CHECK(a.color[2] == a.color[0]);
  CHECK(a.color[3] == 0.0f);
  CHECK(a.direction[0] == Catch::Approx(-1.0).epsilon(1e-5));
  CHECK(a.direction[1] == Catch::Approx(0).margin(1e-6));
  CHECK(b.direction[0] == Catch::Approx(0).margin(1e-6));
  CHECK(b.direction[1] == Catch::Approx(-a.direction[0]).epsilon(1e-5));
  CHECK(b.direction[2] == Catch::Approx(a.direction[2]));
  camera.pan_meters = {12345, -6789};
  camera.pixels_per_meter = 2;
  const auto moved = camera.build_frame(world, {{"Sun", "white", {}, {}, {}}});
  CHECK(moved.sprites[0].sun.direction == a.direction);
  CHECK(moved.sprites[0].sun.color == a.color);
}
