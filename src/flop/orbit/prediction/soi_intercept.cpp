#include <stdexcept>

#include "flop/orbit/prediction/encounter.hpp"
#include "flop/orbit/prediction/encounter_search.hpp"
#include "flop/orbit/ref_frame.hpp"

namespace flop::orbit {
namespace {

// the reverse triangle inequality bounds separation by the gap between the
// two radial ranges. leave a roundoff margin before rejecting a grazing entry.
bool radial_gap_exceeds(double periapsis, double apoapsis, double radius) {
  return periapsis - apoapsis - radius > detail::ROUNDING_MARGIN * periapsis;
}

bool cannot_enter(double mu, const OrbitState &subject, const OrbitState &center, double radius) {
  try {
    const auto subject_bounds = geometry(mu, subject);
    const auto center_bounds = geometry(mu, center);
    return radial_gap_exceeds(subject_bounds.periapsis, center_bounds.apoapsis, radius) ||
           radial_gap_exceeds(center_bounds.periapsis, subject_bounds.apoapsis, radius);
  } catch (const std::domain_error &) {
    // propagation can still work when a dimensional geometry result overflows.
    return false;
  }
}

} // namespace

SOISearchResult first_soi_entry(
    const RefFrameManager &frames,
    const OrbitState &subject,
    FrameId target,
    double begin_ut,
    double end_ut,
    std::size_t step_limit
) {
  detail::validate_interval(begin_ut, end_ut);
  const auto &parent = frames.frame(subject.frame_id);
  const auto &center = frames.frame(target);
  if (center.parent != parent.id) {
    throw std::invalid_argument("SOI intercept target must be a child of the subject's frame");
  }
  const double mu = parent.gravitational_parameter;
  const double radius = center.sphere_of_influence_radius;
  const UniversalPropagator first(mu, subject);
  if (cannot_enter(mu, subject, center.orbit_about_parent.value(), radius)) {
    return {std::nullopt, end_ut};
  }
  const detail::Search search(mu, first, &frames.ephemeris_about_parent(target));
  return detail::search_sphere_crossing(
      search, search.at(begin_ut), radius, -1, end_ut, step_limit
  );
}

} // namespace flop::orbit
