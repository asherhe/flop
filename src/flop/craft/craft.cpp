#include "flop/craft/craft.hpp"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <stdexcept>
#include <utility>

#include "flop/math/utils.hpp"

namespace flop::craft {
Craft::Craft(
    CraftConfig config,
    const world::UniverseContext &context,
    std::vector<PartTypeConfig> part_types
) : context_(&context), name_(std::move(config.name)) {
  const auto &frames = context.reference_frames();
  const auto &orbit_config = config.orbit;
  if (name_.empty() || orbit_config.parent_name.empty() ||
      !std::isfinite(orbit_config.specific_angular_momentum_m2_per_s) ||
      orbit_config.specific_angular_momentum_m2_per_s == 0 ||
      !std::isfinite(orbit_config.eccentricity) || orbit_config.eccentricity < 0 ||
      !std::isfinite(orbit_config.longitude_periapsis_radians) ||
      !std::isfinite(orbit_config.epoch_mean_anomaly_radians) ||
      !std::isfinite(orbit_config.epoch_sec)) {
    throw std::invalid_argument("Invalid craft configuration");
  }
  const auto frame_id = frames.frame_id(orbit_config.parent_name);
  const auto mu = frames.frame(frame_id).gravitational_parameter;
  orbit_state_ = orbit::state_at_epoch_from_elements(
      mu,
      orbit_config.epoch_sec,
      {
          orbit_config.specific_angular_momentum_m2_per_s,
          orbit_config.eccentricity,
          orbit_config.longitude_periapsis_radians,
          orbit_config.epoch_mean_anomaly_radians,
      },
      frame_id
  );
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
    if (!is_finite(placement.position_meters) || !std::isfinite(placement.angle_radians))
      throw std::invalid_argument("Invalid part placement");
    if (!config.plugins_resolved && !type->plugins.empty())
      throw std::invalid_argument("Craft plugins must be resolved by the config loader");
    auto plugins = config.plugins_resolved ? placement.plugins : type->plugins;
    parts_.push_back(
        {placement.type_id,
         placement.id,
         placement.name,
         placement.position_meters,
         placement.angle_radians,
         type->mass_kg,
         type->crossfeed,
         type->attachment_nodes,
         placement.attachments,
         std::move(plugins)}
    );
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
}

const Part &Craft::part(std::string_view id) const {
  const auto found = std::ranges::find(parts_, id, &Part::id);
  if (found == parts_.end())
    throw std::invalid_argument("Unknown craft part: " + std::string(id));
  return *found;
}

void Craft::predict_patches(const orbit::RefFrameManager &frames, double begin_ut, double end_ut) {
  auto prediction = orbit::build_patches(frames, orbit_state_, begin_ut, end_ut);
  if (!prediction.endpoint_resolved || prediction.searched_through < end_ut) {
    throw std::runtime_error("Craft patch prediction did not cover the requested interval");
  }
  patches_ = std::move(prediction);
}

} // namespace flop::craft
