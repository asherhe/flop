#include <cstdio>
#include <d3d12shader.h>
#include <dxcapi.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>

#include "json_writer.hpp"

namespace {
using flop::tools::json::required_name;
using flop::tools::json::ShaderReflection;
using flop::tools::json::UniformBuffer;
// Explicit interface IDs also work with toolchains that do not implement __uuidof.
constexpr GUID dxc_utils_id = {
    0x6245d6af, 0x66e0, 0x48fd, {0x80, 0xb4, 0x4d, 0x27, 0x17, 0x96, 0x74, 0x8c}
};
constexpr GUID dxc_utils_interface = {
    0x4605c4cb, 0x2019, 0x492a, {0xad, 0xa4, 0x65, 0xf2, 0x0b, 0xb7, 0xd6, 0x7f}
};
constexpr GUID reflection_interface = {
    0x5a58797d, 0xa72c, 0x478d, {0x8b, 0xa2, 0xef, 0xc6, 0xb0, 0xef, 0xe8, 0x8e}
};

void check(HRESULT result, const char *operation) {
  if (FAILED(result))
    throw std::runtime_error(operation);
}

const char *uniform_type(const D3D12_SHADER_TYPE_DESC &type) {
  if ((type.Class != D3D_SVC_SCALAR && type.Class != D3D_SVC_VECTOR) || type.Elements != 0 ||
      type.Rows != 1 || type.Columns < 1 || type.Columns > 4)
    throw std::runtime_error(
        "unsupported uniform type: only 32-bit scalar and vector types supported"
    );
  switch (type.Type) {
  case D3D_SVT_FLOAT: return "float";
  case D3D_SVT_INT: return "int";
  case D3D_SVT_UINT: return "uint";
  default: throw std::runtime_error("unsupported uniform scalar type");
  }
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    std::fprintf(stderr, "usage: flop_dxil_reflect <dxcompiler.dll> <shader.dxil>\n");
    return 2;
  }

  HMODULE library = nullptr;
  IDxcUtils *utils = nullptr;
  ID3D12ShaderReflection *reflection = nullptr;
  int status = 1;
  try {
    // Read the compiled DXIL container, not the HLSL source.
    std::ifstream input(argv[2], std::ios::binary);
    if (!input)
      throw std::runtime_error("cannot open DXIL file");
    const std::vector<char> bytes{std::istreambuf_iterator<char>(input), {}};
    if (bytes.empty())
      throw std::runtime_error("DXIL file is empty");
    // Load the pinned DXC library supplied by the build; it stays host-side.
    library = LoadLibraryA(argv[1]);
    if (!library)
      throw std::runtime_error("cannot load dxcompiler.dll");
    const auto create =
        reinterpret_cast<DxcCreateInstanceProc>(GetProcAddress(library, "DxcCreateInstance"));
    if (!create)
      throw std::runtime_error("cannot find DxcCreateInstance");
    check(
        create(dxc_utils_id, dxc_utils_interface, reinterpret_cast<void **>(&utils)),
        "cannot create DXC utils"
    );
    // Obtain DXC's reflection interface for this container.
    const DxcBuffer buffer{bytes.data(), bytes.size(), 0};
    check(
        utils->CreateReflection(
            &buffer, reflection_interface, reinterpret_cast<void **>(&reflection)
        ),
        "cannot reflect DXIL"
    );
    D3D12_SHADER_DESC desc{};
    check(reflection->GetDesc(&desc), "cannot read DXIL shader description");
    const auto shader_type = D3D12_SHVER_GET_TYPE(desc.Version);
    if (shader_type != D3D12_SHVER_VERTEX_SHADER && shader_type != D3D12_SHVER_PIXEL_SHADER)
      throw std::runtime_error("unsupported shader stage");
    const unsigned resource_space = shader_type == D3D12_SHVER_VERTEX_SHADER ? 0 : 2;
    const unsigned uniform_space = resource_space + 1;
    ShaderReflection metadata{ShaderReflection::Format::dxil};
    metadata.stage = shader_type == D3D12_SHVER_VERTEX_SHADER ? "vertex" : "fragment";

    // Report shader resource types and their Direct3D registers and spaces.
    for (UINT i = 0; i < desc.BoundResources; ++i) {
      D3D12_SHADER_INPUT_BIND_DESC resource{};
      check(reflection->GetResourceBindingDesc(i, &resource), "cannot read DXIL resource binding");
      const char *kind = nullptr;
      if (resource.Type == D3D_SIT_CBUFFER && resource.Space == uniform_space)
        kind = "uniform_buffer";
      else if (resource.Type == D3D_SIT_TEXTURE && resource.Space == resource_space)
        kind = "texture";
      else if (resource.Type == D3D_SIT_SAMPLER && resource.Space == resource_space)
        kind = "sampler";
      else
        throw std::runtime_error("unsupported resource type or register space");
      if (resource.BindCount != 1)
        throw std::runtime_error("resource arrays are not supported");
      metadata.resources.push_back(
          {required_name(resource.Name), kind, resource.Space, resource.BindPoint}
      );
    }

    // Report constant-buffer sizes and each member's compiled layout.
    for (UINT i = 0; i < desc.ConstantBuffers; ++i) {
      auto *constant_buffer = reflection->GetConstantBufferByIndex(i);
      D3D12_SHADER_BUFFER_DESC buffer_desc{};
      check(constant_buffer->GetDesc(&buffer_desc), "cannot read DXIL constant buffer");
      UniformBuffer reflected{required_name(buffer_desc.Name), std::nullopt, buffer_desc.Size, {}};
      for (UINT j = 0; j < buffer_desc.Variables; ++j) {
        auto *variable = constant_buffer->GetVariableByIndex(j);
        D3D12_SHADER_VARIABLE_DESC variable_desc{};
        D3D12_SHADER_TYPE_DESC type_desc{};
        check(variable->GetDesc(&variable_desc), "cannot read DXIL uniform");
        check(variable->GetType()->GetDesc(&type_desc), "cannot read DXIL uniform type");
        reflected.members.push_back(
            {required_name(variable_desc.Name),
             uniform_type(type_desc),
             type_desc.Columns,
             variable_desc.StartOffset,
             variable_desc.Size}
        );
      }
      metadata.uniform_buffers.push_back(std::move(reflected));
    }
    std::cout << flop::tools::json::emit(metadata) << '\n';
    status = 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "DXIL reflection: %s\n", error.what());
  }

  // Release COM interfaces before unloading the library that implements them.
  if (reflection)
    reflection->Release();
  if (utils)
    utils->Release();
  if (library)
    FreeLibrary(library);
  return status;
}
