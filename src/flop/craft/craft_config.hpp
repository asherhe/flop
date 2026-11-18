#pragma once

#include <string>

namespace flop::craft {

// configured orbit elements. the parent gravity is resolved when the craft is constructed.
struct CraftOrbitConfig {
  std::string parent_name;
  double specific_angular_momentum_m2_per_s;
  double eccentricity;
  double longitude_periapsis_radians;
  double epoch_mean_anomaly_radians;
  double epoch_sec;
};

// initial attitude only - changing attitude belongs to CraftNewtonian.
struct CraftAttitudeConfig {
  double angle_radians;
  double angular_momentum_kg_m2_per_s;
};

struct CraftConfig {
  std::string name;
  CraftOrbitConfig orbit;
  CraftAttitudeConfig initial_attitude;
};

} // namespace flop::craft
