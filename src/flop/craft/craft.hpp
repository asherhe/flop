#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "flop/craft/craft_config.hpp"
#include "flop/craft/craft_context.hpp"
#include "flop/craft/control.hpp"
#include "flop/craft/newtonian.hpp"
#include "flop/craft/parts/part.hpp"
#include "flop/craft/resources.hpp"
#include "flop/orbit/prediction/patched_conics.hpp"
#include "flop/orbit/propagators/universal.hpp"
#include "flop/orbit/ref_frame.hpp"
#include "flop/world/universe_context.hpp"

namespace flop::craft {

// owns one craft's orbit and prediction. parts and flight behavior follow later.
class Craft : public CraftContext {
public:
  Craft(
      CraftConfig config,
      const world::UniverseContext &context,
      std::vector<PartTypeConfig> part_types = {}
  );

  [[nodiscard]] std::string_view name() const noexcept { return name_; }
  [[nodiscard]] const orbit::OrbitState &orbit_state() const noexcept { return orbit_state_; }
  [[nodiscard]] const orbit::OrbitPatchSegments &patches() const noexcept { return patches_; }
  [[nodiscard]] std::span<const Part> parts() const noexcept override { return parts_; }
  [[nodiscard]] const Part &part(std::string_view id) const;

  [[nodiscard]] const CraftControl &control() const noexcept override { return control_; }
  [[nodiscard]] CraftControl &control() noexcept override { return control_; }

  [[nodiscard]] const CraftResources &resources() const noexcept override { return resources_; }
  [[nodiscard]] CraftResources &resources() noexcept override { return resources_; }

  [[nodiscard]] const CraftNewtonian &newtonian() const noexcept override { return newtonian_; }
  [[nodiscard]] CraftNewtonian &newtonian() noexcept override { return newtonian_; }

  [[nodiscard]] MassProperties mass_properties() const override {
    return resources_.mass_properties(*this);
  }

  // advance Craft simulation. triggers PartPlugin steps and advances CraftNewtonian simulation
  void step(double now_ut, double dt_sec);

  // replace the owned prediction only after the requested interval is covered.
  void predict_patches(const orbit::RefFrameManager &frames, double begin_ut, double end_ut);

private:
  // propagate the current orbit seed to `ut`, reusing a cached propagator built
  // from the seed. the cache is rebuilt lazily after reseed_orbit invalidates it.
  [[nodiscard]] const orbit::UniversalPropagator &propagator();

  // install a new orbit seed at `now_ut` and invalidate every orbit-derived
  // cache. the only place orbit_state_ is reassigned during stepping, so the
  // propagator cache cannot outlive the seed it was built from.
  void reseed_orbit(double now_ut, const orbit::StateVectors &state);

  const world::UniverseContext *context_;
  std::string name_;
  orbit::OrbitState orbit_state_;
  orbit::OrbitPatchSegments patches_;
  std::vector<Part> parts_;
  CraftControl control_;
  CraftResources resources_;
  CraftNewtonian newtonian_;
  // propagator for the current orbit seed. empty until first used or after a
  // reseed; when present it is built from the current orbit_state_.
  std::optional<orbit::UniversalPropagator> propagator_;
};

} // namespace flop::craft
