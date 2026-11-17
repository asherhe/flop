#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <stdexcept>

#include "flop/orbit/prediction/encounter.hpp"
#include "flop/orbit/propagators/universal.hpp"
#include "flop/orbit/scale.hpp"

namespace flop::orbit::detail {

inline constexpr double ROUNDING_MARGIN = 32 * std::numeric_limits<double>::epsilon();

inline void validate_interval(double begin_ut, double end_ut) {
  if (!std::isfinite(begin_ut) || !std::isfinite(end_ut) || begin_ut > end_ut) {
    throw std::invalid_argument("Invalid encounter search interval");
  }
}

inline double max_component(Vec2d vector) {
  return std::max(std::abs(vector.x), std::abs(vector.y));
}

// the squared distance between two orbits is
//   d^2 = |p1 - p2|^2
// the derivative of this is
//   (d^2)' = 2 (p1 - p2) dot (v1 - v2)
// we remove the coefficient of 2 because we only need the sign.
// scale both vectors to keep the sign even when the dimensional product overflows.
inline double range_rate_sign(Vec2d displacement, Vec2d velocity) {
  if (!is_finite(displacement) || !is_finite(velocity)) {
    throw std::domain_error("Relative encounter state exceeds the representable domain");
  }
  const double distance_scale = max_component(displacement);
  const double speed_scale = max_component(velocity);
  if (distance_scale == 0 || speed_scale == 0) {
    return 0;
  }
  return (displacement / distance_scale).dot(velocity / speed_scale);
}

inline double local_time_scale(const StateVectors &state, double mu) {
  const double radius = state.position.length();
  const double speed = state.velocity.length();
  const double circular_speed = detail::circular_speed(mu, radius);
  const double larger = std::max(speed, circular_speed);
  const double smaller = std::min(speed, circular_speed);
  if (!(radius > 0) || !std::isfinite(radius) || !(larger > 0) || !std::isfinite(larger)) {
    throw std::domain_error("Encounter time scale exceeds the representable domain");
  }
  return multiply_divide(radius, 1 / (8 * (1 + smaller / larger)), larger);
}

struct Sample {
  double ut;
  StateVectors first;
  StateVectors second;

  // distance between the two orbits at the sample time
  [[nodiscard]] double separation() const { return (first.position - second.position).length(); }
  [[nodiscard]] double rate() const {
    return range_rate_sign(first.position - second.position, first.velocity - second.velocity);
  }

  // the differences inherit roundoff from the original states. compare the
  // normalized dot product with that uncertainty before using its sign.
  [[nodiscard]] double resolved_rate() const {
    const double value = rate();
    if (value == 0) {
      return 0;
    }
    const double position_scale =
        std::max(max_component(first.position), max_component(second.position));
    const double velocity_scale =
        std::max(max_component(first.velocity), max_component(second.velocity));
    const double position_error =
        (ROUNDING_MARGIN * position_scale) / max_component(first.position - second.position);
    const double velocity_error =
        (ROUNDING_MARGIN * velocity_scale) / max_component(first.velocity - second.velocity);
    return std::abs(value) > position_error + velocity_error ? value : 0.0;
  }

  [[nodiscard]] double separation_error(double radius) const {
    return ROUNDING_MARGIN *
           std::max({radius, max_component(first.position), max_component(second.position)});
  }
};

class Search {
public:
  Search(double mu, const UniversalPropagator &first, const UniversalPropagator *second) :
      mu_(mu), first_(first), second_(second) {}

  [[nodiscard]] Sample at(double ut) const {
    return {ut, first_.state_at(ut), second_ ? second_->state_at(ut) : StateVectors{}};
  }

  [[nodiscard]] Sample next(const Sample &current, double end_ut) const {
    double step = local_time_scale(current.first, mu_);
    if (second_) {
      step = std::min(step, local_time_scale(current.second, mu_));
    }
    const double future = std::max(current.ut + step, std::nextafter(current.ut, end_ut));
    return at(std::min(future, end_ut));
  }

private:
  double mu_;
  const UniversalPropagator &first_;
  const UniversalPropagator *second_;
};

// bracket an upward zero crossing and return its post-crossing side.
template <class Value>
Sample crossing(const Search &search, Sample lower, Sample upper, Value value) {
  while (true) {
    const double mid = std::midpoint(lower.ut, upper.ut);
    if (mid == lower.ut || mid == upper.ut) {
      return upper;
    }
    const Sample sample = search.at(mid);
    if (value(sample) >= 0) {
      upper = sample;
    } else {
      lower = sample;
    }
  }
}

inline SOISearchResult search_sphere_crossing(
    const Search &search,
    Sample previous,
    double radius,
    double direction,
    double end_ut,
    std::size_t step_limit
) {
  // signed distance to the SOI edge. the sign is positive on the destination
  // side for either capture or escape.
  const auto distance = [=](const Sample &point) {
    return direction * (point.separation() - radius);
  };
  const auto rate = [=](const Sample &point) { return direction * point.rate(); };
  const bool entering = direction < 0;
  const auto resolved_rate = [=](const Sample &point) {
    return entering ? direction * point.resolved_rate() : rate(point);
  };
  if (previous.ut < end_ut && distance(previous) >= 0 && resolved_rate(previous) > 0) {
    return {previous.ut, previous.ut};
  }
  for (std::size_t step = 0; step < step_limit && previous.ut < end_ut; ++step) {
    const Sample next = search.next(previous, end_ut);
    if (distance(previous) < 0) {
      if (distance(next) >= 0 && resolved_rate(next) > 0) {
        const double ut = crossing(search, previous, next, distance).ut;
        return {ut, ut};
      }
      // a complete flyby can fit between samples even when neither endpoint
      // falls inside the sphere. find the intervening closest approach, then
      // search between the earlier sample and that approach for the capture time.
      // for escape, use the farthest point to bracket the outward crossing.
      if (resolved_rate(previous) > 0 && rate(next) <= 0) {
        const auto extreme =
            crossing(search, previous, next, [&](const Sample &point) { return -rate(point); });
        if (distance(extreme) > (entering ? extreme.separation_error(radius) : 0)) {
          const double ut = crossing(search, previous, extreme, distance).ut;
          return {ut, ut};
        }
      }
    }
    previous = next;
  }
  return {std::nullopt, previous.ut};
}

} // namespace flop::orbit::detail
