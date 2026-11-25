#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace flop::render {

// describes shader inputs separately from the serialized material format
// renderer inputs are supplied per draw; authored inputs come from content
enum class MaterialInputSource : std::uint8_t { authored, renderer };
enum class MaterialInputType : std::uint8_t { texture_2d, float4 };
enum class ShaderStage : std::uint8_t { vertex, fragment };

struct MaterialInput {
  std::string_view name;
  ShaderStage stage;
  MaterialInputType type;
  MaterialInputSource source;
};

struct MaterialRecipe {
  std::string_view shader;
  std::span<const MaterialInput> inputs;
};

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
        MaterialInputSource::renderer
    },
    MaterialInput{
        "y_axis_and_viewport",
        ShaderStage::vertex,
        MaterialInputType::float4,
        MaterialInputSource::renderer
    },
};
inline constexpr MaterialRecipe craft_sprite_recipe{"craft/sprite", craft_sprite_inputs};

} // namespace flop::render
