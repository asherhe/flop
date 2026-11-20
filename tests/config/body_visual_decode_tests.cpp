// BodyVisualConfig decode: the visual side of a body (color, surface material,
// atmosphere, planet shine) is retained at the file boundary, separate from the
// orbital world::BodyConfig. These load the shipped Hamswell and Sun bodies and
// check units, material property keys, and the atmosphere/no-atmosphere split.

#include <filesystem>
#include <string>
#include <yaml-cpp/yaml.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "flop/config/decode/body_visual.hpp"

namespace {

using Catch::Approx;
using Catch::Matchers::ContainsSubstring;
using flop::config::AssetPath;
using flop::config::AssetResolver;
using flop::config::BodyVisualConfig;
using flop::config::ConfigDecoder;
using flop::config::ConfigView;
using flop::config::load_config;

BodyVisualConfig decode(std::string source) {
  return ConfigDecoder<BodyVisualConfig>::decode(
      ConfigView(YAML::Load(source), "assets/body.yaml")
  );
}

} // namespace

TEST_CASE("Hamswell decodes its surface textures, atmosphere, and planet shine") {
  const AssetResolver resolver(std::filesystem::path(FLOP_TEST_ASSET_ROOT));
  const auto visual = load_config<BodyVisualConfig>(
      resolver, AssetPath("assets/celestialbodies/Hamswell/Hamswell.yaml")
  );

  CHECK(visual.name == "Hamswell");
  CHECK(visual.color == "#729DE0");

  const auto &surface = visual.surface_material.properties;
  CHECK(surface.at("_MainTex").value == "assets/celestialbodies/Hamswell/Hamswell.png");
  CHECK(surface.at("_SpecularTex").value == "assets/celestialbodies/Hamswell/Hamswell-Specular.png");
  CHECK(surface.at("_NormalMap").value == "assets/celestialbodies/Hamswell/Hamswell-Normal.png");
  CHECK(surface.at("_LightTex").value == "assets/celestialbodies/Hamswell/Hamswell-Lights.png");
  CHECK(surface.at("_HasAtmosphere").type == "Integer");

  REQUIRE(visual.atmosphere.has_value());
  // authored in kilometres, retained in metres.
  CHECK(visual.atmosphere->height_meters == Approx(80000.0));
  CHECK(visual.atmosphere->scale_height_meters == Approx(5000.0));
  CHECK(visual.atmosphere->sea_level_pressure_atm == Approx(1.0));
  const auto &scatter = visual.atmosphere->material.properties;
  CHECK(scatter.at("_MiePhaseG").value == "0.6");
  CHECK(scatter.at("_RayleighScaleHeight").value == "5000");

  REQUIRE(visual.planet_shine.has_value());
  CHECK(visual.planet_shine->color == "#d3e7ff");
  CHECK(visual.planet_shine->intensity == Approx(1.0));
}

TEST_CASE("The Sun decodes a surface material but no atmosphere") {
  const AssetResolver resolver(std::filesystem::path(FLOP_TEST_ASSET_ROOT));
  const auto visual = load_config<BodyVisualConfig>(
      resolver, AssetPath("assets/celestialbodies/Sun/Sun.yaml")
  );

  CHECK(visual.name == "Sun");
  CHECK(visual.color == "#EDDC4F");
  CHECK(visual.surface_material.path == "assets/shared/materials/surfaceblank.mat");
  CHECK_FALSE(visual.atmosphere.has_value());
  CHECK_FALSE(visual.planet_shine.has_value());
}

TEST_CASE("A truthy non-mapping atmosphere is rejected at its field") {
  CHECK_THROWS_WITH(
      decode(
          "name: Bad\ncolor: '#fff'\n"
          "surface_material: {path: assets/shared/materials/surface.mat}\n"
          "atmosphere: true\n"
      ),
      ContainsSubstring("[atmosphere]") && ContainsSubstring("mapping or false")
  );
}

TEST_CASE("A missing required visual field reports its path") {
  CHECK_THROWS_WITH(
      decode("name: Bad\natmosphere: false\n"),
      ContainsSubstring("[color]")
  );
}
