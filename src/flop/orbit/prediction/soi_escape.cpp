#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>

#include "flop/orbit/prediction/encounter.hpp"
#include "flop/orbit/prediction/encounter_search.hpp"
#include "flop/orbit/ref_frame.hpp"
#include "flop/orbit/scale.hpp"

namespace flop::orbit {
namespace {

// get elapsed time from the universal anomaly using the universal kepler
// equation. in dimensional units the result is divided by sqrt(mu). here chi
// and the conic are normalized, so the caller converts the elapsed time back.
// use the series near alpha = 0 to preserve the small cubic term.
double time_from_periapsis(double chi, double periapsis, double eccentricity, double alpha) {
  const double root_alpha = std::sqrt(std::abs(alpha));
  const double anomaly = root_alpha * chi;
  double chi_cubed_s;
  if (std::abs(anomaly) < 0.1) {
    const double z = std::copysign(anomaly * anomaly, alpha);
    chi_cubed_s = chi * chi * chi * (1.0 / 6 - z * (1.0 / 120 - z * (1.0 / 5040 - z / 362880)));
  } else {
    const double residual = alpha > 0 ? anomaly - std::sin(anomaly) : std::sinh(anomaly) - anomaly;
    chi_cubed_s = residual / (root_alpha * std::abs(alpha));
  }
  return periapsis * chi + eccentricity * chi_cubed_s;
}

// an empty outer result requests the sampled fallback.
std::optional<SOISearchResult> analytic_escape(
    double mu,
    const UniversalPropagator &propagator,
    const detail::Sample &start,
    double radius,
    double end_ut
) {
  const auto [position, velocity] = start.first;
  const double start_radius = position.length();
  const double start_rate = start.rate();
  if (start_radius >= radius && start_rate > 0) {
    return SOISearchResult{start.ut, start.ut};
  }

  // normalize by the initial radius and local circular speed before computing
  // the conic. outbound and inbound passages have opposite times from periapsis.
  const double speed_scale = detail::circular_speed(mu, start_radius);
  const Vec2d radial = position / start_radius;
  const Vec2d scaled_velocity = velocity / speed_scale;
  const double radial_speed = radial.dot(scaled_velocity);
  const double transverse_speed = radial.cross(scaled_velocity);
  const double transverse_squared = transverse_speed * transverse_speed;
  const double alpha = 2 - detail::relative_speed_squared(velocity, start_radius, mu);
  const double root_alpha = std::sqrt(std::abs(alpha));
  const double eccentricity =
      alpha > 0 ? std::hypot(1 - alpha, root_alpha * radial_speed)
                : std::hypot(transverse_squared - 1, transverse_speed * radial_speed);
  const double periapsis = alpha > 0 && eccentricity < 0.5
                               ? (1 - eccentricity) / alpha
                               : transverse_squared / (1 + eccentricity);
  const double target = radius / start_radius;
  const double apoapsis =
      alpha > 0 ? (1 + eccentricity) / alpha : std::numeric_limits<double>::infinity();
  // check whether the orbit will cross the SOI. touching it at an apsis
  // does not take the orbit across the boundary.
  if (target <= periapsis || target >= apoapsis) {
    return SOISearchResult{std::nullopt, end_ut};
  }
  if (eccentricity <= detail::ROUNDING_MARGIN || !std::isfinite(target)) {
    return std::nullopt;
  }

  // determine the anomaly at the SOI intersection. invert the radius
  // r = q + e chi^2 C(alpha chi^2) to get the outbound universal anomaly.
  // at the starting radius use the radial velocity to retain precision near periapsis.
  double start_chi;
  double exit_chi;
  if (alpha > 0) {
    // keep the starting eccentric anomaly in [-pi, pi] and the outbound one
    // in [0, pi], so the two sides of apoapsis can be distinguished.
    start_chi = std::atan2(root_alpha * radial_speed, 1 - alpha) / root_alpha;
    exit_chi =
        2 * std::atan2(std::sqrt(target - periapsis), std::sqrt(apoapsis - target)) / root_alpha;
  } else {
    const double half_parabolic_chi = std::sqrt((target - periapsis) / (2 * eccentricity));
    if (alpha < 0) {
      start_chi = std::asinh(root_alpha * (radial_speed / eccentricity)) / root_alpha;
      exit_chi = 2 * std::asinh(root_alpha * half_parabolic_chi) / root_alpha;
    } else {
      start_chi = radial_speed / eccentricity;
      exit_chi = 2 * half_parabolic_chi;
    }
  }
  const double start_phase = time_from_periapsis(start_chi, periapsis, eccentricity, alpha);
  const double exit_phase = time_from_periapsis(exit_chi, periapsis, eccentricity, alpha);
  double elapsed = exit_phase - start_phase;
  if (elapsed <= 0 && alpha > 0) {
    // the outbound intersection is on the next period if it has already passed.
    elapsed += 2 * std::numbers::pi / (alpha * std::sqrt(alpha));
  }
  elapsed = detail::multiply_divide(elapsed, start_radius, speed_scale);
  double candidate = start.ut + elapsed;
  if (!(elapsed > 0) || !std::isfinite(candidate)) {
    return std::nullopt;
  }

  const double time_margin =
      detail::ROUNDING_MARGIN * std::max(std::abs(candidate), std::abs(elapsed));
  if (candidate - end_ut > time_margin) {
    return SOISearchResult{std::nullopt, end_ut};
  }
  candidate = std::clamp(candidate, std::nextafter(start.ut, end_ut), end_ut);
  // verify this is the outbound passage. correct small residuals in time while
  // keeping the reported sample on the post-crossing side of the boundary.
  for (int refinement = 0; refinement < 4; ++refinement) {
    const auto state = propagator.state_at(candidate);
    const double distance = state.position.length();
    const double outward_speed = (state.position / distance).dot(state.velocity);
    if (!(outward_speed > 0)) {
      break;
    }
    const double residual = distance - radius;
    if (residual >= 0) {
      const double previous_ut = std::nextafter(candidate, start.ut);
      // at large UT the radius can jump across the boundary between adjacent times.
      if (residual <= detail::ROUNDING_MARGIN * radius ||
          (residual <=
               outward_speed * (candidate - previous_ut) + detail::ROUNDING_MARGIN * radius &&
           propagator.position_at(previous_ut).length() < radius)) {
        return SOISearchResult{candidate, candidate};
      }
    }
    const double corrected = candidate - residual / outward_speed;
    const double next = std::clamp(
        corrected == candidate ? std::nextafter(candidate, residual < 0 ? end_ut : start.ut)
                               : corrected,
        std::nextafter(start.ut, end_ut),
        end_ut
    );
    if (next == candidate) {
      break;
    }
    candidate = next;
  }
  return std::nullopt;
}

} // namespace

SOISearchResult first_soi_escape(
    const RefFrameManager &frames,
    const OrbitState &subject,
    double begin_ut,
    double end_ut,
    std::size_t step_limit
) {
  detail::validate_interval(begin_ut, end_ut);
  const auto &center = frames.frame(subject.frame_id);
  const double mu = center.gravitational_parameter;
  const UniversalPropagator propagator(mu, subject);
  if (!center.parent || begin_ut == end_ut) {
    return {std::nullopt, end_ut};
  }
  const double radius = center.sphere_of_influence_radius;
  const detail::Search search(mu, propagator, nullptr);
  const detail::Sample start = search.at(begin_ut);
  if (const auto result = analytic_escape(mu, propagator, start, radius, end_ut)) {
    return *result;
  }
  return detail::search_sphere_crossing(search, start, radius, 1, end_ut, step_limit);
}

} // namespace flop::orbit
