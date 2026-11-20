// Save loading: a save states every craft in full, including a cartesian orbit
// seed and live resource balances. load_save reconstructs the placed instances
// against the game's fixed part types and refuses a craft that omits its orbit
// (a template cannot be placed). This replaces the earlier scenario-manifest
// approach: the universe (bodies/resources/part types) is fixed by the game
// build, and the save carries only the mutable craft state.

#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <system_error>
#include <vector>
#include <yaml-cpp/yaml.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "flop/config/decode/part.hpp"
#include "flop/config/decode/save.hpp"
#include "flop/config/load.hpp"
#include "flop/craft/config.hpp"
#include "flop/craft/parts/config.hpp"

namespace {

using Catch::Approx;
using Catch::Matchers::ContainsSubstring;
using flop::config::AssetPath;
using flop::config::AssetResolver;
using flop::config::load_part_type;
using flop::config::load_save;
using flop::craft::PartTypeConfig;

const std::filesystem::path ASSET_ROOT{FLOP_TEST_ASSET_ROOT};

std::vector<PartTypeConfig> test_part_types(const AssetResolver &resolver) {
  return {
      load_part_type(resolver, "TestingCommandPod"),
      load_part_type(resolver, "TestingTank"),
      load_part_type(resolver, "TestingEngine"),
  };
}

} // namespace

TEST_CASE("The shipped save decodes its placed craft with cartesian state and live balances") {
  const AssetResolver resolver(ASSET_ROOT);
  const auto types = test_part_types(resolver);
  const auto saved = load_save(resolver, AssetPath("assets/saves/orbital_test.yaml"), types);

  REQUIRE(saved.size() == 1);
  const auto &craft = saved.front();
  CHECK(craft.name == "Orbital Test Craft");
  CHECK(craft.plugins_resolved);
  REQUIRE(craft.orbit.has_value());
  CHECK(craft.orbit->parent_name == "Hamswell");
  CHECK(craft.orbit->state.position.x == Approx(-1775017.642566629));
  CHECK(craft.orbit->state.position.y == Approx(2763005.5625398336));
  CHECK(craft.orbit->state.velocity.x == Approx(-4632.602307965213));
  CHECK(craft.orbit->state.velocity.y == Approx(-2535.239056863811));
  CHECK(craft.orbit->epoch_sec == 0);
  CHECK(craft.initial_attitude.angle_rad == Approx(2.7));

  REQUIRE(craft.parts.size() == 3);
  const auto &tank_plugins = craft.parts[1].plugins;
  const auto tank = std::ranges::find(
      tank_plugins, "ResourceContainerPlugin", &flop::craft::plugin::PartPluginConfig::name
  );
  REQUIRE(tank != tank_plugins.end());
  const auto &balance = std::get<flop::craft::plugin::ResourceConfig>(tank->values);
  CHECK(balance.amount_units.at("NTO") == 1800);
  CHECK(balance.amount_units.at("Hydrazine") == 1200);
}

TEST_CASE("A save rejects a craft that omits its orbit") {
  const AssetResolver resolver(ASSET_ROOT);
  const auto types = test_part_types(resolver);

  // write a save whose single craft has no orbit block, i.e. a bare template.
  const auto temp = std::filesystem::temp_directory_path() /
                    ("flop-save-test-" + std::to_string(std::random_device{}()));
  std::filesystem::create_directories(temp / "assets");
  {
    std::ofstream out(temp / "assets" / "no_orbit.yaml", std::ios::binary);
    out << "craft:\n"
           "  - name: Templated\n"
           "    rotation: {angle: 0, momentum: 0}\n"
           "    parts:\n"
           "      - {type: TestingCommandPod, id: pod_1, name: Pod, "
           "transform: {pos: {x: 0, y: 0}, rot: 0}, attachments: [], plugins: {}}\n";
  }
  const AssetResolver temp_resolver(temp / "assets");
  CHECK_THROWS_WITH(
      load_save(temp_resolver, AssetPath("assets/no_orbit.yaml"), types),
      ContainsSubstring("must state its orbit")
  );
  std::error_code error;
  std::filesystem::remove_all(temp, error);
}
