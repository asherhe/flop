#pragma once

#include <cstdint>
#include <functional>

namespace flop::world {

// strong identifier for a craft within one Universe. assigned by
// Universe::add_craft in increasing order and never reused during a single
// world lifetime, so a stale id can be detected rather than silently aliasing a
// later craft. zero is the unassigned/invalid value; the first assigned id is 1.
// persistent save ids can be layered on later without changing this runtime id.
enum class CraftId : std::uint64_t { invalid = 0 };

} // namespace flop::world

template <> struct std::hash<flop::world::CraftId> {
  [[nodiscard]] std::size_t operator()(flop::world::CraftId id) const noexcept {
    return std::hash<std::uint64_t>{}(static_cast<std::uint64_t>(id));
  }
};
