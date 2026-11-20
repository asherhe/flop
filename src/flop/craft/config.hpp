#pragma once

#include <optional>
#include <string>
#include <vector>

#include "flop/craft/parts/config.hpp"
#include "flop/orbit/state.hpp"

namespace flop::craft {

// a craft's world orbit as a cartesian seed. a template has no position in the
// world and omits this; a save instance supplies it. the parent gravity and
// frame are resolved when the craft is constructed. state vectors are used
// rather than orbital elements so a saved instance records the exact orbit the
// craft had, independent of the element representation.
struct CraftOrbitState {
  std::string parent_name;
  orbit::StateVectors state;
  double epoch_sec;
};

// initial attitude only - changing attitude belongs to CraftNewtonian.
struct CraftAttitudeConfig {
  double angle_rad;
  double angular_momentum_kg_m2_per_s;
};

struct CraftConfig {
  std::string name;
  // absent for a template; present for a placed instance loaded from a save.
  // Craft construction requires it and throws when it is absent.
  std::optional<CraftOrbitState> orbit;
  CraftAttitudeConfig initial_attitude;
  std::vector<PartConfig> parts;
  bool plugins_resolved{};
};

} // namespace flop::craft
