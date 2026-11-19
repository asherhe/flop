#pragma once

#include <span>
#include <string_view>
#include <vector>

#include "flop/craft/resource_registry.hpp"
#include "flop/world/celestial_body.hpp"
#include "flop/world/universe_context.hpp"

namespace flop::world {

// describes a full game world.
// owns universal time (UT), celestial bodies, and all universal Manager instances
class Universe : public UniverseContext {
public:
  explicit Universe(
      std::vector<BodyConfig> configs,
      double initial_ut_sec = 0,
      double gravitational_constant = 6.67430e-11
  );
  Universe(
      std::vector<BodyConfig> configs,
      std::vector<craft::ResourceTypeConfig> resource_configs,
      double initial_ut_sec = 0,
      double gravitational_constant = 6.67430e-11
  );

  [[nodiscard]] double ut_sec() const noexcept { return ut_sec_; }
  [[nodiscard]] double gravitational_constant() const noexcept { return gravitational_constant_; }
  [[nodiscard]] const craft::ResourceRegistry &resources() const noexcept override {
    return resources_;
  }

  [[nodiscard]] std::span<const CelestialBody> bodies() const noexcept { return bodies_; }
  // get CelestialBody by name
  [[nodiscard]] const CelestialBody &body(std::string_view name) const;

  [[nodiscard]] const orbit::RefFrameManager &reference_frames() const noexcept override {
    return frames_;
  }
  // get FrameId by celestial body name
  [[nodiscard]] orbit::FrameId frame_id(std::string_view name) const;

  // position and velocity of a body's center relative to the root at the requested UT
  [[nodiscard]] orbit::StateVectors body_state(std::string_view name, double ut_sec) const;

  // advance UT by a simulation interval supplied by the application
  void tick(double dt_sec);

private:
  double ut_sec_;
  double gravitational_constant_;
  std::vector<CelestialBody> bodies_;
  orbit::RefFrameManager frames_;
  orbit::FrameId root_frame_id_;
  craft::ResourceRegistry resources_;
};

} // namespace flop::world
