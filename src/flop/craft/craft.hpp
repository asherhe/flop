#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "flop/craft/craft_config.hpp"
#include "flop/craft/craft_context.hpp"
#include "flop/craft/newtonian.hpp"
#include "flop/craft/parts/part.hpp"
#include "flop/craft/resources.hpp"
#include "flop/orbit/prediction/patched_conics.hpp"
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
  const world::UniverseContext *context_;
  std::string name_;
  orbit::OrbitState orbit_state_;
  orbit::OrbitPatchSegments patches_;
  std::vector<Part> parts_;
  CraftResources resources_;
  CraftNewtonian newtonian_;
};

} // namespace flop::craft
