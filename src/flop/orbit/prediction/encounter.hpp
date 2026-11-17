#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "flop/orbit/state.hpp"

namespace flop::orbit {

class RefFrameManager;

// states at a local minimum of separation, in their shared reference frame
struct Encounter {
  // time of the encounter
  double ut;
  // state of the current orbit at the encounter
  StateVectors first;
  // state of the other orbit at the encounter
  StateVectors second;
};

struct EncounterSearchResult {
  std::vector<Encounter> encounters;
  // last time sampled or the end of the requested interval if the search finished
  double searched_through;
};

struct SOISearchResult {
  // time at the SOI edge, where the orbit would be reseeded in the next frame
  std::optional<double> crossing_ut;
  // the crossing, the last time sampled if the budget ran out, or the requested end
  double searched_through;
};

inline constexpr std::size_t DEFAULT_ENCOUNTER_STEP_LIMIT = 250000;

// gets a list of encounters with the given orbit over the requested time interval.
// both orbits must name the same frame in frames. propagation uses that frame's gravity.
// a minimum is confirmed by samples with distinguishable approach and recession rates.
// the step limit bounds adaptive sampling. bracket refinement is separate.
[[nodiscard]] EncounterSearchResult closest_approaches(
    const RefFrameManager &frames,
    const OrbitState &first,
    const OrbitState &second,
    double begin_ut,
    double end_ut,
    std::size_t step_limit = DEFAULT_ENCOUNTER_STEP_LIMIT
);

// detects an intercept with a celestial body at the first inward crossing of its SOI.
// the target must be a child of the subject's frame. both orbits use the shared
// parent frame's gravity, and the SOI radius comes from the target frame.
// a subject already inside and moving inward at begin_ut reports an immediate crossing.
// a singleton interval only checks for that immediate crossing.
// to express the state in the capturing body's frame, subtract that body's position
// and velocity evaluated at the time of capture.
[[nodiscard]] SOISearchResult first_soi_entry(
    const RefFrameManager &frames,
    const OrbitState &subject,
    FrameId target,
    double begin_ut,
    double end_ut,
    std::size_t step_limit = DEFAULT_ENCOUNTER_STEP_LIMIT
);

// detects escape from the SOI of the current celestial body, centered at the reference
// origin. a subject already outside and moving outward at begin_ut reports an
// immediate crossing. the root frame has no escape crossing.
// a singleton interval only checks for an immediate crossing.
// to express the state in the parent frame, add the current central body's
// position and velocity relative to its parent at the time of escape.
// the step limit applies to the numerical fallback.
[[nodiscard]] SOISearchResult first_soi_escape(
    const RefFrameManager &frames,
    const OrbitState &subject,
    double begin_ut,
    double end_ut,
    std::size_t step_limit = DEFAULT_ENCOUNTER_STEP_LIMIT
);

} // namespace flop::orbit
