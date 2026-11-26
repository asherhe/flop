#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <string>
#include <vector>

#include "flop/render/shader_library.hpp"

namespace flop::config { class ConfigView; class AssetResolver; }

namespace flop::render {

// describes shader inputs separately from the serialized material format
// renderer inputs are supplied per draw; authored inputs come from content
enum class MaterialInputSource : std::uint8_t { authored, renderer };
enum class MaterialInputType : std::uint8_t { texture_2d, float4 };
enum class ShaderStage : std::uint8_t { vertex, fragment };
// per-draw float4 values in drawable pixels: origin and x axis, then y axis
// and viewport size. sprite corners are relative to the upper-left of the viewport.
enum class AutoInput : std::uint8_t {
  none,
  sprite_projected_origin_and_x_axis,
  sprite_projected_y_axis_and_viewport,
  sun_direction,
  sun_color,
};

constexpr std::string_view auto_input_name(AutoInput input) {
  switch (input) {
  case AutoInput::none: return {};
  case AutoInput::sprite_projected_origin_and_x_axis: return "sprite.projected_origin_and_x_axis";
  case AutoInput::sprite_projected_y_axis_and_viewport: return "sprite.projected_y_axis_and_viewport";
  case AutoInput::sun_direction: return "sun.direction";
  case AutoInput::sun_color: return "sun.color";
  }
  return {};
}

struct MaterialInput {
  std::string_view name;
  ShaderStage stage;
  MaterialInputType type;
  MaterialInputSource source;
  AutoInput auto_input{AutoInput::none};
};

struct MaterialRecipe {
  std::string_view shader;
  std::span<const MaterialInput> inputs;
  std::string_view source{};
};

struct LoadedMaterialRecipe {
  std::string shader;
  std::string source;
  std::vector<std::string> names;
  std::vector<MaterialInput> inputs;
  LoadedMaterialRecipe() = default;
  LoadedMaterialRecipe(const LoadedMaterialRecipe &) = delete;
  LoadedMaterialRecipe &operator=(const LoadedMaterialRecipe &) = delete;
  LoadedMaterialRecipe(LoadedMaterialRecipe &&) = default;
  [[nodiscard]] MaterialRecipe view() const { return {shader, inputs, source}; }
};

LoadedMaterialRecipe decode_material_recipe(const config::ConfigView &document);
LoadedMaterialRecipe load_craft_sprite_recipe(const config::AssetResolver &resolver);
LoadedMaterialRecipe load_craft_lit_sprite_recipe(const config::AssetResolver &resolver);

// the sprite image comes from SpriteConfig::sprite; geometry comes from the
// projected corners and drawable viewport. the renderer owns the albedo sampler
inline constexpr std::array craft_sprite_inputs{
    MaterialInput{
        "albedo",
        ShaderStage::fragment,
        MaterialInputType::texture_2d,
        MaterialInputSource::authored
    },
    MaterialInput{
        "origin_and_x_axis",
        ShaderStage::vertex,
        MaterialInputType::float4,
        MaterialInputSource::renderer,
        AutoInput::sprite_projected_origin_and_x_axis
    },
    MaterialInput{
        "y_axis_and_viewport",
        ShaderStage::vertex,
        MaterialInputType::float4,
        MaterialInputSource::renderer,
        AutoInput::sprite_projected_y_axis_and_viewport
    },
};
inline constexpr MaterialRecipe craft_sprite_recipe{"craft/sprite", craft_sprite_inputs};

struct CraftSpriteBindings {
  unsigned texture_slot;
  unsigned uniform_slot;
  unsigned origin_offset;
  unsigned viewport_offset;
  unsigned uniform_size;
};

CraftSpriteBindings resolve_craft_sprite_bindings(const MaterialRecipe &recipe,
                                                  const shaders::Entry &vertex,
                                                  const shaders::Entry &fragment);

struct CraftLitSpriteBindings {
  CraftSpriteBindings geometry;
  unsigned normal_slot, emission_slot;
  unsigned light_uniform_slot, light_uniform_size, direction_offset, color_offset;
};

CraftLitSpriteBindings resolve_craft_lit_sprite_bindings(const MaterialRecipe &recipe,
    const shaders::Entry &vertex, const shaders::Entry &fragment);

} // namespace flop::render
