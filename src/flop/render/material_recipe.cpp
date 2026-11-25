#include "flop/render/material_recipe.hpp"

#include <stdexcept>
#include <string>

#include <yaml-cpp/yaml.h>

namespace flop::render {

CraftSpriteBindings resolve_craft_sprite_bindings(const MaterialRecipe &recipe,
                                                  const shaders::Entry &vertex,
                                                  const shaders::Entry &fragment) {
  try {
    if (vertex.id != recipe.shader || fragment.id != recipe.shader ||
        vertex.stage != "vertex" || fragment.stage != "fragment" ||
        vertex.format != fragment.format ||
        (vertex.format != "spirv" && vertex.format != "dxil"))
      throw std::runtime_error("incompatible shader variants");

    const auto v = YAML::Load(std::string(vertex.reflection_json));
    const auto f = YAML::Load(std::string(fragment.reflection_json));
    if (v["stage"].as<std::string>() != "vertex" || f["stage"].as<std::string>() != "fragment")
      throw std::runtime_error("reflection stage mismatch");
    const auto buffers = v["uniform_buffers"];
    if (!buffers.IsSequence() || buffers.size() != 1)
      throw std::runtime_error("expected one vertex uniform buffer");
    const auto buffer = buffers[0];
    const auto members = buffer["members"];
    if (!members.IsSequence() || members.size() != 2 || recipe.inputs.size() != 3)
      throw std::runtime_error("unexpected craft sprite inputs");

    CraftSpriteBindings result{};
    result.uniform_size = buffer["size"].as<unsigned>();
    unsigned matched = 0;
    for (const auto &input : recipe.inputs) {
      if (input.name == "albedo" && input.stage == ShaderStage::fragment &&
          input.type == MaterialInputType::texture_2d && input.source == MaterialInputSource::authored) {
        const auto textures = vertex.format == "dxil" ? f["resources"] : f["textures"];
        unsigned count = 0;
        for (const auto &texture : textures) {
          if (texture["name"].as<std::string>() == input.name &&
              (vertex.format != "dxil" || texture["type"].as<std::string>() == "texture")) {
            result.texture_slot = texture["binding"].as<unsigned>();
            ++count;
          }
        }
        if (count != 1) throw std::runtime_error("missing or duplicate albedo texture");
        matched |= 1;
      } else if ((input.name == "origin_and_x_axis" || input.name == "y_axis_and_viewport") &&
                 input.stage == ShaderStage::vertex && input.type == MaterialInputType::float4 &&
                 input.source == MaterialInputSource::renderer) {
        unsigned count = 0;
        for (const auto &member : members) {
          if (member["name"].as<std::string>() != input.name) continue;
          if (member["type"].as<std::string>() != "float" || member["components"].as<unsigned>() != 4 ||
              member["size"].as<unsigned>() != 16)
            throw std::runtime_error("incompatible uniform " + std::string(input.name));
          const auto offset = member["offset"].as<unsigned>();
          if (input.name == "origin_and_x_axis") { result.origin_offset = offset; matched |= 2; }
          else { result.viewport_offset = offset; matched |= 4; }
          ++count;
        }
        if (count != 1) throw std::runtime_error("missing or duplicate uniform " + std::string(input.name));
      } else throw std::runtime_error("unknown or incompatible input " + std::string(input.name));
    }
    if (matched != 7) throw std::runtime_error("missing or duplicate recipe input");
    if (vertex.format == "spirv") result.uniform_slot = buffer["binding"].as<unsigned>();
    else {
      unsigned count = 0;
      for (const auto &resource : v["resources"]) {
        if (resource["type"].as<std::string>() == "uniform_buffer" &&
            resource["name"].as<std::string>() == buffer["name"].as<std::string>()) {
          result.uniform_slot = resource["binding"].as<unsigned>();
          ++count;
        }
      }
      if (count != 1) throw std::runtime_error("missing uniform buffer binding");
    }
    if (result.uniform_size != 32 || result.origin_offset != 0 || result.viewport_offset != 16)
      throw std::runtime_error("uniform layout does not match SpriteGeometry");
    return result;
  } catch (const std::exception &error) {
    throw std::runtime_error("material '" + std::string(recipe.shader) + "' (" +
                             std::string(vertex.format) + "): " + error.what());
  }
}
} // namespace flop::render
