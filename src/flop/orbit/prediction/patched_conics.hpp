#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "flop/orbit/prediction/encounter.hpp"
#include "flop/orbit/state.hpp"

namespace flop::orbit {

// a single conic patch of an orbit. its epoch stays fixed while coasting.
struct OrbitPatch {
  OrbitState orbit;
  double begin_ut;
  double end_ut;
};

// state on the other end of a predicted SOI transition, at the start of the next orbit
struct SOITransition {
  FrameId from;
  // the epoch is the time of transition, and frame_id names the destination
  OrbitState next_orbit;
};

struct SOITransitionSearchResult {
  std::optional<SOITransition> transition;
  // furthest UT searched in common by the eligible entry and escape queries
  double searched_through;
};

// the first conic patch is the current orbit, followed by each predicted patch in time order.
struct OrbitPatchSegments {
  std::vector<OrbitPatch> patches;
  // when the prediction runs out of coverage, the caller can restart at this UT
  double searched_through;

  // at a transition UT the new patch takes over. the final patch includes its end UT.
  [[nodiscard]] const OrbitPatch *patch_at(double ut) const;
};

// check the current frame's escape and each child SOI intercept. the earliest
// crossing wins once the other searches have reached its UT. when crossings tie,
// use the lowest destination frame id. the step limit applies to each SOI search.
[[nodiscard]] SOITransitionSearchResult next_soi_transition(
    const RefFrameManager &frames,
    const OrbitState &orbit,
    double begin_ut,
    double end_ut,
    std::size_t step_limit = DEFAULT_ENCOUNTER_STEP_LIMIT
);

// calculate future trajectory patches from the given time along the starting patch.
// a search or transition limit can stop the prediction before end_ut.
[[nodiscard]] OrbitPatchSegments build_patches(
    const RefFrameManager &frames,
    const OrbitState &orbit,
    double begin_ut,
    double end_ut,
    std::size_t step_limit = DEFAULT_ENCOUNTER_STEP_LIMIT
);

} // namespace flop::orbit
