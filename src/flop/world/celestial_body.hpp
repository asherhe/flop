#pragma once

#include "flop/orbit/ref_frame.hpp"
#include "flop/world/body_config.hpp"

namespace flop::world {

class CelestialBody {
public:
  explicit CelestialBody(BodyConfig config, double gravitational_constant);

  [[nodiscard]] const BodyConfig &config() const noexcept { return config_; }
  [[nodiscard]] const std::string &name() const noexcept { return config_.name; }
  [[nodiscard]] double gravitational_parameter() const noexcept { return gravitational_parameter_; }
  [[nodiscard]] double mass_kg() const noexcept { return mass_kg_; }

  // a child requires its parent's gravitational parameter; the root has none
  [[nodiscard]] orbit::RefFrameDefinition ref_frame_definition(
      std::optional<double> parent_mu = std::nullopt
  ) const;

private:
  BodyConfig config_;
  double gravitational_parameter_;
  double mass_kg_;
};

} // namespace flop::world
