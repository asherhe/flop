#include "flop/world/universe.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include "flop/craft/plugins/behavior_plugins.hpp"
#include "flop/math/utils.hpp"

namespace flop::world {
namespace {

// the enabled command plugin on a craft, or nullptr if none is installed. a
// craft has at most one command pod in the current model; the first one found
// is authoritative.
craft::plugin::CommandPlugin *find_command_plugin(craft::Craft &craft) {
  for (const auto &part : craft.parts()) {
    // parts() is a const span; reach the mutable part through the craft by id.
    auto &mutable_part = craft.part(part.id);
    for (const auto &plugin : mutable_part.installed_plugins()) {
      if (auto *command = dynamic_cast<craft::plugin::CommandPlugin *>(plugin.get())) {
        return command;
      }
    }
  }
  return nullptr;
}

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
    std::vector<craft::ResourceTypeConfig> resource_configs,
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

CraftId Universe::add_craft(
    craft::CraftConfig config, std::vector<craft::PartTypeConfig> part_types
) {
  const auto id = static_cast<CraftId>(next_craft_id_);
  // build before recording so a construction failure leaves the world and the
  // id counter unchanged.
  auto craft = std::make_unique<craft::Craft>(std::move(config), *this, std::move(part_types));
  craft_.emplace(id, std::move(craft));
  craft_order_.push_back(id);
  ++next_craft_id_;
  return id;
}

std::vector<CraftId> Universe::craft_ids() const {
  return craft_order_;
}

const craft::Craft &Universe::craft(CraftId id) const {
  const auto found = craft_.find(id);
  if (found == craft_.end()) {
    throw std::out_of_range("Unknown craft id: " + std::to_string(static_cast<std::uint64_t>(id)));
  }
  return *found->second;
}

const craft::Craft *Universe::find_craft(CraftId id) const noexcept {
  const auto found = craft_.find(id);
  return found == craft_.end() ? nullptr : found->second.get();
}

bool Universe::any_craft_powered() const noexcept {
  return std::ranges::any_of(craft_order_, [this](const CraftId id) {
    return craft_.at(id)->control().throttle() > 0.0;
  });
}

craft::Craft *Universe::mutable_craft(CraftId id) noexcept {
  const auto found = craft_.find(id);
  return found == craft_.end() ? nullptr : found->second.get();
}

void Universe::neutralize_active_pilot_input() {
  craft::Craft *active = mutable_craft(controls_.active_craft_);
  if (active == nullptr) {
    return;
  }
  if (auto *command = find_command_plugin(*active)) {
    command->set_throttle_input(0.0);
    command->set_steering_input(0.0);
  }
}

std::vector<ControlManager::Result> Universe::apply_commands() {
  std::vector<ControlManager::Result> results;
  results.reserve(controls_.pending_.size());
  // take the queue so commands submitted during execution (none today) wait for
  // the next drain, and so the manager is left empty afterward.
  std::vector<ControlManager::Command> commands;
  commands.swap(controls_.pending_);

  for (auto &command : commands) {
    const ControlManager::Status status = std::visit(
        [&](auto &value) -> ControlManager::Status {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, ControlManager::SelectCraft>) {
            if (mutable_craft(value.craft) == nullptr) {
              return ControlManager::Status::missing_target;
            }
            if (value.craft != controls_.active_craft_) {
              // leaving a craft neutralizes its held input so it coasts on its
              // persistent throttle; the newly selected craft starts from the
              // current (neutral) pilot sample.
              neutralize_active_pilot_input();
              controls_.active_craft_ = value.craft;
            }
            return ControlManager::Status::applied;
          } else if constexpr (std::is_same_v<T, ControlManager::SetThrottle>) {
            craft::Craft *active = mutable_craft(controls_.active_craft_);
            if (active == nullptr) {
              return ControlManager::Status::unsupported;
            }
            active->control().set_throttle(std::clamp(value.throttle, 0.0, 1.0));
            return ControlManager::Status::applied;
          } else if constexpr (std::is_same_v<T, ControlManager::SetPluginEnabled>) {
            craft::Craft *target = mutable_craft(value.target.craft);
            if (target == nullptr) {
              return ControlManager::Status::missing_target;
            }
            craft::Part *part = target->find_part(value.target.part);
            if (part == nullptr) {
              return ControlManager::Status::missing_target;
            }
            craft::plugin::PartPlugin *plugin = part->find_plugin(value.target.plugin);
            if (plugin == nullptr) {
              return ControlManager::Status::missing_target;
            }
            plugin->set_enabled(value.enabled);
            return ControlManager::Status::applied;
          }
          return ControlManager::Status::unsupported;
        },
        command
    );
    results.push_back({std::move(command), status});
  }
  return results;
}

void Universe::prepare_active_command_plugin() {
  craft::Craft *active = mutable_craft(controls_.active_craft_);
  if (active == nullptr) {
    return;
  }
  if (auto *command = find_command_plugin(*active)) {
    const PilotSample sample = controls_.pilot_sample_;
    command->set_throttle_input(sample.throttle_rate_per_sec);
    command->set_steering_input(sample.steering);
  }
}

void Universe::tick(TickDurations durations) {
  if (!std::isfinite(durations.control_dt_sec) || durations.control_dt_sec < 0 ||
      !std::isfinite(durations.simulation_dt_sec) || durations.simulation_dt_sec <= 0) {
    throw std::invalid_argument(
        "Universe tick durations must be finite, with a positive simulation step"
    );
  }
  const double next_ut_sec = ut_sec_ + durations.simulation_dt_sec;
  if (!std::isfinite(next_ut_sec) || next_ut_sec <= ut_sec_) {
    throw std::invalid_argument("Universe tick must advance to a finite UT");
  }

  // feed the held pilot input into the active craft just before stepping so the
  // step integrates it; an inactive craft receives no held input.
  prepare_active_command_plugin();

  // step every craft from the same pre-tick UT so stepping order does not change
  // the world time each craft reads.
  const double now_ut = ut_sec_;
  for (const auto id : craft_order_) {
    craft_.at(id)->step(now_ut, durations);
  }

  // TODO(soi): apply any SOI crossings whose crossing UT falls within this step,
  // chronologically across all craft, reseeding the crossing craft into the
  // destination frame before publishing UT. Deferred to a focused SOI unit; no
  // shipped body has a finite SOI child yet, so no crossing is due here.

  ut_sec_ = next_ut_sec;
}

void Universe::tick(double dt_sec) {
  if (!std::isfinite(dt_sec) || dt_sec <= 0) {
    throw std::invalid_argument("Universe tick must be finite and positive");
  }
  // same-duration adapter: control and simulation advance by the same dt.
  tick(TickDurations{dt_sec, dt_sec});
}

double warp_ceiling(const Universe &universe) noexcept {
  return universe.any_craft_powered() ? POWERED_WARP_CEILING
                                      : std::numeric_limits<double>::infinity();
}

} // namespace flop::world
