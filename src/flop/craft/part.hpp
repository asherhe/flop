#pragma once

#include <string>
#include <vector>

#include "flop/craft/part_config.hpp"

namespace flop::craft {

// installed geometry and dry properties; plugins are added in a later step.
struct Part {
  std::string type_id;
  std::string id;
  std::string name;
  Vec2d position_meters;
  double angle_radians;
  double dry_mass_kg;
  bool crossfeed;
  std::vector<AttachmentNodeConfig> attachment_nodes;
  std::vector<PartAttachmentConfig> attachments;
  std::vector<PartPluginConfig> plugins;
};

} // namespace flop::craft
