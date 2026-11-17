#pragma once

#include <cstdint>

namespace flop::orbit {

// runtime id assigned by the manager, scoped to its frame hierarchy. zero is unassigned.
enum class FrameId : std::uint32_t {};

} // namespace flop::orbit
