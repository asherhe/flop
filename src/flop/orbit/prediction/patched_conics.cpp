#include "flop/orbit/prediction/patched_conics.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <span>
#include <stdexcept>

#include "flop/orbit/prediction/encounter_search.hpp"
#include "flop/orbit/ref_frame.hpp"

namespace flop::orbit {
namespace {

constexpr std::size_t TRANSITION_LIMIT = 1024;

struct Candidate {
  double ut;
  FrameId destination;
};

} // namespace

const OrbitPatch *OrbitPatchSegments::patch_at(double ut) const {
  if (!std::isfinite(ut) || patches.empty() || ut > searched_through ||
      (ut == searched_through && !endpoint_resolved)) {
    return nullptr;
  }
  const auto next = std::ranges::upper_bound(patches, ut, {}, &OrbitPatch::begin_ut);
  return next == patches.begin() ? nullptr : &*std::prev(next);
}

SOITransitionSearchResult next_soi_transition(
    const RefFrameManager &frames,
    const OrbitState &orbit,
    double begin_ut,
    double end_ut,
    std::size_t step_limit
) {
  detail::validate_interval(begin_ut, end_ut);
  const auto &center = frames.frame(orbit.frame_id);
  const UniversalPropagator propagator(center.gravitational_parameter, orbit);
  std::optional<Candidate> candidate;
  double searched_through = end_ut;
  const auto consider = [&](const SOISearchResult &search, FrameId destination) {
    searched_through = std::min(searched_through, search.searched_through);
    // keep track of the most imminent transition.
    if (search.crossing_ut &&
        (!candidate || *search.crossing_ut < candidate->ut ||
         (*search.crossing_ut == candidate->ut && destination < candidate->destination))) {
      candidate = Candidate{*search.crossing_ut, destination};
    }
  };

  // check if the body has an SOI first.
  if (center.parent) {
    consider(first_soi_escape(frames, orbit, begin_ut, end_ut, step_limit), *center.parent);
  }
  for (const auto &child : frames.frames()) {
    if (child.parent != center.id) {
      continue;
    }
    consider(
        first_soi_entry(frames, orbit, child.id, begin_ut, searched_through, step_limit), child.id
    );
  }

  // another search's coverage can expire before the first predicted transition.
  if (!candidate || candidate->ut > searched_through) {
    return {std::nullopt, searched_through};
  }

  const auto state = propagator.state_at(candidate->ut);
  // use the body's state at the very same time as the capture or escape.
  // subtract it on capture and add it on escape through the frame offset.
  const auto offset = frames.offset(center.id, candidate->destination, candidate->ut);
  const StateVectors next_state{state.position + offset.position, state.velocity + offset.velocity};
  const double radius = next_state.position.length();
  if (!(radius > 0) || !std::isfinite(radius) || !is_finite(next_state.velocity)) {
    throw std::domain_error("SOI transition reached a singular or unrepresentable state");
  }
  return {
      SOITransition{center.id, {candidate->ut, next_state, candidate->destination}},
      candidate->ut,
  };
}

OrbitPatchSegments build_patches(
    const RefFrameManager &frames,
    const OrbitState &orbit,
    double begin_ut,
    double end_ut,
    std::size_t step_limit
) {
  OrbitPatchSegments prediction{{{orbit, begin_ut, begin_ut}}, begin_ut};
  // current time as we step through patches.
  double ut = begin_ut;
  std::size_t chain_begin = 0;
  for (std::size_t count = 0; count < TRANSITION_LIMIT; ++count) {
    const auto result =
        next_soi_transition(frames, prediction.patches.back().orbit, ut, end_ut, step_limit);
    prediction.patches.back().end_ut = result.searched_through;
    prediction.searched_through = result.searched_through;
    if (!result.transition) {
      return prediction;
    }
    if (result.searched_through != ut) {
      chain_begin = prediction.patches.size() - 1;
    }
    const auto &next_orbit = result.transition->next_orbit;
    const auto chain = std::span<const OrbitPatch>(prediction.patches).subspan(chain_begin);
    // follow every transition at this UT until the craft settles in a frame or repeats one.
    if (std::ranges::any_of(chain, [&](const OrbitPatch &patch) {
          return patch.orbit.frame_id == next_orbit.frame_id;
        })) {
      prediction.endpoint_resolved = false;
      return prediction;
    }

    // advance to the next patch at the transition time.
    ut = result.searched_through;
    prediction.patches.push_back({next_orbit, ut, ut});
  }
  prediction.endpoint_resolved = false;
  return prediction;
}

} // namespace flop::orbit
