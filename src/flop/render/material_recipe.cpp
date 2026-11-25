#include "flop/render/material_recipe.hpp"

#include <stdexcept>
#include <string>

#include <yaml-cpp/yaml.h>
#include "flop/config/load.hpp"

namespace flop::render {

LoadedMaterialRecipe decode_material_recipe(const config::ConfigView &document) {
  document.validate_unique_keys();
  LoadedMaterialRecipe result;
  result.shader = document.required("shader").nonempty_string();
  const auto properties = document.required("properties");
  for (const auto &entry : properties.entries()) result.names.push_back(entry.key);
  result.inputs.reserve(result.names.size());
  std::size_t index = 0;
  for (const auto &entry : properties.entries()) {
    const auto stage_field = entry.value.required("stage");
    const auto stage_name = stage_field.nonempty_string();
    ShaderStage stage;
    if (stage_name == "vertex") stage = ShaderStage::vertex;
    else if (stage_name == "fragment") stage = ShaderStage::fragment;
    else stage_field.fail(stage_field.mark(), "Unknown shader stage");
    const auto type_field = entry.value.required("type");
    const auto type_name = type_field.nonempty_string();
    MaterialInputType type;
    if (type_name == "texture_2d") type = MaterialInputType::texture_2d;
    else if (type_name == "float4") type = MaterialInputType::float4;
    else type_field.fail(type_field.mark(), "Unknown material input type");
    AutoInput semantic = AutoInput::none;
    if (const auto field = entry.value.optional("auto")) {
      const auto name = field->nonempty_string();
      if (name == auto_input_name(AutoInput::sprite_projected_origin_and_x_axis))
        semantic = AutoInput::sprite_projected_origin_and_x_axis;
      else if (name == auto_input_name(AutoInput::sprite_projected_y_axis_and_viewport))
        semantic = AutoInput::sprite_projected_y_axis_and_viewport;
      else field->fail(field->mark(), "Unknown automatic input");
    }
    for (const auto &field : entry.value.entries())
      if (field.key != "stage" && field.key != "type" && field.key != "auto")
        field.value.fail(field.value.mark(), "Unknown recipe property field");
    result.inputs.push_back({result.names[index++], stage, type,
                             semantic == AutoInput::none ? MaterialInputSource::authored : MaterialInputSource::renderer,
                             semantic});
  }
  for (const auto &entry : document.entries())
    if (entry.key != "shader" && entry.key != "properties")
      entry.value.fail(entry.value.mark(), "Unknown recipe field");
  return result;
}

LoadedMaterialRecipe load_craft_sprite_recipe(const config::AssetResolver &resolver) {
  const config::AssetPath path("assets/shared/shaders/craft_sprite.yaml");
  const auto yaml = config::detail::read_config(resolver, path);
  return decode_material_recipe(config::ConfigView(yaml, path.address()));
}

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
          input.type == MaterialInputType::texture_2d && input.source == MaterialInputSource::authored &&
          input.auto_input == AutoInput::none) {
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
                 input.source == MaterialInputSource::renderer &&
                 ((input.name == "origin_and_x_axis" && input.auto_input == AutoInput::sprite_projected_origin_and_x_axis) ||
                  (input.name == "y_axis_and_viewport" && input.auto_input == AutoInput::sprite_projected_y_axis_and_viewport))) {
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
