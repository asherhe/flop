#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <random>

#include "flop/render/material_recipe.hpp"
#include "flop/config/load.hpp"

TEST_CASE("Material recipes decode automatic semantics and reject unknown fields") {
  const auto node = YAML::Load("shader: craft/sprite\nproperties:\n  albedo: {stage: fragment, type: texture_2d}\n  origin_and_x_axis: {stage: vertex, type: float4, auto: sprite.projected_origin_and_x_axis}\n");
  auto recipe = flop::render::decode_material_recipe(flop::config::ConfigView(node, "fixture.yaml"));
  REQUIRE(recipe.view().inputs.size() == 2);
  CHECK(recipe.view().inputs[1].auto_input == flop::render::AutoInput::sprite_projected_origin_and_x_axis);
  const auto invalid = YAML::Load("shader: craft/sprite\nproperties:\n  x: {stage: vertex, type: float4, auto: unknown.value}\n");
  CHECK_THROWS_AS(flop::render::decode_material_recipe(flop::config::ConfigView(invalid, "fixture.yaml")), flop::config::ConfigError);
  for (const auto *yaml : {
           "shader: craft/sprite\nshader: duplicate\nproperties: {}",
           "shader: craft/sprite\nproperties: {}\nunknown: true",
           "shader: craft/sprite\nproperties:\n  x: {stage: vertex, type: float4, extra: true}",
           "shader: craft/sprite\nproperties:\n  x: {stage: invalid, type: float4}",
           "shader: craft/sprite\nproperties:\n  x: {stage: vertex, type: invalid}"}) {
    CHECK_THROWS_AS(flop::render::decode_material_recipe(
                        flop::config::ConfigView(YAML::Load(yaml), "fixture.yaml")),
                    flop::config::ConfigError);
  }
}

using namespace flop::render;

TEST_CASE("A missing material recipe reports its asset path") {
  namespace fs = std::filesystem;
  struct TempDirectory {
    fs::path path;
    TempDirectory() {
      do { path = fs::temp_directory_path() / ("flop-recipe-test-" + std::to_string(std::random_device{}())); }
      while (!fs::create_directory(path));
    }
    ~TempDirectory() { std::error_code error; fs::remove_all(path, error); }
  } temp;
  fs::create_directory(temp.path / "assets");
  try {
    static_cast<void>(load_craft_sprite_recipe(flop::config::AssetResolver(temp.path / "assets")));
    FAIL("Expected a missing recipe error");
  } catch (const std::exception &error) {
    CHECK(std::string(error.what()).find("craft_sprite.yaml") != std::string::npos);
  }
}

TEST_CASE("Craft recipe resolves reflected bindings and rejects incompatible inputs") {
  const char *vertex_json = R"({"stage":"vertex","uniform_buffers":[{"binding":2,"size":32,"members":[{"name":"origin_and_x_axis","type":"float","components":4,"offset":0,"size":16},{"name":"y_axis_and_viewport","type":"float","components":4,"offset":16,"size":16}]}]})";
  const char *fragment_json = R"({"stage":"fragment","textures":[{"name":"albedo","binding":3}]})";
  shaders::Entry vertex{"craft/sprite", "vertex", "spirv", "vertex", nullptr, 0, 0, 0, 0, 1, vertex_json};
  shaders::Entry fragment{"craft/sprite", "fragment", "spirv", "fragment", nullptr, 0, 1, 0, 0, 0, fragment_json};
  const auto bindings = resolve_craft_sprite_bindings(craft_sprite_recipe, vertex, fragment);
  CHECK(bindings.uniform_slot == 2);
  CHECK(bindings.texture_slot == 3);
  auto wrong_shader = fragment;
  wrong_shader.id = "other/shader";
  CHECK_THROWS_AS(resolve_craft_sprite_bindings(craft_sprite_recipe, vertex, wrong_shader), std::runtime_error);
  auto wrong_format = fragment;
  wrong_format.format = "dxil";
  CHECK_THROWS_AS(resolve_craft_sprite_bindings(craft_sprite_recipe, vertex, wrong_format), std::runtime_error);
  auto bad_inputs = craft_sprite_inputs;
  bad_inputs[1].auto_input = AutoInput::sprite_projected_y_axis_and_viewport;
  const MaterialRecipe bad_recipe{"craft/sprite", bad_inputs};
  CHECK_THROWS_AS(resolve_craft_sprite_bindings(bad_recipe, vertex, fragment), std::runtime_error);
  fragment.reflection_json = R"({"stage":"fragment","textures":[]})";
  CHECK_THROWS_AS(resolve_craft_sprite_bindings(craft_sprite_recipe, vertex, fragment), std::runtime_error);
  fragment.reflection_json = fragment_json;
  vertex.reflection_json = R"({"stage":"vertex","uniform_buffers":[{"binding":0,"size":16,"members":[]}]})";
  CHECK_THROWS_AS(resolve_craft_sprite_bindings(craft_sprite_recipe, vertex, fragment), std::runtime_error);
}

TEST_CASE("Craft recipe resolves DXIL resource bindings") {
  shaders::Entry vertex{"craft/sprite", "vertex", "dxil", "vertex", nullptr, 0, 0, 0, 0, 1,
    R"({"stage":"vertex","resources":[{"name":"SpriteGeometry","type":"uniform_buffer","binding":0}],"uniform_buffers":[{"name":"SpriteGeometry","size":32,"members":[{"name":"origin_and_x_axis","type":"float","components":4,"offset":0,"size":16},{"name":"y_axis_and_viewport","type":"float","components":4,"offset":16,"size":16}]}]})"};
  shaders::Entry fragment{"craft/sprite", "fragment", "dxil", "fragment", nullptr, 0, 1, 0, 0, 0,
    R"({"stage":"fragment","resources":[{"name":"albedo_sampler","type":"sampler","binding":0},{"name":"albedo","type":"texture","binding":0}]})"};
  const auto bindings = resolve_craft_sprite_bindings(craft_sprite_recipe, vertex, fragment);
  CHECK(bindings.uniform_slot == 0);
  CHECK(bindings.texture_slot == 0);
}
