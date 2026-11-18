#pragma once

#include "flop/orbit/ref_frame.hpp"
#include "flop/world/body_config.hpp"
#include "flop/world/universe_context.hpp"

namespace flop::world {

class CelestialBody {
public:
  CelestialBody(BodyConfig config, double gravitational_constant, const UniverseContext &context);

  [[nodiscard]] const BodyConfig &config() const noexcept { return config_; }
  [[nodiscard]] const std::string &name() const noexcept { return config_.name; }
  [[nodiscard]] double gravitational_parameter() const noexcept { return gravitational_parameter_; }
  [[nodiscard]] double mass_kg() const noexcept { return mass_kg_; }

  // a child requires its parent's gravitational parameter; the root has none
  [[nodiscard]] orbit::RefFrameDefinition ref_frame_definition(
      std::optional<double> parent_mu = std::nullopt
  ) const;

private:
  // the world this body belongs to. currently unused: a body resolves all of
  // its physical values from its own config at construction. stored so future
  // body behavior can reach shared managers without a new constructor argument.
  // not dereferenced during Universe construction, while the Universe is still
  // being built.
  const UniverseContext *context_;
  BodyConfig config_;
  double gravitational_parameter_;
  double mass_kg_;
};

} // namespace flop::world
