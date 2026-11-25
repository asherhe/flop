#pragma once

#include <map>
#include <string>
#include <variant>

#include "flop/math/vec2.hpp"

namespace flop::craft::plugin {

struct CommandConfig {
  bool auto_steerable{};
  double kp{3.54}, ki{0.12}, kd{3.12};
};

struct ReactionWheelConfig {
  double torque_newton_meters{};
};

struct EngineConfig {
  double isp_sec{};
  double thrust_newtons{};
  Vec2d thrust_direction{};
  std::map<std::string, double> propellant_ratio;
};

struct ResourceConfig {
  std::map<std::string, double> max_amount_units;
  std::map<std::string, double> amount_units;
  std::map<std::string, int> priority;
};

struct MaterialPropertyConfig {
  std::string type;
  std::string value;
};

struct MaterialConfig {
  std::string path;
  std::map<std::string, MaterialPropertyConfig> properties;
};

struct SpriteRendererConfig {
  std::string sprite;
  MaterialConfig material;
  Vec2d size_meters{};
  // normalized sprite pivot, from lower-left corner
  Vec2d pivot{0.5, 0.5};
};

struct LightConfig {
  Vec2d light_position_meters{};
  std::string color;
  double intensity{};
  double inner_radius_meters{};
  double outer_radius_meters{};
  double falloff{};
};

using PluginValues = std::variant<
    CommandConfig,
    ReactionWheelConfig,
    EngineConfig,
    ResourceConfig,
    SpriteRendererConfig,
    LightConfig>;

struct PartPluginConfig {
  std::string name;
  bool enabled{true};
  PluginValues values;
  // stable instance id, unique within the owning part. empty means "default to
  // the plugin name", which preserves the current one-instance-per-type assets.
  // placed last so existing positional {name, enabled, values} initializers and
  // decoders stay valid.
  std::string id;
};

} // namespace flop::craft::plugin
