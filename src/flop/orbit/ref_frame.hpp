#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "flop/orbit/frame_id.hpp"
#include "flop/orbit/propagators/universal.hpp"

namespace flop::orbit {

// input for building a frame hierarchy. names are unique, and parent_name names
// another definition in the same hierarchy regardless of input order.
struct RefFrameDefinition {
  std::string name;
  double gravitational_parameter;
  std::optional<std::string> parent_name;
  // the manager assigns the seed's frame id from parent_name
  std::optional<OrbitState> orbit_about_parent;
  double sphere_of_influence_radius;
};

// one frame in the hierarchy, with its origin at a body's center and axes parallel
// to its parent's axes. the root has no parent or parent-relative orbit.
struct RefFrame {
  FrameId id;
  std::string name;
  // this body's gravitational parameter, used to propagate its children's orbits
  double gravitational_parameter;
  std::optional<FrameId> parent;
  // the seed for this body's motion relative to its parent, using the parent's gravity
  std::optional<OrbitState> orbit_about_parent;
  // boundary around this body for sphere-of-influence prediction
  double sphere_of_influence_radius;
};

// assigns frame ids, owns and validates the tree, and prepares each child orbit once.
// frame() retrieves one body's data, and offset() evaluates the motion between two
// frame origins at a shared time.
class RefFrameManager {
public:
  explicit RefFrameManager(std::vector<RefFrameDefinition> definitions);
  RefFrameManager(const RefFrameManager &) = default;
  RefFrameManager(RefFrameManager &&) noexcept = default;
  RefFrameManager &operator=(RefFrameManager other) noexcept;

  [[nodiscard]] FrameId frame_id(std::string_view name) const;
  [[nodiscard]] const RefFrame &frame(FrameId id) const;
  [[nodiscard]] std::span<const RefFrame> frames() const { return frames_; }

  // prepared motion of this frame relative to its parent. the root has no ephemeris.
  [[nodiscard]] const UniversalPropagator &ephemeris_about_parent(FrameId id) const;

  // get the position and velocity of the 'from' origin relative to the 'to' origin.
  // add these to a state expressed in 'from' to express it in 'to'.
  [[nodiscard]] StateVectors offset(FrameId from, FrameId to, double ut) const;

private:
  [[nodiscard]] std::size_t frame_index(FrameId id) const;

  std::vector<RefFrame> frames_;
  // indexed alongside frames_; the root has no parent-relative ephemeris
  std::vector<std::optional<UniversalPropagator>> ephemerides_;
};

} // namespace flop::orbit
