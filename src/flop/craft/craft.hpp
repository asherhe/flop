#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "flop/craft/config.hpp"
#include "flop/craft/control.hpp"
#include "flop/craft/craft_context.hpp"
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
      std::vector<PartTypeConfig> part_types = {},
      std::uint64_t runtime_id = 0
  );

  [[nodiscard]] std::string_view name() const noexcept { return name_; }
  [[nodiscard]] double ut_sec() const noexcept override { return context_->ut_sec(); }
  [[nodiscard]] std::uint64_t runtime_id() const noexcept override { return runtime_id_; }
  [[nodiscard]] const orbit::OrbitState &orbit_state() const noexcept { return orbit_state_; }
  [[nodiscard]] const orbit::OrbitPatchSegments &patches() const noexcept { return patches_; }
  [[nodiscard]] std::span<const Part> parts() const noexcept override { return parts_; }
  [[nodiscard]] std::span<Part> mutable_parts() noexcept override { return parts_; }
  [[nodiscard]] const Part &part(std::string_view id) const;
  // mutable access to an installed part, for feeding pilot input into a part's
  // plugins (e.g. a command pod's throttle/steering).
  [[nodiscard]] Part &part(std::string_view id);

  // the installed part with the given id, or nullptr if this craft has none.
  // unlike part(id), this does not throw, so a queued command targeting a part
  // that no longer exists fails cleanly.
  [[nodiscard]] const Part *find_part(const PartId &part_id) const;
  [[nodiscard]] Part *find_part(const PartId &part_id);

  [[nodiscard]] const CraftControl &control() const noexcept override { return control_; }
  [[nodiscard]] CraftControl &control() noexcept override { return control_; }

  [[nodiscard]] const CraftResources &resources() const noexcept override { return resources_; }
  [[nodiscard]] CraftResources &resources() noexcept override { return resources_; }

  [[nodiscard]] const CraftNewtonian &newtonian() const noexcept override { return newtonian_; }
  [[nodiscard]] CraftNewtonian &newtonian() noexcept override { return newtonian_; }

  [[nodiscard]] MassProperties mass_properties() const override {
    return resources_.mass_properties(*this);
  }

  // advance Craft simulation by dt_sec. runs PartPlugin steps in two phases
  // (control, then actuation), then advances CraftNewtonian simulation.
  void step(double now_ut, double dt_sec);

  // position and velocity of the craft at the requested UT, in its current
  // orbit's frame. a const query for rendering and read-only inspection: it
  // reuses the cached propagator when one is already built from the current
  // seed and otherwise builds a throwaway propagator, so calling it every
  // displayed frame neither rebuilds the step cache nor mutates the craft.
  [[nodiscard]] orbit::StateVectors state_at(double ut) const;

  // frame the craft's orbit is currently expressed in.
  [[nodiscard]] orbit::FrameId frame_id() const noexcept { return orbit_state_.frame_id; }

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
  std::uint64_t runtime_id_{};
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
