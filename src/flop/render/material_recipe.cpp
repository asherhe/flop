#include "flop/render/material_recipe.hpp"

#include <stdexcept>
#include <string>
#include <yaml-cpp/yaml.h>

#include "flop/config/load.hpp"

namespace flop::render {

LoadedMaterialRecipe decode_material_recipe(const config::ConfigView &document) {
  document.validate_unique_keys();

  LoadedMaterialRecipe result;
  result.source = document.source_address();
  result.shader = document.required("shader").nonempty_string();

  const auto properties = document.required("properties");
  for (const auto &entry : properties.entries())
    result.names.push_back(entry.key);

  result.inputs.reserve(result.names.size());
  std::size_t index = 0;
  for (const auto &entry : properties.entries()) {
    const auto stage_field = entry.value.required("stage");
    const auto stage_name = stage_field.nonempty_string();

    ShaderStage stage;
    if (stage_name == "vertex")
      stage = ShaderStage::vertex;
    else if (stage_name == "fragment")
      stage = ShaderStage::fragment;
    else
      stage_field.fail(stage_field.mark(), "Unknown shader stage");

    const auto type_field = entry.value.required("type");
    const auto type_name = type_field.nonempty_string();

    MaterialInputType type;
    if (type_name == "texture_2d")
      type = MaterialInputType::texture_2d;
    else if (type_name == "float4")
      type = MaterialInputType::float4;
    else
      type_field.fail(type_field.mark(), "Unknown material input type");

    AutoInput semantic = AutoInput::none;
    if (const auto field = entry.value.optional("auto")) {
      const auto name = field->nonempty_string();
      if (name == auto_input_name(AutoInput::sprite_projected_origin_viewport))
        semantic = AutoInput::sprite_projected_origin_viewport;
      else if (name == auto_input_name(AutoInput::sprite_projected_xy_axis))
        semantic = AutoInput::sprite_projected_xy_axis;
      else if (name == auto_input_name(AutoInput::sun_direction))
        semantic = AutoInput::sun_direction;
      else if (name == auto_input_name(AutoInput::sun_color))
        semantic = AutoInput::sun_color;
      else if (name == auto_input_name(AutoInput::planet_shine_direction_and_spread))
        semantic = AutoInput::planet_shine_direction_and_spread;
      else if (name == auto_input_name(AutoInput::planet_shine_color))
        semantic = AutoInput::planet_shine_color;
      else if (name == auto_input_name(AutoInput::local_light_texture))
        semantic = AutoInput::local_light_texture;
      else
        field->fail(field->mark(), "Unknown automatic input");

      const bool texture = semantic == AutoInput::local_light_texture;
      if (type != (texture ? MaterialInputType::texture_2d : MaterialInputType::float4))
        field->fail(field->mark(), "Automatic input has incompatible type");
      const bool sun = semantic == AutoInput::sun_direction || semantic == AutoInput::sun_color ||
          semantic == AutoInput::planet_shine_direction_and_spread || semantic == AutoInput::planet_shine_color || texture;
      if ((sun && stage != ShaderStage::fragment) || (!sun && stage != ShaderStage::vertex))
        field->fail(field->mark(), "Automatic input has incompatible shader stage");
    }
    for (const auto &field : entry.value.entries())
      if (field.key != "stage" && field.key != "type" && field.key != "auto")
        field.value.fail(field.value.mark(), "Unknown recipe property field");
    result.inputs.push_back({
        result.names[index++],
        stage,
        type,
        semantic == AutoInput::none ? MaterialInputSource::authored : MaterialInputSource::renderer,
        semantic,
    });
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

LoadedMaterialRecipe load_craft_lit_sprite_recipe(const config::AssetResolver &resolver) {
  const config::AssetPath path("assets/shared/shaders/craft_lit_sprite.yaml");
  const auto yaml = config::detail::read_config(resolver, path);
  return decode_material_recipe(config::ConfigView(yaml, path.address()));
}

namespace {
struct ReflectedStage {
  YAML::Node data;
  std::string_view format;

  unsigned texture(std::string_view name) const {
    const auto items = format == "dxil" ? data["resources"] : data["textures"];
    unsigned count = 0, slot = 0;
    for (const auto &item : items) {
      if (item["name"].as<std::string>() == name &&
          (format != "dxil" || item["type"].as<std::string>() == "texture")) {
        slot = item["binding"].as<unsigned>();
        ++count;
      }
    }
    if (count != 1)
      throw std::runtime_error("missing or duplicate texture " + std::string(name));
    return slot;
  }

  YAML::Node buffer() const {
    const auto buffers = data["uniform_buffers"];
    if (!buffers.IsSequence() || buffers.size() != 1)
      throw std::runtime_error("expected one uniform buffer");
    return buffers[0];
  }

  unsigned buffer_slot(const YAML::Node &buffer) const {
    if (format == "spirv")
      return buffer["binding"].as<unsigned>();
    unsigned count = 0, slot = 0;
    for (const auto &item : data["resources"]) {
      if (item["type"].as<std::string>() == "uniform_buffer" &&
          item["name"].as<std::string>() == buffer["name"].as<std::string>()) {
        slot = item["binding"].as<unsigned>();
        ++count;
      }
    }
    if (count != 1)
      throw std::runtime_error("missing or duplicate uniform buffer binding");
    return slot;
  }

  unsigned member(const YAML::Node &buffer, std::string_view name) const {
    const auto members = buffer["members"];
    if (!members.IsSequence())
      throw std::runtime_error("expected uniform members");
    unsigned count = 0, offset = 0;
    for (const auto &item : members) {
      if (item["name"].as<std::string>() != name)
        continue;
      if (item["type"].as<std::string>() != "float" || item["components"].as<unsigned>() != 4 ||
          item["size"].as<unsigned>() != 16)
        throw std::runtime_error("incompatible uniform " + std::string(name));
      offset = item["offset"].as<unsigned>();
      ++count;
    }
    if (count != 1)
      throw std::runtime_error("missing or duplicate uniform " + std::string(name));
    return offset;
  }
};

struct ResolvedInputs {
  CraftSpriteBindings geometry{};
  unsigned normal{}, emission{}, local_light{}, light_slot{}, light_size{}, direction{}, color{}, shine_direction{}, shine_color{};
};

ResolvedInputs resolve_inputs(
    const MaterialRecipe &recipe,
    const shaders::Entry &vertex,
    const shaders::Entry &fragment,
    bool lit
) {
  try {
    if (vertex.id != recipe.shader || fragment.id != recipe.shader || vertex.stage != "vertex" ||
        fragment.stage != "fragment" || vertex.format != fragment.format ||
        (vertex.format != "spirv" && vertex.format != "dxil"))
      throw std::runtime_error("incompatible shader variants");
    if (recipe.inputs.size() != (lit ? 10u : 3u))
      throw std::runtime_error("unexpected recipe input count");
    const ReflectedStage v{YAML::Load(std::string(vertex.reflection_json)), vertex.format};
    const ReflectedStage f{YAML::Load(std::string(fragment.reflection_json)), fragment.format};
    if (v.data["stage"].as<std::string>() != "vertex" ||
        f.data["stage"].as<std::string>() != "fragment")
      throw std::runtime_error("reflection stage mismatch");
    const auto geometry = v.buffer();
    const auto light = lit ? f.buffer() : YAML::Node{};
    ResolvedInputs result;
    result.geometry.uniform_size = geometry["size"].as<unsigned>();
    result.geometry.uniform_slot = v.buffer_slot(geometry);
    if (lit) {
      result.light_size = light["size"].as<unsigned>();
      result.light_slot = f.buffer_slot(light);
    }
    unsigned matched = 0;
    for (const auto &input : recipe.inputs) {
      unsigned bit = 0;
      if (input.stage == ShaderStage::fragment && input.source == MaterialInputSource::authored &&
          input.type == MaterialInputType::texture_2d && input.auto_input == AutoInput::none) {
        if (input.name == "albedo") {
          bit = 1;
          result.geometry.texture_slot = f.texture(input.name);
        } else if (lit && input.name == "normal_map") {
          bit = 8;
          result.normal = f.texture(input.name);
        } else if (lit && input.name == "emission_map") {
          bit = 16;
          result.emission = f.texture(input.name);
        }
      } else if (lit && input.stage == ShaderStage::fragment &&
                 input.source == MaterialInputSource::renderer &&
                 input.type == MaterialInputType::texture_2d && input.name == "local_light" &&
                 input.auto_input == AutoInput::local_light_texture) {
        bit = 512;
        result.local_light = f.texture(input.name);
      } else if (
          input.source == MaterialInputSource::renderer && input.type == MaterialInputType::float4
      ) {
        if (input.stage == ShaderStage::vertex && input.name == "origin_viewport" &&
            input.auto_input == AutoInput::sprite_projected_origin_viewport) {
          bit = 2;
          result.geometry.origin_viewport_offset = v.member(geometry, input.name);
        } else if (
            input.stage == ShaderStage::vertex && input.name == "xy_axis" &&
            input.auto_input == AutoInput::sprite_projected_xy_axis
        ) {
          bit = 4;
          result.geometry.xy_axis_offset = v.member(geometry, input.name);
        } else if (
            lit && input.stage == ShaderStage::fragment && input.name == "sun_direction" &&
            input.auto_input == AutoInput::sun_direction
        ) {
          bit = 32;
          result.direction = f.member(light, input.name);
        } else if (
            lit && input.stage == ShaderStage::fragment && input.name == "sun_color" &&
            input.auto_input == AutoInput::sun_color
        ) {
          bit = 64;
          result.color = f.member(light, input.name);
        } else if (lit && input.stage == ShaderStage::fragment && input.name == "planet_shine_direction_and_spread" &&
                   input.auto_input == AutoInput::planet_shine_direction_and_spread) {
          bit = 128;
          result.shine_direction = f.member(light, input.name);
        } else if (lit && input.stage == ShaderStage::fragment && input.name == "planet_shine_color" &&
                   input.auto_input == AutoInput::planet_shine_color) {
          bit = 256;
          result.shine_color = f.member(light, input.name);
        }
      }
      if (!bit || (matched & bit))
        throw std::runtime_error(
            "unknown, duplicate or incompatible input " + std::string(input.name)
        );
      matched |= bit;
    }
    if (matched != (lit ? 1023u : 7u))
      throw std::runtime_error("missing recipe input");
    if (result.geometry.uniform_size != 32 || result.geometry.origin_viewport_offset != 0 ||
        result.geometry.xy_axis_offset != 16)
      throw std::runtime_error("uniform layout does not match SpriteGeometry");
    if (lit && (result.light_size != 64 || result.direction != 0 || result.color != 16 ||
                result.shine_direction != 32 || result.shine_color != 48))
      throw std::runtime_error("uniform layout does not match sprite lighting");
    return result;
  } catch (const std::exception &error) {
    throw std::runtime_error(
        (recipe.source.empty() ? std::string{} : std::string(recipe.source) + ": ") +
        "material '" + std::string(recipe.shader) + "' (" + std::string(vertex.format) +
        "): " + error.what()
    );
  }
}
} // namespace

CraftSpriteBindings resolve_craft_sprite_bindings(
    const MaterialRecipe &recipe, const shaders::Entry &vertex, const shaders::Entry &fragment
) {
  return resolve_inputs(recipe, vertex, fragment, false).geometry;
}

CraftLitSpriteBindings resolve_craft_lit_sprite_bindings(
    const MaterialRecipe &recipe, const shaders::Entry &vertex, const shaders::Entry &fragment
) {
  if (recipe.shader != "craft/lit_sprite")
    throw std::runtime_error("unexpected lit sprite shader");
  const auto inputs = resolve_inputs(recipe, vertex, fragment, true);
  return {
      inputs.geometry,
      inputs.normal,
      inputs.emission,
      inputs.local_light,
      inputs.light_slot,
      inputs.light_size,
      inputs.direction,
      inputs.color,
      inputs.shine_direction,
      inputs.shine_color
  };
}
} // namespace flop::render
