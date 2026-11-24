#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <spirv_cross.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "json_writer.hpp"

namespace {
using flop::tools::json::ShaderReflection;
using flop::tools::json::UniformBuffer;

std::string scalar_type(const spirv_cross::SPIRType &t) {
  if (!t.array.empty() || t.columns != 1 || t.vecsize > 4 || t.width != 32)
    throw std::runtime_error(
        "unsupported uniform type: only 32-bit scalar and vector types supported"
    );
  switch (t.basetype) {
  case spirv_cross::SPIRType::Float: return "float";
  case spirv_cross::SPIRType::Int: return "int";
  case spirv_cross::SPIRType::UInt: return "uint";
  default: throw std::runtime_error("unsupported uniform scalar type");
  }
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: flop_spirv_reflect <shader.spirv>\n";
    return 2;
  }
  try {
    std::ifstream input(argv[1], std::ios::binary);
    if (!input)
      throw std::runtime_error("cannot open SPIR-V file");

    const std::vector<char> bytes{std::istreambuf_iterator<char>(input), {}};
    if (bytes.empty() || bytes.size() % 4 != 0)
      throw std::runtime_error("invalid SPIR-V size");

    // merge into four-byte words
    std::vector<std::uint32_t> words(bytes.size() / 4);
    for (std::size_t i = 0; i < words.size(); ++i)
      for (std::size_t j = 0; j < 4; ++j)
        words[i] |= std::uint32_t(static_cast<unsigned char>(bytes[4 * i + j])) << (8 * j);

    spirv_cross::Compiler compiler(std::move(words));
    const auto entries = compiler.get_entry_points_and_stages();
    if (entries.size() != 1)
      throw std::runtime_error("expected exactly one entry point");
    const auto model = entries.front().execution_model;
    if (model != spv::ExecutionModelVertex && model != spv::ExecutionModelFragment)
      throw std::runtime_error("unsupported shader stage");

    const unsigned resource_set = model == spv::ExecutionModelVertex ? 0 : 2;
    const unsigned uniform_set = resource_set + 1;
    const auto resources = compiler.get_shader_resources(compiler.get_active_interface_variables());
    if (!resources.storage_buffers.empty() || !resources.storage_images.empty() ||
        !resources.push_constant_buffers.empty() || !resources.subpass_inputs.empty() ||
        !resources.atomic_counters.empty())
      throw std::runtime_error("unsupported resource type");
    if (!resources.separate_images.empty() || !resources.separate_samplers.empty())
      throw std::runtime_error(
          "separate textures and samplers are not supported; use combined image sampler bindings"
      );

    auto slots = [&](const auto &items, unsigned expected_set) {
      std::set<unsigned> result;
      for (const auto &item : items) {
        if (!compiler.has_decoration(item.id, spv::DecorationDescriptorSet) ||
            !compiler.has_decoration(item.id, spv::DecorationBinding) ||
            compiler.get_decoration(item.id, spv::DecorationDescriptorSet) != expected_set)
          throw std::runtime_error(
              "resource '" + item.name + "' has invalid descriptor set or binding"
          );
        if (!compiler.get_type(item.type_id).array.empty())
          throw std::runtime_error("resource arrays are not supported");
        const auto binding = compiler.get_decoration(item.id, spv::DecorationBinding);
        if (!result.insert(binding).second)
          throw std::runtime_error("duplicate resource binding");
      }
      unsigned i = 0;
      for (const auto binding : result)
        if (binding != i++)
          throw std::runtime_error("resource bindings must be consecutive from zero");
      return result;
    };

    slots(resources.uniform_buffers, uniform_set);
    slots(resources.sampled_images, resource_set);
    for (const auto &image : resources.sampled_images) {
      const auto &type = compiler.get_type(image.type_id);
      if (type.image.dim != spv::Dim2D || type.image.arrayed || type.image.ms || type.image.depth)
        throw std::runtime_error("only non-arrayed, non-multisampled 2D color textures are supported");
      const auto &sampled = compiler.get_type(type.image.type);
      if (sampled.basetype != spirv_cross::SPIRType::Float || sampled.width != 32)
        throw std::runtime_error("only 32-bit floating-point texture samples are supported");
    }

    ShaderReflection metadata{ShaderReflection::Format::spirv};
    metadata.stage = model == spv::ExecutionModelVertex ? "vertex" : "fragment";
    metadata.entrypoint = entries.front().name;
    for (const auto &buffer : resources.uniform_buffers) {
      const auto &type = compiler.get_type(buffer.base_type_id);
      UniformBuffer reflected{
          buffer.name,
          compiler.get_decoration(buffer.id, spv::DecorationBinding),
          compiler.get_declared_struct_size(type),
          {}
      };
      for (unsigned i = 0; i < type.member_types.size(); ++i) {
        const auto &member = compiler.get_type(type.member_types[i]);
        reflected.members.push_back(
            {compiler.get_member_name(type.self, i),
             scalar_type(member),
             member.vecsize,
             compiler.type_struct_member_offset(type, i),
             compiler.get_declared_struct_member_size(type, i)}
        );
      }
      metadata.uniform_buffers.push_back(std::move(reflected));
    }
    for (const auto &item : resources.sampled_images) {
      metadata.textures.push_back(
          {item.name, compiler.get_decoration(item.id, spv::DecorationBinding)}
      );
    }
    std::cout << flop::tools::json::emit(metadata) << '\n';
  } catch (const std::exception &error) {
    std::cerr << "shader reflection (" << argv[1] << "): " << error.what() << '\n';
    return 1;
  }
}
