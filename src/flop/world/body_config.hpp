#pragma once

#include <optional>
#include <string>

namespace flop::world {

struct BodyOrbitConfig {
  // the body this one is orbiting around
  std::string parent_name;
  double semimajor_axis_meters;
  double eccentricity;
  double longitude_periapsis_radians;
  double epoch_mean_anomaly_radians;
  double epoch_sec;
};

struct BodyConfig {
  // display name and unique identifier for body
  std::string name;
  double radius_meters;
  double surface_gravity_m_per_s2;
  double day_length_sec;
  // not given for root body
  std::optional<BodyOrbitConfig> orbit;
};

} // namespace flop::world
