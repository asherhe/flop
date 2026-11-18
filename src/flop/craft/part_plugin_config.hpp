#pragma once

#include <map>
#include <string>
#include <variant>

#include "flop/math/vec2.hpp"

namespace flop::craft {

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

struct ResourceContainerConfig {
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
    ResourceContainerConfig,
    SpriteRendererConfig,
    LightConfig>;

struct PartPluginConfig {
  std::string name;
  bool enabled{true};
  PluginValues values;
};

} // namespace flop::craft
