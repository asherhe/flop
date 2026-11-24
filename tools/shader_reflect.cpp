// shader reflection utility for SPIR-V binaries
// extracts parameters required by the shader

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <spirv_cross.hpp>
#include <stdexcept>
#include <vector>

// TODO: use manifest to generate shader prototype
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
    std::vector<std::uint32_t> words(bytes.size() / 4);
    for (std::size_t i = 0; i < words.size(); ++i) {
      for (std::size_t j = 0; j < 4; ++j)
        words[i] |= std::uint32_t(static_cast<unsigned char>(bytes[4 * i + j])) << (8 * j);
    }
    spirv_cross::Compiler compiler(std::move(words));
    const auto resources = compiler.get_shader_resources();
    const auto print = [&](const char *kind, const auto &items) {
      for (const auto &item : items) {
        std::cout << kind << ' ' << item.name
                  << " set=" << compiler.get_decoration(item.id, spv::DecorationDescriptorSet)
                  << " binding=" << compiler.get_decoration(item.id, spv::DecorationBinding)
                  << '\n';
      }
    };
    print("uniform_buffer", resources.uniform_buffers);
    print("sampled_image", resources.separate_images);
    print("sampler", resources.separate_samplers);
    print("combined_image_sampler", resources.sampled_images);
    print("storage_image", resources.storage_images);
    print("storage_buffer", resources.storage_buffers);
    print("push_constant", resources.push_constant_buffers);
  } catch (const std::exception &error) {
    std::cerr << "shader reflection: " << error.what() << '\n';
    return 1;
  }
}
