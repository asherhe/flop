// Universe craft ownership: stable ids, stable addresses across growth, and
// clean failure for stale ids. These exercise the boundary added for the SDL
// application build, not the simulation stepping (covered elsewhere).

#include <limits>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "flop/craft/config.hpp"
#include "flop/craft/craft.hpp"
#include "flop/craft/ids.hpp"
#include "flop/craft/resource_type_config.hpp"
#include "flop/world/craft_id.hpp"
#include "flop/world/universe.hpp"

namespace {

using flop::craft::CraftConfig;
using flop::craft::PartTypeConfig;
using flop::craft::ResourceFlow;
using flop::craft::ResourceTypeConfig;
using flop::world::BodyConfig;
using flop::world::CraftId;
using flop::world::Universe;

// a single infinite-SOI root body the craft can orbit.
BodyConfig root_body() {
  return {"Hamswell", 3.0e6, 9.8, 86400, {}};
}

std::vector<ResourceTypeConfig> resource_types() {
  return {{"NTO", "N2O4", 1.0, ResourceFlow::stage}};
}

PartTypeConfig pod_type() {
  return {"Pod", "Pod", 800, true, {{"bottom", {0, -1}}}};
}

// a minimal one-part craft orbiting the root body.
CraftConfig craft_config(const char *name) {
  CraftConfig config{
      name,
      flop::craft::CraftOrbitState{"Hamswell", {{3.0e6, 0}, {0, 5000}}, 0},
      {0.0, 0.0},
      {{"Pod", "pod_1", "Pod", {0, 0}, 0, {}, {}}},
  };
  config.plugins_resolved = true;
  return config;
}

} // namespace

TEST_CASE("Universe starts empty and assigns increasing craft ids") {
  Universe universe({root_body()}, resource_types());
  CHECK(universe.craft_count() == 0);
  CHECK(universe.craft_ids().empty());

  const CraftId first = universe.add_craft(craft_config("First"), {pod_type()});
  const CraftId second = universe.add_craft(craft_config("Second"), {pod_type()});

  CHECK(static_cast<std::uint64_t>(first) == 1);
  CHECK(static_cast<std::uint64_t>(second) == 2);
  CHECK(first != second);
  CHECK(universe.craft_count() == 2);
  CHECK(universe.craft_ids() == std::vector<CraftId>{first, second});
  CHECK(universe.craft(first).name() == "First");
  CHECK(universe.craft(second).name() == "Second");
}

TEST_CASE("Craft keep their address and id as storage grows") {
  Universe universe({root_body()}, resource_types());
  const CraftId first = universe.add_craft(craft_config("First"), {pod_type()});
  const flop::craft::Craft *first_address = &universe.craft(first);

  // add enough craft to force any contiguous container to reallocate.
  for (int index = 0; index < 64; ++index) {
    universe.add_craft(craft_config("Filler"), {pod_type()});
  }

  // the originally returned id still resolves to the same craft object.
  CHECK(&universe.craft(first) == first_address);
  CHECK(universe.craft(first).name() == "First");
  CHECK(universe.craft_count() == 65);
}

TEST_CASE("Universe rejects stale and never-assigned craft ids") {
  Universe universe({root_body()}, resource_types());
  const CraftId valid = universe.add_craft(craft_config("Only"), {pod_type()});

  CHECK(universe.find_craft(valid) != nullptr);
  // an id past the high-water mark was never assigned.
  const auto future = static_cast<CraftId>(static_cast<std::uint64_t>(valid) + 1000);
  CHECK(universe.find_craft(future) == nullptr);
  CHECK(universe.find_craft(CraftId::invalid) == nullptr);
  CHECK_THROWS_AS(universe.craft(future), std::out_of_range);
  CHECK_THROWS_AS(universe.craft(CraftId::invalid), std::out_of_range);
}
