#include "flop/craft/craft.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace flop::craft {

Craft::Craft(CraftConfig config, const orbit::RefFrameManager &frames) : name_(std::move(config.name)) {
  const auto &orbit_config = config.orbit;
  if (name_.empty() || orbit_config.parent_name.empty() ||
      !std::isfinite(orbit_config.specific_angular_momentum_m2_per_s) ||
      orbit_config.specific_angular_momentum_m2_per_s == 0 ||
      !std::isfinite(orbit_config.eccentricity) || orbit_config.eccentricity < 0 ||
      !std::isfinite(orbit_config.longitude_periapsis_radians) ||
      !std::isfinite(orbit_config.epoch_mean_anomaly_radians) ||
      !std::isfinite(orbit_config.epoch_sec)) {
    throw std::invalid_argument("Invalid craft configuration");
  }
  const auto frame_id = frames.frame_id(orbit_config.parent_name);
  const auto mu = frames.frame(frame_id).gravitational_parameter;
  orbit_state_ = orbit::state_at_epoch_from_elements(
      mu,
      orbit_config.epoch_sec,
      {
          orbit_config.specific_angular_momentum_m2_per_s,
          orbit_config.eccentricity,
          orbit_config.longitude_periapsis_radians,
          orbit_config.epoch_mean_anomaly_radians,
      },
      frame_id
  );
  patches_ = {{{orbit_state_, orbit_config.epoch_sec, orbit_config.epoch_sec}}, orbit_config.epoch_sec};
}

void Craft::predict_patches(const orbit::RefFrameManager &frames, double begin_ut, double end_ut) {
  auto prediction = orbit::build_patches(frames, orbit_state_, begin_ut, end_ut);
  if (!prediction.endpoint_resolved || prediction.searched_through < end_ut) {
    throw std::runtime_error("Craft patch prediction did not cover the requested interval");
  }
  patches_ = std::move(prediction);
}

} // namespace flop::craft
