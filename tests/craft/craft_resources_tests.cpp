#include <limits>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "flop/craft/craft_resources.hpp"
#include "flop/craft/part.hpp"
#include "flop/craft/part_plugin_config.hpp"
#include "flop/craft/resource_config.hpp"

#include "support/stub_universe_context.hpp"

namespace {
using Catch::Matchers::ContainsSubstring;
using flop::craft::ContainerState;
using flop::craft::CraftResources;
using flop::craft::Part;
using flop::craft::PartPluginConfig;
using flop::craft::ResourceConfig;
using flop::craft::ResourceContainerConfig;
using flop::craft::ResourceFlow;
using flop::craft::ResourceSlot;
using flop::testing::StubUniverseContext;

flop::orbit::RefFrameManager frames() {
  return flop::orbit::RefFrameManager(
      {{"Hamswell", 8.82e13, {}, {}, std::numeric_limits<double>::infinity()}}
  );
}

StubUniverseContext context() {
  return StubUniverseContext(
      frames(),
      {{"NTO", "N2O4", 1.0, ResourceFlow::stage}, {"Fuel", "Hydrazine", 2.0, ResourceFlow::stage}}
  );
}

PartPluginConfig container_plugin(ResourceContainerConfig config, bool enabled = true) {
  return {"ResourceContainerPlugin", enabled, std::move(config)};
}

Part tank(std::string id, ResourceContainerConfig config, bool enabled = true) {
  Part part;
  part.type_id = "Tank";
  part.id = std::move(id);
  part.plugins.push_back(container_plugin(std::move(config), enabled));
  return part;
}

const ResourceSlot &slot(const ContainerState &container, std::string_view resource_id) {
  const auto found = std::ranges::find(container.slots, resource_id, &ResourceSlot::resource_id);
  REQUIRE(found != container.slots.end());
  return *found;
}
} // namespace

TEST_CASE("CraftResources builds one container per installed container plugin") {
  const std::vector<Part> parts{
      tank("tank_1", {{{"NTO", 3600}}, {{"NTO", 1800}}, {{"NTO", 2}}}),
      tank("tank_2", {{{"Fuel", 1000}}, {{"Fuel", 500}}, {}}),
  };
  const auto ctx = context();
  const CraftResources resources(parts, ctx);

  REQUIRE(resources.containers().size() == 2);
  CHECK(resources.containers()[0].part_index == 0);
  CHECK(resources.containers()[0].enabled);
  CHECK(slot(resources.containers()[0], "NTO").amount_units == 1800);
  CHECK(slot(resources.containers()[0], "NTO").max_amount_units == 3600);
  CHECK(slot(resources.containers()[0], "NTO").priority == 2);
  CHECK(resources.containers()[1].part_index == 1);
  CHECK(slot(resources.containers()[1], "Fuel").amount_units == 500);
}

TEST_CASE("CraftResources defaults omitted starting amount and priority to zero") {
  const std::vector<Part> parts{tank("tank_1", {{{"NTO", 3600}}, {}, {}})};
  const auto ctx = context();
  const CraftResources resources(parts, ctx);

  const auto &nto = slot(resources.containers().front(), "NTO");
  CHECK(nto.amount_units == 0);
  CHECK(nto.priority == 0);
  CHECK(nto.max_amount_units == 3600);
}

TEST_CASE("CraftResources clamps starting amounts into the capacity range") {
  const std::vector<Part> parts{
      tank("tank_1", {{{"NTO", 3600}, {"Fuel", 1000}}, {{"NTO", 5000}, {"Fuel", -200}}, {}}),
  };
  const auto ctx = context();
  const CraftResources resources(parts, ctx);

  CHECK(slot(resources.containers().front(), "NTO").amount_units == 3600);
  CHECK(slot(resources.containers().front(), "Fuel").amount_units == 0);
}

TEST_CASE("CraftResources records the owning plugin enabled flag") {
  const std::vector<Part> parts{tank("tank_1", {{{"NTO", 3600}}, {{"NTO", 100}}, {}}, false)};
  const auto ctx = context();
  const CraftResources resources(parts, ctx);
  CHECK_FALSE(resources.containers().front().enabled);
}

TEST_CASE("CraftResources rejects unknown resource ids and non-finite capacities") {
  const auto ctx = context();

  const std::vector<Part> unknown{tank("tank_1", {{{"Xenon", 10}}, {}, {}})};
  CHECK_THROWS_WITH(CraftResources(unknown, ctx), ContainsSubstring("Unknown resource ID"));

  const std::vector<Part> infinite_capacity{
      tank("tank_1", {{{"NTO", std::numeric_limits<double>::infinity()}}, {}, {}})
  };
  CHECK_THROWS_WITH(
      CraftResources(infinite_capacity, ctx), ContainsSubstring("capacity must be finite")
  );

  const std::vector<Part> negative_capacity{tank("tank_1", {{{"NTO", -1}}, {}, {}})};
  CHECK_THROWS_WITH(
      CraftResources(negative_capacity, ctx), ContainsSubstring("capacity must be finite")
  );

  const std::vector<Part> infinite_amount{
      tank("tank_1", {{{"NTO", 10}}, {{"NTO", std::numeric_limits<double>::infinity()}}, {}})
  };
  CHECK_THROWS_WITH(
      CraftResources(infinite_amount, ctx), ContainsSubstring("amount must be finite")
  );
}

TEST_CASE("CraftResources ignores parts without container plugins") {
  Part pod;
  pod.type_id = "Pod";
  pod.id = "pod_1";
  const std::vector<Part> parts{pod};
  const auto ctx = context();
  const CraftResources resources(parts, ctx);
  CHECK(resources.containers().empty());
}
