#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace flop {

// one key in a keyframed curve
template <typename T> struct CurveKey {
  double time{};
  T value{};
};

struct CurveSegment {
  std::size_t left;
  std::size_t right;
};

// time must lie strictly between the endpoints of ordered, distinct keys.
template <typename T>
[[nodiscard]] CurveSegment find_curve_segment(std::span<const CurveKey<T>> keys, double time) {
  const auto right =
      std::upper_bound(keys.begin(), keys.end(), time, [](double t, const auto &key) {
        return t < key.time;
      });
  const auto index = static_cast<std::size_t>(right - keys.begin());
  return {index - 1, index};
}

template <typename T> struct LinearInterpolation {
  [[nodiscard]] T operator()(std::span<const CurveKey<T>> keys, double time) const {
    const auto segment = find_curve_segment(keys, time);
    const auto &left = keys[segment.left];
    const auto &right = keys[segment.right];
    const double t = (time - left.time) / (right.time - left.time);
    return left.value * (1.0 - t) + right.value * t;
  }
};

// monotone piecewise cubic hermite interpolation with inferred tangents.
struct SmoothInterpolation {
  [[nodiscard]] double operator()(std::span<const CurveKey<double>> keys, double time) const {
    const auto segment = find_curve_segment(keys, time);
    const auto &left = keys[segment.left];
    const auto &right = keys[segment.right];
    const double h = right.time - left.time;
    const double t = (time - left.time) / h;
    const double t2 = t * t;
    const double t3 = t2 * t;
    return (2 * t3 - 3 * t2 + 1) * left.value +
           (t3 - 2 * t2 + t) * h * tangent(keys, segment.left) + (-2 * t3 + 3 * t2) * right.value +
           (t3 - t2) * h * tangent(keys, segment.right);
  }

private:
  static double secant(std::span<const CurveKey<double>> keys, std::size_t i) {
    return (keys[i + 1].value - keys[i].value) / (keys[i + 1].time - keys[i].time);
  }

  static double endpoint(double h0, double h1, double d0, double d1) {
    const double slope = ((2 * h0 + h1) * d0 - h0 * d1) / (h0 + h1);
    if (d0 == 0 || std::signbit(slope) != std::signbit(d0))
      return 0;
    if (std::signbit(d0) != std::signbit(d1) && std::abs(slope) > 3 * std::abs(d0))
      return 3 * d0;
    return slope;
  }

  static double tangent(std::span<const CurveKey<double>> keys, std::size_t i) {
    if (keys.size() == 2)
      return secant(keys, 0);
    if (i == 0)
      return endpoint(
          keys[1].time - keys[0].time, keys[2].time - keys[1].time, secant(keys, 0), secant(keys, 1)
      );
    if (i == keys.size() - 1)
      return endpoint(
          keys[i].time - keys[i - 1].time,
          keys[i - 1].time - keys[i - 2].time,
          secant(keys, i - 1),
          secant(keys, i - 2)
      );
    const double d0 = secant(keys, i - 1), d1 = secant(keys, i);
    if (d0 == 0 || d1 == 0 || std::signbit(d0) != std::signbit(d1))
      return 0;
    const double h0 = keys[i].time - keys[i - 1].time;
    const double h1 = keys[i + 1].time - keys[i].time;
    const double w0 = 2 * h1 + h0, w1 = h1 + 2 * h0;
    return (w0 + w1) / (w0 / d0 + w1 / d1);
  }
};

// a keyframed curve. the policy receives the ordered keys and an interior time.
template <typename T, typename Interpolation = LinearInterpolation<T>> class KeyCurve {
public:
  using Key = CurveKey<T>;

  KeyCurve() = default;
  explicit KeyCurve(std::vector<Key> keys, Interpolation interpolation = {}) :
      keys_(std::move(keys)), interpolation_(std::move(interpolation)) {
    for (const auto &key : keys_)
      if (!std::isfinite(key.time))
        throw std::invalid_argument("Curve key must be finite");
    std::ranges::sort(keys_, {}, &Key::time);
    for (std::size_t i = 1; i < keys_.size(); ++i)
      if (keys_[i - 1].time == keys_[i].time)
        throw std::invalid_argument("Duplicate curve key");
  }

  [[nodiscard]] bool empty() const noexcept { return keys_.empty(); }
  [[nodiscard]] std::span<const Key> keys() const noexcept { return keys_; }

  [[nodiscard]] T evaluate(double time) const {
    if (keys_.empty())
      throw std::logic_error("Cannot evaluate an empty curve");
    if (!std::isfinite(time))
      throw std::invalid_argument("Curve time must be finite");
    if (time <= keys_.front().time)
      return keys_.front().value;
    if (time >= keys_.back().time)
      return keys_.back().value;
    const auto next =
        std::lower_bound(keys_.begin(), keys_.end(), time, [](const auto &key, double t) {
          return key.time < t;
        });
    if (next->time == time)
      return next->value;
    return interpolation_(std::span<const Key>(keys_), time);
  }

private:
  std::vector<Key> keys_;
  [[no_unique_address]] Interpolation interpolation_;
};

} // namespace flop
