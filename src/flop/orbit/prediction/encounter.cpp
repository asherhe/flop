#include "flop/orbit/prediction/encounter.hpp"

#include <optional>
#include <stdexcept>

#include "flop/orbit/prediction/encounter_search.hpp"
#include "flop/orbit/ref_frame.hpp"

namespace flop::orbit {

EncounterSearchResult closest_approaches(
    const RefFrameManager &frames,
    const OrbitState &first,
    const OrbitState &second,
    double begin_ut,
    double end_ut,
    std::size_t step_limit
) {
  detail::validate_interval(begin_ut, end_ut);
  if (first.frame_id != second.frame_id) {
    throw std::invalid_argument("Encounter orbits must share a reference frame");
  }
  const double mu = frames.frame(first.frame_id).gravitational_parameter;
  const UniversalPropagator first_propagator(mu, first);
  const UniversalPropagator second_propagator(mu, second);
  const detail::Search search(mu, first_propagator, &second_propagator);
  EncounterSearchResult result{{}, begin_ut};
  detail::Sample previous = search.at(begin_ut);
  std::optional<detail::Sample> approaching;
  if (previous.resolved_rate() < 0) {
    approaching = previous;
  }
  // search for brackets where extrema exist. confirm a change from
  // approaching to receding across samples whose signs are resolved.
  for (std::size_t step = 0; step < step_limit && previous.ut < end_ut; ++step) {
    const detail::Sample next = search.next(previous, end_ut);
    const double rate = next.resolved_rate();
    if (rate < 0) {
      approaching = next;
    } else if (rate > 0 && approaching) {
      // we've bracketed a local minimum of distance. refine its time and
      // retain the states of both orbits at that encounter.
      const detail::Sample approach =
          detail::crossing(search, *approaching, next, [](const detail::Sample &point) {
            return point.rate();
          });
      result.encounters.push_back({approach.ut, approach.first, approach.second});
      approaching.reset();
    }
    previous = next;
  }
  result.searched_through = previous.ut;
  return result;
}

} // namespace flop::orbit
