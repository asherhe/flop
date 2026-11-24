#include <catch2/catch_test_macros.hpp>

#include <string>

#include "flop/render/shader_library.hpp"

namespace {
using namespace flop::render::shaders;
constexpr unsigned char vertex_bytes[] = {1, 2};
constexpr unsigned char fragment_bytes[] = {3};
constexpr Entry mock_manifest[] = {
    {"mock.program", "vertex", "spirv", "vertex", vertex_bytes, sizeof(vertex_bytes), 0, 0, 0, 0},
    {"mock.program", "fragment", "spirv", "fragment", fragment_bytes, sizeof(fragment_bytes), 0, 0, 0, 0},
    {"mock.program", "fragment", "dxil", "fragment", fragment_bytes, sizeof(fragment_bytes), 0, 0, 0, 0},
    {"other.program", "vertex", "dxil", "vertex", vertex_bytes, sizeof(vertex_bytes), 0, 0, 0, 0},
};
} // namespace

TEST_CASE("Shader library selects entries by ID, stage, and format") {
  REQUIRE(contains(mock_manifest, "mock.program", "vertex", "spirv"));
  REQUIRE_FALSE(contains(mock_manifest, "mock.program", "vertex", "dxil"));
  const auto &entry = find(mock_manifest, "mock.program", "fragment", "dxil");
  REQUIRE(entry.code == fragment_bytes);
  REQUIRE(entry.entrypoint == "fragment");
}

TEST_CASE("Shader library reports the formats available for a missing variant") {
  try {
    (void)find(mock_manifest, "mock.program", "fragment", "metal");
    FAIL("Expected missing shader variant");
  } catch (const std::runtime_error &error) {
    const std::string message = error.what();
    REQUIRE(message.find("mock.program") != std::string::npos);
    REQUIRE(message.find("fragment") != std::string::npos);
    REQUIRE(message.find("metal") != std::string::npos);
    REQUIRE(message.find("spirv") != std::string::npos);
    REQUIRE(message.find("dxil") != std::string::npos);
  }
}
