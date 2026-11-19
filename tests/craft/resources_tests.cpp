#include <limits>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "flop/craft/parts/part.hpp"
#include "flop/craft/plugins/config.hpp"
#include "flop/craft/plugins/resource_plugin.hpp"
#include "flop/craft/resource_type_config.hpp"
#include "flop/craft/resources.hpp"

#include "support/stub_craft_context.hpp"
#include "support/stub_universe_context.hpp"

namespace {
using Catch::Approx;
using flop::craft::CraftResources;
using flop::craft::Part;
using flop::craft::ResourceFlow;
using flop::craft::plugin::PartPluginConfig;
using flop::craft::plugin::ResourceConfig;
using flop::craft::plugin::ResourcePlugin;
using flop::craft::plugin::ResourceSlot;
using flop::testing::StubCraftContext;
using flop::testing::StubUniverseContext;

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

Part dry_part(std::string id, double dry_mass_kg, flop::Vec2d position) {
  Part part;
  part.type_id = "Part";
  part.id = std::move(id);
  part.dry_mass_kg = dry_mass_kg;
  part.position_meters = position;
  return part;
}

// a two-way attachment between two parts, so crossfeed traversal reaches both.
void attach(Part &a, Part &b) {
  a.attachments.push_back({"node_" + b.id, b.id, "node_" + a.id});
  b.attachments.push_back({"node_" + a.id, a.id, "node_" + b.id});
}

// collect move-only parts into a vector and build their runtime plugins. Part
// owns its runtime plugins and is move-only, so parts cannot be gathered with
// braced list initialization.
template <class... Parts>
std::vector<Part> assemble(const StubUniverseContext &universe, Parts &&...parts) {
  std::vector<Part> result;
  result.reserve(sizeof...(parts));
  (result.push_back(std::forward<Parts>(parts)), ...);
  for (std::size_t index = 0; index < result.size(); ++index) {
    result[index].install_plugins(universe.resources(), index);
  }
  return result;
}

// the first resource container installed on a part.
const ResourcePlugin &container(const Part &part) {
  for (const auto &plugin : part.installed_plugins()) {
    if (const auto *resource = dynamic_cast<const ResourcePlugin *>(plugin.get())) {
      return *resource;
    }
  }
  FAIL("part has no resource container plugin");
  throw std::logic_error("unreachable");
}

const ResourceSlot &slot(const Part &part, std::string_view resource_id) {
  const auto *found = container(part).find_slot(resource_id);
  REQUIRE(found != nullptr);
  return *found;
}
} // namespace

TEST_CASE("CraftResources mass properties sum dry mass with no resources") {
  const auto parts =
      assemble(context(), dry_part("pod_1", 800, {0, 0}), dry_part("tank_2", 600, {0, -2}));
  const auto ctx = context();
  const CraftResources resources(parts, ctx);

  const auto mass = resources.mass_properties(StubCraftContext{}.with_parts(parts));
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
  const auto parts = assemble(context(), std::move(pod), std::move(tank));
  const auto ctx = context();
  const CraftResources resources(parts, ctx);

  const auto mass = resources.mass_properties(StubCraftContext{}.with_parts(parts));
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
  const auto parts = assemble(context(), std::move(tank));
  const auto ctx = context();
  const CraftResources resources(parts, ctx);

  REQUIRE_FALSE(container(parts.front()).enabled());
  const auto mass = resources.mass_properties(StubCraftContext{}.with_parts(parts));
  // 600 dry + 100 units * 2 kg/unit = 800 kg, all at the single part's position
  CHECK(mass.total_mass_kg == Approx(800));
  CHECK(mass.center_of_mass_meters.y == Approx(-2));
}

TEST_CASE("CraftResources mass properties place a single part's COM at the part") {
  const auto parts = assemble(context(), dry_part("solo", 500, {3, -4}));
  const auto ctx = context();
  const CraftResources resources(parts, ctx);

  const auto mass = resources.mass_properties(StubCraftContext{}.with_parts(parts));
  CHECK(mass.total_mass_kg == 500);
  CHECK(mass.center_of_mass_meters.x == Approx(3));
  CHECK(mass.center_of_mass_meters.y == Approx(-4));
  // a single point mass sits on the com, so the point-mass inertia is zero.
  CHECK(mass.moment_of_inertia_kg_m2 == Approx(0));
}

TEST_CASE("CraftResources mass properties handle a massless craft without dividing by zero") {
  const auto parts = assemble(context(), dry_part("ghost", 0, {5, 5}));
  const auto ctx = context();
  const CraftResources resources(parts, ctx);

  const auto mass = resources.mass_properties(StubCraftContext{}.with_parts(parts));
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
  const auto parts = assemble(
      context(),
      std::move(pod),
      std::move(tank_a),
      std::move(tank_b),
      std::move(wall),
      std::move(tank_c)
  );
  const auto ctx = context();
  const CraftResources resources(parts, ctx);

  // reading NTO (stage flow) from the pod reaches tank_a and tank_b but not
  // tank_c, which sits behind the non-crossfeed wall.
  CHECK(resources.read(StubCraftContext{}.with_parts(parts), 0, "NTO") == 1000);
}

TEST_CASE("CraftResources None reach is limited to the source part") {
  Part source = tank("source", {{{"Ablator", 50}}, {{"Ablator", 50}}, {}});
  Part other = tank("other", {{{"Ablator", 50}}, {{"Ablator", 50}}, {}});
  attach(source, other);
  const auto parts = assemble(context(), std::move(source), std::move(other));
  const auto ctx = context();
  const CraftResources resources(parts, ctx);

  // Ablator has None flow, so only the source part's own container is reached.
  CHECK(resources.read(StubCraftContext{}.with_parts(parts), 0, "Ablator") == 50);
}

TEST_CASE("CraftResources Craft reach spans every container regardless of attachment") {
  Part a = tank("a", {{{"Electricity", 100}}, {{"Electricity", 30}}, {}});
  Part b = tank("b", {{{"Electricity", 100}}, {{"Electricity", 45}}, {}});
  // no attachments between a and b at all.
  const auto parts = assemble(context(), std::move(a), std::move(b));
  const auto ctx = context();
  const CraftResources resources(parts, ctx);

  // Electricity has Craft flow, so both containers are reachable from either.
  CHECK(resources.read(StubCraftContext{}.with_parts(parts), 0, "Electricity") == 75);
  CHECK(resources.read(StubCraftContext{}.with_parts(parts), 1, "Electricity") == 75);
}

TEST_CASE("CraftResources crossfeed traversal terminates on cyclic attachments") {
  Part a = tank("a", {{{"NTO", 100}}, {{"NTO", 10}}, {}});
  Part b = tank("b", {{{"NTO", 100}}, {{"NTO", 20}}, {}});
  Part c = tank("c", {{{"NTO", 100}}, {{"NTO", 30}}, {}});
  attach(a, b);
  attach(b, c);
  attach(c, a); // closes the loop
  const auto parts = assemble(context(), std::move(a), std::move(b), std::move(c));
  const auto ctx = context();
  const CraftResources resources(parts, ctx);

  CHECK(resources.read(StubCraftContext{}.with_parts(parts), 0, "NTO") == 60);
}

TEST_CASE("CraftResources drain delivers from a single reachable tank") {
  auto parts = assemble(context(), tank("tank", {{{"NTO", 1000}}, {{"NTO", 400}}, {}}));
  const auto ctx = context();
  CraftResources resources(parts, ctx);

  CHECK(resources.drain(StubCraftContext{}.with_parts(parts), 0, "NTO", 150) == Approx(150));
  CHECK(resources.read(StubCraftContext{}.with_parts(parts), 0, "NTO") == Approx(250));
}

TEST_CASE("CraftResources drain never reports more than is available") {
  auto parts = assemble(context(), tank("tank", {{{"NTO", 1000}}, {{"NTO", 200}}, {}}));
  const auto ctx = context();
  CraftResources resources(parts, ctx);

  CHECK(resources.drain(StubCraftContext{}.with_parts(parts), 0, "NTO", 500) == Approx(200));
  CHECK(resources.read(StubCraftContext{}.with_parts(parts), 0, "NTO") == Approx(0));
  // a second drain on the empty tank delivers nothing.
  CHECK(resources.drain(StubCraftContext{}.with_parts(parts), 0, "NTO", 10) == Approx(0));
}

TEST_CASE("CraftResources drain empties higher-priority containers first") {
  Part high = tank("high", {{{"NTO", 1000}}, {{"NTO", 300}}, {{"NTO", 5}}});
  Part low = tank("low", {{{"NTO", 1000}}, {{"NTO", 300}}, {{"NTO", 1}}});
  attach(high, low);
  auto parts = assemble(context(), std::move(high), std::move(low));
  const auto ctx = context();
  CraftResources resources(parts, ctx);

  // draining 300 should empty the high-priority tank and leave the low one full.
  CHECK(resources.drain(StubCraftContext{}.with_parts(parts), 0, "NTO", 300) == Approx(300));
  CHECK(slot(parts[0], "NTO").amount_units == Approx(0));
  CHECK(slot(parts[1], "NTO").amount_units == Approx(300));
}

TEST_CASE("CraftResources drain splits an equal-priority group by current contents") {
  // two equal-priority tanks with different fill levels share a drain in
  // proportion to how full each is.
  Part a = tank("a", {{{"NTO", 1000}}, {{"NTO", 300}}, {{"NTO", 2}}});
  Part b = tank("b", {{{"NTO", 1000}}, {{"NTO", 100}}, {{"NTO", 2}}});
  attach(a, b);
  auto parts = assemble(context(), std::move(a), std::move(b));
  const auto ctx = context();
  CraftResources resources(parts, ctx);

  // group has 400 units; draining 200 takes half of each tank's contents.
  CHECK(resources.drain(StubCraftContext{}.with_parts(parts), 0, "NTO", 200) == Approx(200));
  CHECK(slot(parts[0], "NTO").amount_units == Approx(150));
  CHECK(slot(parts[1], "NTO").amount_units == Approx(50));
}

TEST_CASE("CraftResources drain on empty tanks delivers zero without dividing by zero") {
  Part a = tank("a", {{{"NTO", 1000}}, {}, {{"NTO", 1}}});
  Part b = tank("b", {{{"NTO", 1000}}, {}, {{"NTO", 1}}});
  attach(a, b);
  auto parts = assemble(context(), std::move(a), std::move(b));
  const auto ctx = context();
  CraftResources resources(parts, ctx);

  CHECK(resources.drain(StubCraftContext{}.with_parts(parts), 0, "NTO", 100) == Approx(0));
  CHECK(resources.read(StubCraftContext{}.with_parts(parts), 0, "NTO") == Approx(0));
}

TEST_CASE("CraftResources drain of a non-positive request delivers zero") {
  auto parts = assemble(context(), tank("tank", {{{"NTO", 1000}}, {{"NTO", 400}}, {}}));
  const auto ctx = context();
  CraftResources resources(parts, ctx);

  CHECK(resources.drain(StubCraftContext{}.with_parts(parts), 0, "NTO", 0) == Approx(0));
  CHECK(resources.drain(StubCraftContext{}.with_parts(parts), 0, "NTO", -5) == Approx(0));
  CHECK(resources.read(StubCraftContext{}.with_parts(parts), 0, "NTO") == Approx(400));
}

TEST_CASE("CraftResources skips disabled containers when reading and draining") {
  Part on = tank("on", {{{"NTO", 1000}}, {{"NTO", 200}}, {}});
  Part off = tank("off", {{{"NTO", 1000}}, {{"NTO", 500}}, {}}, false);
  attach(on, off);
  auto parts = assemble(context(), std::move(on), std::move(off));
  const auto ctx = context();
  CraftResources resources(parts, ctx);

  // the disabled tank's 500 units are not reachable.
  CHECK(resources.read(StubCraftContext{}.with_parts(parts), 0, "NTO") == Approx(200));
  CHECK(resources.drain(StubCraftContext{}.with_parts(parts), 0, "NTO", 1000) == Approx(200));
  // the disabled tank still holds its contents (and its mass).
  CHECK(slot(parts[1], "NTO").amount_units == Approx(500));
}
