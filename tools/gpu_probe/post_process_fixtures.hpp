#pragma once

#include <array>
#include <cstdint>

// An opaque red HDR sample and its exact RGBA16F representation.
inline constexpr std::array<float, 4> hdr_red{2.0f, 0.0f, 0.0f, 1.0f};
inline constexpr std::array<std::uint16_t, 4> hdr_red_half{0x4000, 0, 0, 0x3c00};
