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

namespace {
std::string quote(const std::string &s) {
  std::string out = "\"";
  for (const unsigned char c : s) {
    if (c < 32)
      throw std::runtime_error("control character in reflected name");
    if (c == '\\' || c == '"')
      out += '\\';
    out += static_cast<char>(c);
  }
  return out + '"';
}

std::string name(const std::string &s) {
  if (s.empty())
    throw std::runtime_error("reflection name missing");
  return quote(s);
}

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
    std::cerr << "usage: flop_shader_reflect <shader.spirv>\n";
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
        !resources.sampled_images.empty() || !resources.push_constant_buffers.empty() ||
        !resources.subpass_inputs.empty() || !resources.atomic_counters.empty())
      throw std::runtime_error("unsupported resource type");

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
    const auto images = slots(resources.separate_images, resource_set);
    const auto samplers = slots(resources.separate_samplers, resource_set);
    if (images != samplers)
      throw std::runtime_error("texture and sampler bindings must match");

    // TODO: DXC emits separate SPIR-V textures and samplers. further testing is needed to determine
    // if it works on SDL/Vulcan
    std::string json = "{\"version\":1,\"binding_validation\":\"partial\",\"stage\":" +
                       quote(model == spv::ExecutionModelVertex ? "vertex" : "fragment") +
                       ",\"entrypoint\":" + name(entries.front().name) + ",\"uniform_buffers\":[";
    bool first = true;
    for (const auto &buffer : resources.uniform_buffers) {
      if (!first)
        json += ',';
      first = false;
      const auto &type = compiler.get_type(buffer.base_type_id);
      json += "{\"name\":" + name(buffer.name) + ",\"binding\":" +
              std::to_string(compiler.get_decoration(buffer.id, spv::DecorationBinding)) +
              ",\"size\":" + std::to_string(compiler.get_declared_struct_size(type)) +
              ",\"members\":[";
      for (unsigned i = 0; i < type.member_types.size(); ++i) {
        if (i)
          json += ',';
        const auto &member = compiler.get_type(type.member_types[i]);
        json += "{\"name\":" + name(compiler.get_member_name(type.self, i)) +
                ",\"type\":" + quote(scalar_type(member)) +
                ",\"components\":" + std::to_string(member.vecsize) +
                ",\"offset\":" + std::to_string(compiler.type_struct_member_offset(type, i)) +
                ",\"size\":" + std::to_string(compiler.get_declared_struct_member_size(type, i)) +
                '}';
      }
      json += "]}";
    }
    json += "],\"textures\":[";
    first = true;
    for (const auto &item : resources.separate_images) {
      if (!first)
        json += ',';
      first = false;
      json += "{\"name\":" + name(item.name) + ",\"binding\":" +
              std::to_string(compiler.get_decoration(item.id, spv::DecorationBinding)) + '}';
    }
    json += "]}";
    std::cout << json << '\n';
  } catch (const std::exception &error) {
    std::cerr << "shader reflection (" << argv[1] << "): " << error.what() << '\n';
    return 1;
  }
}
