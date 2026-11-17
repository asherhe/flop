#include "flop/orbit/prediction/encounter.hpp"

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
  // search for brackets where extrema exist. a change from negative to
  // nonnegative in the derivative identifies a local minimum of distance.
  for (std::size_t step = 0; step < step_limit && previous.ut < end_ut; ++step) {
    const detail::Sample next = search.next(previous, end_ut);
    if (previous.rate() < 0 && next.rate() >= 0) {
      // we've bracketed a local minimum of distance. refine its time and
      // retain the states of both orbits at that encounter.
      const detail::Sample approach =
          detail::crossing(search, previous, next, [](const detail::Sample &point) {
            return point.rate();
          });
      result.encounters.push_back({approach.ut, approach.first, approach.second});
    }
    previous = next;
  }
  result.searched_through = previous.ut;
  return result;
}

} // namespace flop::orbit
