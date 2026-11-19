#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

#include "flop/config/decode/part_plugin.hpp"
#include "flop/config/load.hpp"
#include "flop/craft/parts/config.hpp"

namespace flop::config {

namespace detail {

inline Vec2d decode_part_position(const ConfigView &value) {
  return {value.required("x").as<double>(), value.required("y").as<double>()};
}

} // namespace detail

template <> struct ConfigDecoder<craft::PartConfig> {
  [[nodiscard]] static craft::PartConfig decode(const ConfigView &document) {
    const auto pose = document.required("transform");
    craft::PartConfig result{
        document.required("type").nonempty_string(),
        document.required("id").nonempty_string(),
        document.required("name").nonempty_string(),
        detail::decode_part_position(pose.required("pos")),
        pose.required("rot").as<double>(),
        {},
        {},
    };
    for (const auto &attachment : document.required("attachments").sequence()) {
      const auto mode = attachment.required("mode");
      if (mode.as<std::string>() != "Node")
        mode.fail(mode.mark(), "Unsupported attachment mode");
      result.attachments.push_back({
          attachment.required("node").nonempty_string(),
          attachment.required("attach_part").nonempty_string(),
          attachment.required("attach_node").nonempty_string(),
      });
    }
    static_cast<void>(document.required("plugins").entries());
    return result;
  }
};

template <> struct ConfigDecoder<craft::PartTypeConfig> {
  [[nodiscard]] static craft::PartTypeConfig decode(const ConfigView &document) {
    const auto mass = document.required("mass");
    craft::PartTypeConfig result{
        {},
        document.required("name").nonempty_string(),
        mass.checked_finite(mass.positive_double() * 1000),
        document.required("crossfeed").as<bool>(),
        {},
        {},
    };
    for (const auto &node : document.required("attachment_nodes").sequence()) {
      result.attachment_nodes.push_back({
          node.required("name").nonempty_string(),
          detail::decode_part_position(node.required("pos")),
      });
    }
    for (const auto &entry : document.required("plugins").entries())
      result.plugins.push_back(decode_part_plugin(entry.key, entry.value));
    return result;
  }
};

// the authored part type is its asset folder name, not a duplicated YAML field.
[[nodiscard]] inline craft::PartTypeConfig load_part_type(
    const AssetResolver &resolver, std::string_view type_id
) {
  const std::string id(type_id);
  if (id.empty() || id.find('/') != std::string::npos || id.find('\\') != std::string::npos)
    throw std::invalid_argument("Invalid part type ID");
  auto result = load_config<craft::PartTypeConfig>(
      resolver, AssetPath("assets/parts/" + id + "/" + id + ".yaml")
  );
  result.type_id = id;
  return result;
}

} // namespace flop::config
