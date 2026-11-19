#include <filesystem>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "flop/config/decode/body.hpp"
#include "flop/config/decode/resources.hpp"
#include "flop/world/universe.hpp"

namespace {
using Catch::Matchers::ContainsSubstring;
using flop::config::AssetPath;
using flop::config::AssetResolver;
using flop::config::ConfigDecoder;
using flop::config::ConfigView;
using flop::config::load_config;
using flop::craft::ResourceFlow;
using flop::craft::ResourceTypeConfig;

std::vector<ResourceTypeConfig> decode(std::string source) {
  return ConfigDecoder<std::vector<ResourceTypeConfig>>::decode(
      ConfigView(YAML::Load(source), "assets/resources.yaml")
  );
}

} // namespace

TEST_CASE("Shipped resource types decode into owning configurations") {
  const AssetResolver resolver(std::filesystem::path(FLOP_TEST_ASSET_ROOT));
  const auto resources =
      load_config<std::vector<ResourceTypeConfig>>(resolver, AssetPath("assets/resources.yaml"));
  REQUIRE(resources.size() == 2);
  CHECK(resources[0].id == "Hydrazine");
  CHECK(resources[0].kg_per_unit == 1);
  CHECK(resources[0].flow == ResourceFlow::stage);
  CHECK(resources[1].id == "NTO");
  CHECK(resources[1].name == "N2O4");

  const auto sun = load_config<flop::world::BodyConfig>(
      resolver, AssetPath("assets/celestialbodies/Sun/Sun.yaml")
  );
  const flop::world::Universe universe({sun}, resources);
  CHECK(universe.resources().at("Hydrazine").kg_per_unit == 1);
  CHECK(universe.resources().at("NTO").name == "N2O4");
}

TEST_CASE("Resource decoder identifies invalid fields at their YAML locations") {
  CHECK_THROWS_WITH(
      decode("- {type: Fuel, name: Fuel, density: 0, flow: Stage}\n"),
      ContainsSubstring("[[0].density]")
  );
  CHECK_THROWS_WITH(
      decode("- {type: Fuel, name: Fuel, density: 1, flow: Pipe}\n"),
      ContainsSubstring("[[0].flow]")
  );
  CHECK_THROWS_WITH(
      decode(
          "- {type: Fuel, name: Fuel, density: 1, flow: None}\n"
          "- {type: Fuel, name: Fuel, density: 1, flow: Craft}\n"
      ),
      ContainsSubstring("[[1].type]")
  );
}
