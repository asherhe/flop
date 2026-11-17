#include "flop/orbit/ref_frame.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace flop::orbit {
namespace {

void accumulate(StateVectors &total, const StateVectors &step, double sign) {
  total.position += sign * step.position;
  total.velocity += sign * step.velocity;
}

} // namespace

RefFrameManager::RefFrameManager(std::vector<RefFrameDefinition> definitions) {
  if (definitions.size() > std::numeric_limits<std::uint32_t>::max()) {
    throw std::length_error("Too many orbital reference frames");
  }

  frames_.reserve(definitions.size());
  for (auto &definition : definitions) {
    if (definition.name.empty() ||
        std::ranges::find(frames_, definition.name, &RefFrame::name) != frames_.end()) {
      throw std::invalid_argument("Orbital frame names must be nonempty and unique");
    }
    const auto id = static_cast<FrameId>(frames_.size() + 1);
    frames_.push_back({
        id,
        std::move(definition.name),
        definition.gravitational_parameter,
        std::nullopt,
        std::move(definition.orbit_about_parent),
        definition.sphere_of_influence_radius,
    });
  }

  for (std::size_t index = 0; index < frames_.size(); ++index) {
    const auto &parent_name = definitions.at(index).parent_name;
    auto &node = frames_.at(index);
    if (parent_name.has_value() != node.orbit_about_parent.has_value()) {
      throw std::invalid_argument("Invalid orbital reference frame");
    }
    if (parent_name) {
      const auto parent_id = frame_id(*parent_name);
      node.parent = parent_id;
      node.orbit_about_parent.value().frame_id = parent_id;
    }
  }

  if (std::ranges::count_if(frames_, [](const auto &node) { return !node.parent; }) != 1) {
    throw std::invalid_argument("An ephemeris requires one root frame");
  }

  for (const auto &node : frames_) {
    if (!std::isfinite(node.gravitational_parameter) || node.gravitational_parameter <= 0) {
      throw std::invalid_argument("Invalid orbital reference frame");
    }
    if (node.parent &&
        (!std::isfinite(node.sphere_of_influence_radius) || node.sphere_of_influence_radius <= 0)) {
      throw std::invalid_argument("Invalid child sphere of influence");
    }
    const RefFrame *ancestor = &node;
    for (std::size_t depth = 0; ancestor->parent; ++depth) {
      if (depth >= frames_.size() - 1) {
        throw std::invalid_argument("Cyclic frame hierarchy");
      }
      ancestor = &frame(*ancestor->parent);
    }
  }

  ephemerides_.resize(frames_.size());
  for (std::size_t index = 0; index < frames_.size(); ++index) {
    const auto &node = frames_[index];
    if (node.parent) {
      const auto &parent = frame(*node.parent);
      ephemerides_[index].emplace(parent.gravitational_parameter, *node.orbit_about_parent);
    }
  }
}

RefFrameManager &RefFrameManager::operator=(RefFrameManager other) noexcept {
  frames_.swap(other.frames_);
  ephemerides_.swap(other.ephemerides_);
  return *this;
}

std::size_t RefFrameManager::frame_index(FrameId id) const {
  const auto number = static_cast<std::uint32_t>(id);
  if (number == 0 || number > frames_.size()) {
    throw std::invalid_argument("Unknown orbital frame");
  }
  return number - 1;
}

FrameId RefFrameManager::frame_id(std::string_view name) const {
  const auto result =
      std::ranges::find_if(frames_, [name](const RefFrame &frame) { return frame.name == name; });
  if (result == frames_.end()) {
    throw std::invalid_argument("Unknown orbital frame name: " + std::string(name));
  }
  return result->id;
}

const RefFrame &RefFrameManager::frame(FrameId id) const {
  return frames_[frame_index(id)];
}

StateVectors RefFrameManager::offset(FrameId from, FrameId to, double ut) const {
  if (!std::isfinite(ut)) {
    throw std::invalid_argument("Invalid frame evaluation time");
  }
  const auto path = [this](FrameId id) {
    std::vector<std::size_t> ancestors;
    for (std::optional<FrameId> current = id; current;) {
      const auto index = frame_index(*current);
      ancestors.push_back(index);
      current = frames_[index].parent;
    }
    return ancestors;
  };

  auto from_path = path(from);
  auto to_path = path(to);
  // get the lowest common ancestor and cancel the shared path before summing.
  while (!from_path.empty() && !to_path.empty() && from_path.back() == to_path.back()) {
    from_path.pop_back();
    to_path.pop_back();
  }
  // position and velocity of the two origins in common space
  StateVectors result{};
  for (const auto index : from_path) {
    accumulate(result, ephemerides_[index]->state_at(ut), 1);
  }
  for (const auto index : to_path) {
    accumulate(result, ephemerides_[index]->state_at(ut), -1);
  }
  if (!is_finite(result.position) || !is_finite(result.velocity)) {
    throw std::domain_error("Orbital frame offset exceeds the representable domain");
  }
  return result;
}

} // namespace flop::orbit
