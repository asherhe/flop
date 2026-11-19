#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "flop/craft/parts/part.hpp"
#include "flop/craft/parts/part_plugin_config.hpp"
#include "flop/craft/resource_type_config.hpp"
#include "flop/craft/resources.hpp"

#include "support/stub_craft_context.hpp"
#include "support/stub_universe_context.hpp"

namespace {
using Catch::Approx;
using Catch::Matchers::ContainsSubstring;
using flop::craft::ContainerState;
using flop::craft::CraftResources;
using flop::craft::Part;
using flop::craft::PartPluginConfig;
using flop::craft::ResourceConfig;
using flop::craft::ResourceFlow;
using flop::craft::ResourceSlot;
using flop::craft::ResourceTypeConfig;
using flop::testing::StubCraftContext;
using flop::testing::StubUniverseContext;

// a craft context exposing the given parts, as the resource queries read them.
StubCraftContext craft_ctx(std::span<const Part> parts) {
  StubCraftContext context;
  context.with_parts(parts);
  return context;
}

flop::orbit::RefFrameManager frames() {
  return flop::orbit::RefFrameManager(
      {{"Hamswell", 8.82e13, {}, {}, std::numeric_limits<double>::infinity()}}
  );
}

StubUniverseContext context() {
  return StubUniverseContext(
      frames(),
      {{"NTO", "N2O4", 1.0, ResourceFlow::stage},
       {"Fuel", "Hydrazine", 2.0, ResourceFlow::stage},
       {"Ablator", "Ablator", 1.0, ResourceFlow::none},
       {"Electricity", "EC", 0.001, ResourceFlow::craft}}
  );
}

PartPluginConfig container_plugin(ResourceConfig config, bool enabled = true) {
  return {"ResourceContainerPlugin", enabled, std::move(config)};
}

Part tank(std::string id, ResourceConfig config, bool enabled = true) {
  Part part;
  part.type_id = "Tank";
  part.id = std::move(id);
  part.crossfeed = true;
  part.plugins.push_back(container_plugin(std::move(config), enabled));
  return part;
}

// a two-way attachment between two parts, so crossfeed traversal reaches both.
void attach(Part &a, Part &b) {
  a.attachments.push_back({"node_" + b.id, b.id, "node_" + a.id});
  b.attachments.push_back({"node_" + a.id, a.id, "node_" + b.id});
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

namespace {

Part dry_part(std::string id, double dry_mass_kg, flop::Vec2d position) {
  Part part;
  part.type_id = "Part";
  part.id = std::move(id);
  part.dry_mass_kg = dry_mass_kg;
  part.position_meters = position;
  return part;
}

} // namespace

TEST_CASE("CraftResources mass properties sum dry mass with no resources") {
  const std::vector<Part> parts{
      dry_part("pod_1", 800, {0, 0}),
      dry_part("tank_2", 600, {0, -2}),
  };
  const auto ctx = context();
  const CraftResources resources(parts, ctx);

  const auto mass = resources.mass_properties(craft_ctx(parts));
  CHECK(mass.total_mass_kg == 1400);
  CHECK(mass.center_of_mass_meters.x == 0);
  // 600 kg at y = -2, 800 kg at y = 0  ->  -1200 / 1400
  CHECK(mass.center_of_mass_meters.y == Approx(-1200.0 / 1400.0));
  // point-mass inertia about the com: 800*(6/7)^2 + 600*(8/7)^2 = 67200/49
  CHECK(mass.moment_of_inertia_kg_m2 == Approx(67200.0 / 49.0));
}

TEST_CASE("CraftResources mass properties include live resource mass") {
  // NTO is 1 kg/unit, Fuel is 2 kg/unit in the test registry.
  Part pod = dry_part("pod_1", 800, {0, 0});
  Part tank = dry_part("tank_2", 600, {0, -2});
  tank.plugins.push_back(container_plugin({{{"NTO", 3600}}, {{"NTO", 500}}, {}}));
  const std::vector<Part> parts{pod, tank};
  const auto ctx = context();
  const CraftResources resources(parts, ctx);

  const auto mass = resources.mass_properties(craft_ctx(parts));
  // tank: 600 dry + 500 * 1 kg/unit = 1100 kg at y = -2; pod: 800 kg at y = 0
  CHECK(mass.total_mass_kg == Approx(1900));
  CHECK(mass.center_of_mass_meters.y == Approx(-2200.0 / 1900.0));
  // inertia uses the resource-augmented masses:
  // 800*(22/19)^2 + 1100*(16/19)^2 = 668800/361
  CHECK(mass.moment_of_inertia_kg_m2 == Approx(668800.0 / 361.0));
}

TEST_CASE("CraftResources mass properties count disabled container contents") {
  Part tank = dry_part("tank_1", 600, {0, -2});
  tank.plugins.push_back(container_plugin({{{"Fuel", 1000}}, {{"Fuel", 100}}, {}}, false));
  const std::vector<Part> parts{tank};
  const auto ctx = context();
  const CraftResources resources(parts, ctx);

  REQUIRE_FALSE(resources.containers().front().enabled);
  const auto mass = resources.mass_properties(craft_ctx(parts));
  // 600 dry + 100 units * 2 kg/unit = 800 kg, all at the single part's position
  CHECK(mass.total_mass_kg == Approx(800));
  CHECK(mass.center_of_mass_meters.y == Approx(-2));
}

TEST_CASE("CraftResources mass properties place a single part's COM at the part") {
  const std::vector<Part> parts{dry_part("solo", 500, {3, -4})};
  const auto ctx = context();
  const CraftResources resources(parts, ctx);

  const auto mass = resources.mass_properties(craft_ctx(parts));
  CHECK(mass.total_mass_kg == 500);
  CHECK(mass.center_of_mass_meters.x == Approx(3));
  CHECK(mass.center_of_mass_meters.y == Approx(-4));
  // a single point mass sits on the com, so the point-mass inertia is zero.
  CHECK(mass.moment_of_inertia_kg_m2 == Approx(0));
}

TEST_CASE("CraftResources mass properties handle a massless craft without dividing by zero") {
  const std::vector<Part> parts{dry_part("ghost", 0, {5, 5})};
  const auto ctx = context();
  const CraftResources resources(parts, ctx);

  const auto mass = resources.mass_properties(craft_ctx(parts));
  CHECK(mass.total_mass_kg == 0);
  CHECK(mass.center_of_mass_meters.x == 0);
  CHECK(mass.center_of_mass_meters.y == 0);
  CHECK(mass.moment_of_inertia_kg_m2 == 0);
}

TEST_CASE(
    "CraftResources crossfeed read reaches connected tanks and stops at non-crossfeed parts"
) {
  // pod - tank_a - tank_b, all crossfeed; tank_c attached past a non-crossfeed wall.
  Part pod = dry_part("pod", 800, {0, 0});
  pod.crossfeed = true;
  Part tank_a = tank("tank_a", {{{"NTO", 1000}}, {{"NTO", 400}}, {}});
  Part tank_b = tank("tank_b", {{{"NTO", 1000}}, {{"NTO", 600}}, {}});
  Part wall = dry_part("wall", 100, {0, -3});
  wall.crossfeed = false;
  Part tank_c = tank("tank_c", {{{"NTO", 1000}}, {{"NTO", 999}}, {}});
  attach(pod, tank_a);
  attach(tank_a, tank_b);
  attach(tank_b, wall);
  attach(wall, tank_c);
  const std::vector<Part> parts{pod, tank_a, tank_b, wall, tank_c};
  const auto ctx = context();
  const CraftResources resources(parts, ctx);

  // reading NTO (stage flow) from the pod reaches tank_a and tank_b but not
  // tank_c, which sits behind the non-crossfeed wall.
  CHECK(resources.read(craft_ctx(parts), 0, "NTO") == 1000);
}

TEST_CASE("CraftResources None reach is limited to the source part") {
  Part source = tank("source", {{{"Ablator", 50}}, {{"Ablator", 50}}, {}});
  Part other = tank("other", {{{"Ablator", 50}}, {{"Ablator", 50}}, {}});
  attach(source, other);
  const std::vector<Part> parts{source, other};
  const auto ctx = context();
  const CraftResources resources(parts, ctx);

  // Ablator has None flow, so only the source part's own container is reached.
  CHECK(resources.read(craft_ctx(parts), 0, "Ablator") == 50);
}

TEST_CASE("CraftResources Craft reach spans every container regardless of attachment") {
  Part a = tank("a", {{{"Electricity", 100}}, {{"Electricity", 30}}, {}});
  Part b = tank("b", {{{"Electricity", 100}}, {{"Electricity", 45}}, {}});
  // no attachments between a and b at all.
  const std::vector<Part> parts{a, b};
  const auto ctx = context();
  const CraftResources resources(parts, ctx);

  // Electricity has Craft flow, so both containers are reachable from either.
  CHECK(resources.read(craft_ctx(parts), 0, "Electricity") == 75);
  CHECK(resources.read(craft_ctx(parts), 1, "Electricity") == 75);
}

TEST_CASE("CraftResources crossfeed traversal terminates on cyclic attachments") {
  Part a = tank("a", {{{"NTO", 100}}, {{"NTO", 10}}, {}});
  Part b = tank("b", {{{"NTO", 100}}, {{"NTO", 20}}, {}});
  Part c = tank("c", {{{"NTO", 100}}, {{"NTO", 30}}, {}});
  attach(a, b);
  attach(b, c);
  attach(c, a); // closes the loop
  const std::vector<Part> parts{a, b, c};
  const auto ctx = context();
  const CraftResources resources(parts, ctx);

  CHECK(resources.read(craft_ctx(parts), 0, "NTO") == 60);
}

TEST_CASE("CraftResources drain delivers from a single reachable tank") {
  std::vector<Part> parts{tank("tank", {{{"NTO", 1000}}, {{"NTO", 400}}, {}})};
  const auto ctx = context();
  CraftResources resources(parts, ctx);

  CHECK(resources.drain(craft_ctx(parts), 0, "NTO", 150) == Approx(150));
  CHECK(resources.read(craft_ctx(parts), 0, "NTO") == Approx(250));
}

TEST_CASE("CraftResources drain never reports more than is available") {
  std::vector<Part> parts{tank("tank", {{{"NTO", 1000}}, {{"NTO", 200}}, {}})};
  const auto ctx = context();
  CraftResources resources(parts, ctx);

  CHECK(resources.drain(craft_ctx(parts), 0, "NTO", 500) == Approx(200));
  CHECK(resources.read(craft_ctx(parts), 0, "NTO") == Approx(0));
  // a second drain on the empty tank delivers nothing.
  CHECK(resources.drain(craft_ctx(parts), 0, "NTO", 10) == Approx(0));
}

TEST_CASE("CraftResources drain empties higher-priority containers first") {
  Part high = tank("high", {{{"NTO", 1000}}, {{"NTO", 300}}, {{"NTO", 5}}});
  Part low = tank("low", {{{"NTO", 1000}}, {{"NTO", 300}}, {{"NTO", 1}}});
  attach(high, low);
  std::vector<Part> parts{high, low};
  const auto ctx = context();
  CraftResources resources(parts, ctx);

  // draining 300 should empty the high-priority tank and leave the low one full.
  CHECK(resources.drain(craft_ctx(parts), 0, "NTO", 300) == Approx(300));
  const auto &containers = resources.containers();
  const auto high_amount = containers[0].slots.front().amount_units;
  const auto low_amount = containers[1].slots.front().amount_units;
  CHECK(high_amount == Approx(0));
  CHECK(low_amount == Approx(300));
}

TEST_CASE("CraftResources drain splits an equal-priority group by current contents") {
  // two equal-priority tanks with different fill levels share a drain in
  // proportion to how full each is.
  Part a = tank("a", {{{"NTO", 1000}}, {{"NTO", 300}}, {{"NTO", 2}}});
  Part b = tank("b", {{{"NTO", 1000}}, {{"NTO", 100}}, {{"NTO", 2}}});
  attach(a, b);
  std::vector<Part> parts{a, b};
  const auto ctx = context();
  CraftResources resources(parts, ctx);

  // group has 400 units; draining 200 takes half of each tank's contents.
  CHECK(resources.drain(craft_ctx(parts), 0, "NTO", 200) == Approx(200));
  const auto &containers = resources.containers();
  CHECK(containers[0].slots.front().amount_units == Approx(150));
  CHECK(containers[1].slots.front().amount_units == Approx(50));
}

TEST_CASE("CraftResources drain on empty tanks delivers zero without dividing by zero") {
  Part a = tank("a", {{{"NTO", 1000}}, {}, {{"NTO", 1}}});
  Part b = tank("b", {{{"NTO", 1000}}, {}, {{"NTO", 1}}});
  attach(a, b);
  std::vector<Part> parts{a, b};
  const auto ctx = context();
  CraftResources resources(parts, ctx);

  CHECK(resources.drain(craft_ctx(parts), 0, "NTO", 100) == Approx(0));
  CHECK(resources.read(craft_ctx(parts), 0, "NTO") == Approx(0));
}

TEST_CASE("CraftResources drain of a non-positive request delivers zero") {
  std::vector<Part> parts{tank("tank", {{{"NTO", 1000}}, {{"NTO", 400}}, {}})};
  const auto ctx = context();
  CraftResources resources(parts, ctx);

  CHECK(resources.drain(craft_ctx(parts), 0, "NTO", 0) == Approx(0));
  CHECK(resources.drain(craft_ctx(parts), 0, "NTO", -5) == Approx(0));
  CHECK(resources.read(craft_ctx(parts), 0, "NTO") == Approx(400));
}

TEST_CASE("CraftResources skips disabled containers when reading and draining") {
  Part on = tank("on", {{{"NTO", 1000}}, {{"NTO", 200}}, {}});
  Part off = tank("off", {{{"NTO", 1000}}, {{"NTO", 500}}, {}}, false);
  attach(on, off);
  std::vector<Part> parts{on, off};
  const auto ctx = context();
  CraftResources resources(parts, ctx);

  // the disabled tank's 500 units are not reachable.
  CHECK(resources.read(craft_ctx(parts), 0, "NTO") == Approx(200));
  CHECK(resources.drain(craft_ctx(parts), 0, "NTO", 1000) == Approx(200));
  // the disabled tank still holds its contents (and its mass).
  CHECK(resources.containers()[1].slots.front().amount_units == Approx(500));
}
