#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <random>

#include "flop/render/material_recipe.hpp"
#include "flop/config/load.hpp"

TEST_CASE("Material recipes decode automatic semantics and reject unknown fields") {
  const auto node = YAML::Load("shader: shader:craft/sprite\nproperties:\n  albedo: {stage: fragment, type: texture_2d}\n  origin_viewport: {stage: vertex, type: float4, auto: sprite.projected_origin_viewport}\n");
  auto recipe = flop::render::decode_material_recipe(flop::config::ConfigView(node, "fixture.yaml"));
  REQUIRE(recipe.view().inputs.size() == 2);
  CHECK(recipe.view().inputs[1].auto_input == flop::render::AutoInput::sprite_projected_origin_viewport);
  const auto invalid = YAML::Load("shader: shader:craft/sprite\nproperties:\n  x: {stage: vertex, type: float4, auto: unknown.value}\n");
  CHECK_THROWS_AS(flop::render::decode_material_recipe(flop::config::ConfigView(invalid, "fixture.yaml")), flop::config::ConfigError);
  for (const auto *yaml : {
           "shader: shader:craft/sprite\nshader: duplicate\nproperties: {}",
           "shader: shader:craft/sprite\nproperties: {}\nunknown: true",
           "shader: shader:craft/sprite\nproperties:\n  x: {stage: vertex, type: float4, extra: true}",
           "shader: shader:craft/sprite\nproperties:\n  x: {stage: invalid, type: float4}",
           "shader: shader:craft/sprite\nproperties:\n  x: {stage: vertex, type: invalid}"}) {
    CHECK_THROWS_AS(flop::render::decode_material_recipe(
                        flop::config::ConfigView(YAML::Load(yaml), "fixture.yaml")),
                    flop::config::ConfigError);
  }
}

TEST_CASE("Sun semantics require fragment float4 inputs") {
  const auto valid = YAML::Load("shader: shader:craft/lit_sprite\nproperties:\n  direction: {stage: fragment, type: float4, auto: sun.direction}\n  color: {stage: fragment, type: float4, auto: sun.color}\n");
  const auto recipe = flop::render::decode_material_recipe(flop::config::ConfigView(valid, "lighting.yaml"));
  REQUIRE(recipe.inputs.size() == 2);
  CHECK(recipe.inputs[0].auto_input == flop::render::AutoInput::sun_direction);
  CHECK(recipe.inputs[1].auto_input == flop::render::AutoInput::sun_color);
  for (const auto *yaml : {
           "shader: shader:craft/lit_sprite\nproperties:\n  x: {stage: vertex, type: float4, auto: sun.color}",
           "shader: shader:craft/lit_sprite\nproperties:\n  x: {stage: fragment, type: texture_2d, auto: sun.direction}"}) {
    try {
      (void)flop::render::decode_material_recipe(flop::config::ConfigView(YAML::Load(yaml), "lighting.yaml"));
      FAIL("expected an invalid automatic input error");
    } catch (const flop::config::ConfigError &error) {
      CHECK(std::string(error.what()).find("lighting.yaml") != std::string::npos);
      CHECK(std::string(error.what()).find("properties.x.auto") != std::string::npos);
    }
  }
}

TEST_CASE("Local light texture semantic requires a fragment texture") {
  const auto valid = YAML::Load("shader: shader:craft/lit_sprite\nproperties:\n  local_light: {stage: fragment, type: texture_2d, auto: local_light.texture}\n");
  const auto recipe = flop::render::decode_material_recipe(flop::config::ConfigView(valid, "lighting.yaml"));
  REQUIRE(recipe.inputs.size() == 1);
  CHECK(recipe.inputs[0].auto_input == flop::render::AutoInput::local_light_texture);
  for (const auto *yaml : {
           "shader: shader:craft/lit_sprite\nproperties:\n  x: {stage: vertex, type: texture_2d, auto: local_light.texture}",
           "shader: shader:craft/lit_sprite\nproperties:\n  x: {stage: fragment, type: float4, auto: local_light.texture}"}) {
    CHECK_THROWS_AS(flop::render::decode_material_recipe(
                        flop::config::ConfigView(YAML::Load(yaml), "lighting.yaml")),
                    flop::config::ConfigError);
  }
}

using namespace flop::render;

TEST_CASE("Lit sprite bindings validate reflected light inputs") {
  const auto yaml = YAML::Load(R"(shader: shader:craft/lit_sprite
properties:
  albedo: {stage: fragment, type: texture_2d}
  normal_map: {stage: fragment, type: texture_2d}
  emission_map: {stage: fragment, type: texture_2d}
  local_light: {stage: fragment, type: texture_2d, auto: local_light.texture}
  origin_viewport: {stage: vertex, type: float4, auto: sprite.projected_origin_viewport}
  xy_axis: {stage: vertex, type: float4, auto: sprite.projected_xy_axis}
  sun_direction: {stage: fragment, type: float4, auto: sun.direction}
  sun_color: {stage: fragment, type: float4, auto: sun.color}
  planet_shine_direction_and_spread: {stage: fragment, type: float4, auto: planet_shine.direction_and_spread}
  planet_shine_color: {stage: fragment, type: float4, auto: planet_shine.color}
)");
  const auto recipe = decode_material_recipe(flop::config::ConfigView(yaml, "fixture.yaml"));
  shaders::Entry vertex{"shader:craft/lit_sprite", "vertex", "spirv", "vertex", nullptr, 0, 0, 0, 0, 1,
    R"({"stage":"vertex","uniform_buffers":[{"binding":0,"size":32,"members":[{"name":"origin_viewport","type":"float","components":4,"offset":0,"size":16},{"name":"xy_axis","type":"float","components":4,"offset":16,"size":16}]}]})"};
  shaders::Entry fragment{"shader:craft/lit_sprite", "fragment", "spirv", "fragment", nullptr, 0, 4, 0, 0, 1,
    R"({"stage":"fragment","textures":[{"name":"albedo","binding":0},{"name":"normal_map","binding":1},{"name":"emission_map","binding":2},{"name":"local_light","binding":3}],"uniform_buffers":[{"binding":0,"size":64,"members":[{"name":"sun_direction","type":"float","components":4,"offset":0,"size":16},{"name":"sun_color","type":"float","components":4,"offset":16,"size":16},{"name":"planet_shine_direction_and_spread","type":"float","components":4,"offset":32,"size":16},{"name":"planet_shine_color","type":"float","components":4,"offset":48,"size":16}]}]})"};
  const auto bindings = resolve_craft_lit_sprite_bindings(recipe.view(), vertex, fragment);
  CHECK(bindings.normal_slot == 1);
  CHECK(bindings.emission_slot == 2);
  CHECK(bindings.local_light_slot == 3);
  CHECK(bindings.color_offset == 16);
  vertex.format = "dxil";
  vertex.reflection_json = R"({"stage":"vertex","resources":[{"name":"Geometry","type":"uniform_buffer","binding":0}],"uniform_buffers":[{"name":"Geometry","size":32,"members":[{"name":"origin_viewport","type":"float","components":4,"offset":0,"size":16},{"name":"xy_axis","type":"float","components":4,"offset":16,"size":16}]}]})";
  fragment.format = "dxil";
  fragment.reflection_json = R"({"stage":"fragment","resources":[{"name":"albedo","type":"texture","binding":0},{"name":"normal_map","type":"texture","binding":1},{"name":"emission_map","type":"texture","binding":2},{"name":"local_light","type":"texture","binding":3},{"name":"Light","type":"uniform_buffer","binding":1}],"uniform_buffers":[{"name":"Light","size":64,"members":[{"name":"sun_direction","type":"float","components":4,"offset":0,"size":16},{"name":"sun_color","type":"float","components":4,"offset":16,"size":16},{"name":"planet_shine_direction_and_spread","type":"float","components":4,"offset":32,"size":16},{"name":"planet_shine_color","type":"float","components":4,"offset":48,"size":16}]}]})";
  const auto dxil = resolve_craft_lit_sprite_bindings(recipe.view(), vertex, fragment);
  CHECK(dxil.light_uniform_slot == 1);
  CHECK(dxil.normal_slot == 1);
  CHECK(dxil.emission_slot == 2);
  CHECK(dxil.local_light_slot == 3);
  fragment.reflection_json = R"({"stage":"fragment","textures":[],"uniform_buffers":[]})";
  try {
    (void)resolve_craft_lit_sprite_bindings(recipe.view(), vertex, fragment);
    FAIL("expected invalid reflection");
  } catch (const std::runtime_error &error) {
    CHECK(std::string(error.what()).find("fixture.yaml") != std::string::npos);
  }
}

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
  const char *vertex_json = R"({"stage":"vertex","uniform_buffers":[{"binding":2,"size":32,"members":[{"name":"origin_viewport","type":"float","components":4,"offset":0,"size":16},{"name":"xy_axis","type":"float","components":4,"offset":16,"size":16}]}]})";
  const char *fragment_json = R"({"stage":"fragment","textures":[{"name":"albedo","binding":3}]})";
  shaders::Entry vertex{"shader:craft/sprite", "vertex", "spirv", "vertex", nullptr, 0, 0, 0, 0, 1, vertex_json};
  shaders::Entry fragment{"shader:craft/sprite", "fragment", "spirv", "fragment", nullptr, 0, 1, 0, 0, 0, fragment_json};
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
  bad_inputs[1].auto_input = AutoInput::sprite_projected_xy_axis;
  const MaterialRecipe bad_recipe{"shader:craft/sprite", bad_inputs};
  CHECK_THROWS_AS(resolve_craft_sprite_bindings(bad_recipe, vertex, fragment), std::runtime_error);
  fragment.reflection_json = R"({"stage":"fragment","textures":[]})";
  CHECK_THROWS_AS(resolve_craft_sprite_bindings(craft_sprite_recipe, vertex, fragment), std::runtime_error);
  fragment.reflection_json = fragment_json;
  vertex.reflection_json = R"({"stage":"vertex","uniform_buffers":[{"binding":0,"size":16,"members":[]}]})";
  CHECK_THROWS_AS(resolve_craft_sprite_bindings(craft_sprite_recipe, vertex, fragment), std::runtime_error);
}

TEST_CASE("Craft recipe resolves DXIL resource bindings") {
  shaders::Entry vertex{"shader:craft/sprite", "vertex", "dxil", "vertex", nullptr, 0, 0, 0, 0, 1,
    R"({"stage":"vertex","resources":[{"name":"SpriteGeometry","type":"uniform_buffer","binding":0}],"uniform_buffers":[{"name":"SpriteGeometry","size":32,"members":[{"name":"origin_viewport","type":"float","components":4,"offset":0,"size":16},{"name":"xy_axis","type":"float","components":4,"offset":16,"size":16}]}]})"};
  shaders::Entry fragment{"shader:craft/sprite", "fragment", "dxil", "fragment", nullptr, 0, 1, 0, 0, 0,
    R"({"stage":"fragment","resources":[{"name":"albedo_sampler","type":"sampler","binding":0},{"name":"albedo","type":"texture","binding":0}]})"};
  const auto bindings = resolve_craft_sprite_bindings(craft_sprite_recipe, vertex, fragment);
  CHECK(bindings.uniform_slot == 0);
  CHECK(bindings.texture_slot == 0);
}
