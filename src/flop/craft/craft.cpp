#include "flop/craft/craft.hpp"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <stdexcept>
#include <utility>

#include "flop/craft/plugins/part_plugin.hpp"
#include "flop/math/utils.hpp"
#include "flop/orbit/propagators/universal.hpp"

namespace flop::craft {
Craft::Craft(
    CraftConfig config,
    const world::UniverseContext &context,
    std::vector<PartTypeConfig> part_types
) : context_(&context), name_(std::move(config.name)) {
  const auto &frames = context.reference_frames();
  if (!config.orbit.has_value()) {
    throw std::invalid_argument(
        "Craft '" + name_ + "' has no orbit; a template cannot be placed without a save instance"
    );
  }
  const auto &orbit_config = *config.orbit;
  if (name_.empty() || orbit_config.parent_name.empty() ||
      !is_finite(orbit_config.state.position) || !is_finite(orbit_config.state.velocity) ||
      orbit_config.state.position == Vec2d{} || !std::isfinite(orbit_config.epoch_sec)) {
    throw std::invalid_argument("Invalid craft configuration");
  }
  const auto frame_id = frames.frame_id(orbit_config.parent_name);
  // seed the orbit directly from the saved cartesian state, the same shape
  // reseed_orbit installs after a burn. no element conversion is involved.
  orbit_state_ = {orbit_config.epoch_sec, orbit_config.state, frame_id};
  patches_ = {
      {{orbit_state_, orbit_config.epoch_sec, orbit_config.epoch_sec}}, orbit_config.epoch_sec
  };

  if (config.parts.empty())
    throw std::invalid_argument("Craft requires at least one part");

  for (auto type = part_types.begin(); type != part_types.end(); ++type) {
    if (type->type_id.empty() || type->name.empty() || !positive_finite(type->mass_kg) ||
        std::ranges::find(part_types.begin(), type, type->type_id, &PartTypeConfig::type_id) !=
            type)
      throw std::invalid_argument("Invalid or duplicate part type");
    for (auto node = type->attachment_nodes.begin(); node != type->attachment_nodes.end(); ++node) {
      if (node->name.empty() || !is_finite(node->position_meters) ||
          std::ranges::find(
              type->attachment_nodes.begin(), node, node->name, &AttachmentNodeConfig::name
          ) != node)
        throw std::invalid_argument("Invalid or duplicate attachment node");
    }
  }

  for (const auto &placement : config.parts) {
    const auto type = std::ranges::find(part_types, placement.type_id, &PartTypeConfig::type_id);
    if (type == part_types.end())
      throw std::invalid_argument("Unknown part type: " + placement.type_id);
    if (std::ranges::find(parts_, placement.id, &Part::id) != parts_.end())
      throw std::invalid_argument("Duplicate craft part ID: " + placement.id);
    if (!is_finite(placement.position_meters) || !std::isfinite(placement.angle_rad))
      throw std::invalid_argument("Invalid part placement");
    if (!config.plugins_resolved && !type->plugins.empty())
      throw std::invalid_argument("Craft plugins must be resolved by the config loader");
    auto plugins = config.plugins_resolved ? placement.plugins : type->plugins;
    Part part;
    part.type_id = placement.type_id;
    part.id = placement.id;
    part.name = placement.name;
    part.position_meters = placement.position_meters;
    part.angle_rad = placement.angle_rad;
    part.dry_mass_kg = type->mass_kg;
    part.crossfeed = type->crossfeed;
    part.attachment_nodes = type->attachment_nodes;
    part.attachments = placement.attachments;
    part.plugins = std::move(plugins);
    parts_.push_back(std::move(part));
  }
  for (const auto &part : parts_) {
    std::vector<std::string_view> occupied_nodes;
    for (const auto &attachment : part.attachments) {
      if (std::ranges::find(occupied_nodes, attachment.node_name) != occupied_nodes.end())
        throw std::invalid_argument("Attachment node is occupied more than once");
      occupied_nodes.push_back(attachment.node_name);
      const auto other = std::ranges::find(parts_, attachment.attach_part_id, &Part::id);
      if (other == parts_.end() || other->id == part.id)
        throw std::invalid_argument("Unknown or self-referencing part attachment");
      if (std::ranges::find(
              part.attachment_nodes, attachment.node_name, &AttachmentNodeConfig::name
          ) == part.attachment_nodes.end() ||
          std::ranges::find(
              other->attachment_nodes, attachment.attach_node_name, &AttachmentNodeConfig::name
          ) == other->attachment_nodes.end())
        throw std::invalid_argument("Unknown attachment node");
      if (std::ranges::find_if(other->attachments, [&](const PartAttachmentConfig &reverse) {
            return reverse.attach_part_id == part.id &&
                   reverse.node_name == attachment.attach_node_name &&
                   reverse.attach_node_name == attachment.node_name;
          }) == other->attachments.end())
        throw std::invalid_argument("Part attachment is not reciprocal");
    }
  }

  // built once parts_ is fully validated: each part builds its own runtime
  // plugin objects from its authored plugin configuration, bound to its index.
  for (std::size_t index = 0; index < parts_.size(); ++index) {
    parts_[index].install_plugins(context.resources(), index);
  }

  // resource state reads the installed container plugins and the shared
  // resource registry.
  resources_ = CraftResources(parts_, context);
  newtonian_ = CraftNewtonian(config.initial_attitude);
}

const Part &Craft::part(std::string_view id) const {
  const auto found = std::ranges::find(parts_, id, &Part::id);
  if (found == parts_.end())
    throw std::invalid_argument("Unknown craft part: " + std::string(id));
  return *found;
}

Part &Craft::part(std::string_view id) {
  const auto found = std::ranges::find(parts_, id, &Part::id);
  if (found == parts_.end())
    throw std::invalid_argument("Unknown craft part: " + std::string(id));
  return *found;
}

const Part *Craft::find_part(const PartId &part_id) const {
  const auto found = std::ranges::find(parts_, part_id.value(), &Part::id);
  return found == parts_.end() ? nullptr : &*found;
}

Part *Craft::find_part(const PartId &part_id) {
  const auto found = std::ranges::find(parts_, part_id.value(), &Part::id);
  return found == parts_.end() ? nullptr : &*found;
}

const orbit::UniversalPropagator &Craft::propagator() {
  if (!propagator_.has_value()) {
    const double mu =
        context_->reference_frames().frame(orbit_state_.frame_id).gravitational_parameter;
    propagator_.emplace(mu, orbit_state_);
  }
  return *propagator_;
}

orbit::StateVectors Craft::state_at(double ut) const {
  // reuse the step cache when it is already built from the current seed; a
  // displayed frame then costs one anomaly solve and no allocation. otherwise
  // build a throwaway propagator so the query stays const and does not disturb
  // the next step's cache.
  if (propagator_.has_value()) {
    return propagator_->state_at(ut);
  }
  const double mu =
      context_->reference_frames().frame(orbit_state_.frame_id).gravitational_parameter;
  return orbit::UniversalPropagator(mu, orbit_state_).state_at(ut);
}

void Craft::reseed_orbit(double now_ut, const orbit::StateVectors &state) {
  orbit_state_ = {now_ut, state, orbit_state_.frame_id};
  // the cached propagator was built from the previous seed; drop it so the next
  // propagate rebuilds from the new one.
  propagator_.reset();
}

void Craft::step(double now_ut, double dt_sec) {
  orbit::StateVectors state = propagator().state_at(now_ut);

  for (auto &part : parts_) {
    for (const auto &plugin : part.installed_plugins())
      plugin->reset_step();
  }

  // control-phase plugins (the command pod) write steering before actuation-phase
  // plugins read it, regardless of the order parts were installed in.
  for (auto &part : parts_) {
    part.step_phase(*this, dt_sec, plugin::TickPhase::control);
  }
  for (auto &part : parts_) {
    part.step_phase(*this, dt_sec, plugin::TickPhase::actuation);
  }

  const bool vel_changed = newtonian_.step(*this, state, dt_sec);
  // a burn changed the linear velocity, so install a new seed at now_ut.
  // coasting keeps the analytical seed exact and reuses the cached propagator.
  if (vel_changed) {
    reseed_orbit(now_ut, state);
  }
}

// replaces patches_ only; independent of the step propagator cache, which tracks
// orbit_state_. this does not reseed the orbit.
void Craft::predict_patches(const orbit::RefFrameManager &frames, double begin_ut, double end_ut) {
  auto prediction = orbit::build_patches(frames, orbit_state_, begin_ut, end_ut);
  if (!prediction.endpoint_resolved || prediction.searched_through < end_ut) {
    throw std::runtime_error("Craft patch prediction did not cover the requested interval");
  }
  patches_ = std::move(prediction);
}

} // namespace flop::craft
