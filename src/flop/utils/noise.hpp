#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace flop {

// stable seed construction for runtime identities; independent of std::hash.
inline std::uint64_t noise_seed(std::uint64_t seed, std::string_view value) noexcept {
  for (const unsigned char c : value) {
    seed ^= c;
    seed *= 1099511628211ULL;
  }
  return seed;
}

namespace detail {
inline std::uint64_t noise_mix(std::uint64_t value) noexcept {
  value ^= value >> 30;
  value *= 0xbf58476d1ce4e5b9ULL;
  value ^= value >> 27;
  value *= 0x94d049bb133111ebULL;
  return value ^ (value >> 31);
}

// lattice coordinates wrap at 2^32, including for negative coordinates.
template <std::size_t N>
double gradient_noise(const std::array<double, N> &position, std::uint64_t seed) noexcept {
  std::array<std::uint32_t, N> cell{};
  std::array<double, N> fraction{}, fade{};
  for (std::size_t i = 0; i < N; ++i) {
    if (!std::isfinite(position[i])) return 0.5;
    const double base = std::floor(position[i]);
    double wrapped = std::fmod(base, 4294967296.0);
    if (wrapped < 0.0) wrapped += 4294967296.0;
    cell[i] = static_cast<std::uint32_t>(wrapped);
    fraction[i] = position[i] - base;
    const double t = fraction[i];
    fade[i] = t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
  }
  double result = 0.0;
  for (std::size_t corner = 0; corner < (std::size_t{1} << N); ++corner) {
    std::uint64_t hash = seed;
    double weight = 1.0;
    std::array<double, N> delta{};
    for (std::size_t i = 0; i < N; ++i) {
      const bool high = (corner & (std::size_t{1} << i)) != 0;
      hash = noise_mix(hash ^ static_cast<std::uint32_t>(cell[i] + (high ? 1U : 0U)));
      delta[i] = fraction[i] - (high ? 1.0 : 0.0);
      weight *= high ? fade[i] : 1.0 - fade[i];
    }
    // bounded axis-aligned gradients keep the interpolated value in [-1, 1].
    const auto axis = static_cast<std::size_t>(hash % N);
    const double magnitude = N == 1 ? 0.5 + static_cast<double>((hash >> 16) & 255U) / 510.0 : 1.0;
    const double gradient = ((hash & 0x100U) ? delta[axis] : -delta[axis]) * magnitude;
    result += weight * gradient;
  }
  return std::clamp(0.5 + 0.5 * result, 0.0, 1.0);
}
} // namespace detail

// seeded gradient Perlin noise, normalized to [0, 1].
inline double perlin_noise(double x, std::uint64_t seed) noexcept {
  return detail::gradient_noise<1>({x}, seed);
}
inline double perlin_noise(double x, double y, std::uint64_t seed) noexcept {
  return detail::gradient_noise<2>({x, y}, seed);
}
inline double perlin_noise(double x, double y, double z, std::uint64_t seed) noexcept {
  return detail::gradient_noise<3>({x, y, z}, seed);
}

} // namespace flop
