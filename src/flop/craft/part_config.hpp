#pragma once

#include <string>
#include <vector>

#include "flop/craft/part_plugin_config.hpp"
#include "flop/math/vec2.hpp"

namespace flop::craft {

struct AttachmentNodeConfig {
  std::string name;
  Vec2d position_meters;
};

struct PartTypeConfig {
  std::string type_id;
  std::string name;
  double mass_kg;
  bool crossfeed;
  std::vector<AttachmentNodeConfig> attachment_nodes;
  std::vector<PartPluginConfig> plugins;
};

struct PartAttachmentConfig {
  std::string node_name;
  std::string attach_part_id;
  std::string attach_node_name;
};

// one installed part. the authored Unity transform is flattened at decode.
struct PartConfig {
  std::string type_id;
  std::string id;
  std::string name;
  Vec2d position_meters;
  double angle_radians;
  std::vector<PartAttachmentConfig> attachments;
  std::vector<PartPluginConfig> plugins;
};

} // namespace flop::craft
