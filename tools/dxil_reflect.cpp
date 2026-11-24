#include <cstdio>
#include <d3d12shader.h>
#include <dxcapi.h>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>

namespace {
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
    // Report shader resource types and their Direct3D registers and spaces.
    for (UINT i = 0; i < desc.BoundResources; ++i) {
      D3D12_SHADER_INPUT_BIND_DESC resource{};
      check(reflection->GetResourceBindingDesc(i, &resource), "cannot read DXIL resource binding");
      std::printf(
          "resource %s type=%u space=%u binding=%u count=%u\n",
          resource.Name,
          static_cast<unsigned>(resource.Type),
          resource.Space,
          resource.BindPoint,
          resource.BindCount
      );
    }

    // Report constant-buffer sizes and each member's compiled layout.
    for (UINT i = 0; i < desc.ConstantBuffers; ++i) {
      auto *constant_buffer = reflection->GetConstantBufferByIndex(i);
      D3D12_SHADER_BUFFER_DESC buffer_desc{};
      check(constant_buffer->GetDesc(&buffer_desc), "cannot read DXIL constant buffer");
      std::printf("buffer %s size=%u\n", buffer_desc.Name, buffer_desc.Size);
      for (UINT j = 0; j < buffer_desc.Variables; ++j) {
        auto *variable = constant_buffer->GetVariableByIndex(j);
        D3D12_SHADER_VARIABLE_DESC variable_desc{};
        D3D12_SHADER_TYPE_DESC type_desc{};
        check(variable->GetDesc(&variable_desc), "cannot read DXIL uniform");
        check(variable->GetType()->GetDesc(&type_desc), "cannot read DXIL uniform type");
        std::printf(
            "member %s offset=%u size=%u type=%u rows=%u columns=%u\n",
            variable_desc.Name,
            variable_desc.StartOffset,
            variable_desc.Size,
            static_cast<unsigned>(type_desc.Type),
            type_desc.Rows,
            type_desc.Columns
        );
      }
    }
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
