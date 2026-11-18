#include "flop/world/universe.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

#include "flop/math/utils.hpp"

namespace flop::world {
namespace {

double valid_ut(double value) {
  if (!std::isfinite(value)) {
    throw std::invalid_argument("Universe UT must be finite");
  }
  return value;
}

double valid_gravitational_constant(double value) {
  if (!positive_finite(value)) {
    throw std::invalid_argument("Universe gravitational constant must be finite and positive");
  }
  return value;
}

std::vector<CelestialBody> make_bodies(
    std::vector<BodyConfig> configs, double gravitational_constant, const UniverseContext &context
) {
  std::vector<CelestialBody> bodies;
  bodies.reserve(configs.size());
  for (auto &config : configs) {
    if (config.name.empty()) {
      throw std::invalid_argument("Celestial body name cannot be empty");
    }
    if (std::ranges::find_if(bodies, [&](const CelestialBody &body) {
          return body.name() == config.name;
        }) != bodies.end()) {
      throw std::invalid_argument("Duplicate celestial body name: " + config.name);
    }
    bodies.emplace_back(config, gravitational_constant, context);
  }
  return bodies;
}

orbit::RefFrameManager make_frames(const std::vector<CelestialBody> &bodies) {
  if (std::ranges::count_if(bodies, [](const CelestialBody &body) {
        return !body.config().orbit;
      }) != 1) {
    throw std::invalid_argument("Universe requires exactly one root celestial body");
  }

  std::vector<orbit::RefFrameDefinition> definitions;
  definitions.reserve(bodies.size());
  for (const auto &body : bodies) {
    if (!body.config().orbit) {
      definitions.push_back(body.ref_frame_definition());
      continue;
    }
    const auto &parent_name = body.config().orbit->parent_name;
    const auto parent = std::ranges::find_if(bodies, [&](const CelestialBody &candidate) {
      return candidate.name() == parent_name;
    });
    if (parent == bodies.end()) {
      throw std::invalid_argument(
          "Unknown parent celestial body '" + parent_name + "' for '" + body.name() + "'"
      );
    }
    definitions.push_back(body.ref_frame_definition(parent->gravitational_parameter()));
  }
  return orbit::RefFrameManager(std::move(definitions));
}

orbit::FrameId root_frame_id(const orbit::RefFrameManager &frames) {
  const auto all = frames.frames();
  const auto root =
      std::ranges::find_if(all, [](const orbit::RefFrame &frame) { return !frame.parent; });
  return root->id;
}

} // namespace

Universe::Universe(
    std::vector<BodyConfig> configs, double initial_ut_sec, double gravitational_constant
) : Universe(std::move(configs), {}, initial_ut_sec, gravitational_constant) {}

Universe::Universe(
    std::vector<BodyConfig> configs,
    std::vector<craft::ResourceConfig> resource_configs,
    double initial_ut_sec,
    double gravitational_constant
) :
    ut_sec_(valid_ut(initial_ut_sec)),
    gravitational_constant_(valid_gravitational_constant(gravitational_constant)),
    bodies_(make_bodies(std::move(configs), gravitational_constant_, *this)),
    frames_(make_frames(bodies_)), root_frame_id_(root_frame_id(frames_)),
    resources_(std::move(resource_configs)) {}

const CelestialBody &Universe::body(std::string_view name) const {
  const auto found = std::ranges::find_if(bodies_, [name](const CelestialBody &candidate) {
    return candidate.name() == name;
  });
  if (found == bodies_.end()) {
    throw std::invalid_argument("Unknown celestial body: " + std::string(name));
  }
  return *found;
}

orbit::FrameId Universe::frame_id(std::string_view name) const {
  return frames_.frame_id(body(name).name());
}

orbit::StateVectors Universe::body_state(std::string_view name, double ut_sec) const {
  return frames_.offset(frame_id(name), root_frame_id_, ut_sec);
}

void Universe::tick(double dt_sec) {
  if (!std::isfinite(dt_sec) || dt_sec <= 0) {
    throw std::invalid_argument("Universe tick must be finite and positive");
  }
  const double next_ut_sec = ut_sec_ + dt_sec;
  if (!std::isfinite(next_ut_sec) || next_ut_sec <= ut_sec_) {
    throw std::invalid_argument("Universe tick must advance to a finite UT");
  }
  ut_sec_ = next_ut_sec;
}

} // namespace flop::world
