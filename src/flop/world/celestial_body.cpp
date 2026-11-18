#include "flop/world/celestial_body.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

#include "flop/math/utils.hpp"
#include "flop/orbit/state.hpp"

namespace flop::world {

CelestialBody::CelestialBody(BodyConfig config, double gravitational_constant) :
    config_(std::move(config)) {
  if (config_.name.empty() || !positive_finite(config_.radius_meters) ||
      !positive_finite(config_.surface_gravity_m_per_s2) ||
      !positive_finite(config_.day_length_sec) || !positive_finite(gravitational_constant)) {
    throw std::invalid_argument("Invalid celestial body physical values");
  }
  if (config_.orbit) {
    const auto &orbit = *config_.orbit;
    if (orbit.parent_name.empty() || !positive_finite(orbit.semimajor_axis_meters) ||
        !std::isfinite(orbit.eccentricity) || orbit.eccentricity < 0 || orbit.eccentricity >= 1 ||
        !std::isfinite(orbit.longitude_periapsis_radians) ||
        !std::isfinite(orbit.epoch_mean_anomaly_radians) || !std::isfinite(orbit.epoch_sec)) {
      throw std::invalid_argument("Invalid celestial body orbit values");
    }
  }

  gravitational_parameter_ =
      config_.surface_gravity_m_per_s2 * config_.radius_meters * config_.radius_meters;
  mass_kg_ = gravitational_parameter_ / gravitational_constant;
  if (!positive_finite(gravitational_parameter_) || !positive_finite(mass_kg_)) {
    throw std::invalid_argument("Celestial body gravity or mass exceeds the representable domain");
  }
}

orbit::RefFrameDefinition CelestialBody::ref_frame_definition(
    std::optional<double> parent_mu
) const {
  if (!config_.orbit) {
    if (parent_mu) {
      throw std::invalid_argument("Root celestial body cannot have a parent gravity");
    }
    return {
        config_.name,
        gravitational_parameter_,
        std::nullopt,
        std::nullopt,
        std::numeric_limits<double>::infinity(),
    };
  }
  if (!parent_mu || !positive_finite(*parent_mu)) {
    throw std::invalid_argument("Child celestial body requires a positive parent gravity");
  }

  const auto &orbit = *config_.orbit;
  const double p =
      orbit.semimajor_axis_meters * (1 - orbit.eccentricity) * (1 + orbit.eccentricity);
  const double h = std::sqrt(*parent_mu * p);
  const double soi =
      orbit.semimajor_axis_meters * std::pow(gravitational_parameter_ / *parent_mu, 0.4);
  if (!positive_finite(p) || !positive_finite(h) || !positive_finite(soi)) {
    throw std::invalid_argument("Celestial body orbit exceeds the representable domain");
  }
  const auto seed = orbit::state_at_epoch_from_elements(
      *parent_mu,
      orbit.epoch_sec,
      {
          h,
          orbit.eccentricity,
          orbit.longitude_periapsis_radians,
          orbit.epoch_mean_anomaly_radians,
      }
  );
  return {config_.name, gravitational_parameter_, orbit.parent_name, seed, soi};
}

} // namespace flop::world
