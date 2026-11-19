#pragma once

#include <string>
#include <vector>

#include "flop/craft/parts/config.hpp"

namespace flop::craft {

// configured orbit elements. the parent gravity is resolved when the craft is constructed.
struct CraftOrbitConfig {
  std::string parent_name;
  double specific_angular_momentum_m2_per_s;
  double eccentricity;
  double longitude_periapsis_rad;
  double epoch_mean_anomaly_rad;
  double epoch_sec;
};

// initial attitude only - changing attitude belongs to CraftNewtonian.
struct CraftAttitudeConfig {
  double angle_rad;
  double angular_momentum_kg_m2_per_s;
};

struct CraftConfig {
  std::string name;
  CraftOrbitConfig orbit;
  CraftAttitudeConfig initial_attitude;
  std::vector<PartConfig> parts;
  bool plugins_resolved{};
};

} // namespace flop::craft
