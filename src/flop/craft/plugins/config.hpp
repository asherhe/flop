#pragma once

#include <map>
#include <string>
#include <variant>
#include <array>

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

struct AssetSource {
  std::string address;
  std::string key;
  int line{-1};
  int column{-1};
};

struct SpriteMaterialConfig {
  std::string shader;
  // retained for a future lit shader, not yet sampled by craft/sprite
  std::string normal_map;
  std::string emission_map;
  AssetSource normal_map_source;
  AssetSource emission_map_source;
};

struct SpriteConfig {
  std::string sprite;
  SpriteMaterialConfig material;
  Vec2d size_meters{};
  // normalized sprite pivot, from lower-left corner
  Vec2d pivot{0.5, 0.5};
  AssetSource sprite_source;
};

struct LightConfig {
  Vec2d light_position_meters{};
  std::string color;
  double intensity{};
  double inner_radius_meters{};
  double outer_radius_meters{};
  double falloff{};
};

struct ParticleConfig {
  Vec2d nozzle_position_meters{};
  double nozzle_size_meters{};
  double spread_angle_rad{};
  std::array<double, 2> lifetime_sec{};
  std::array<double, 2> rate_per_sec{};
  std::array<double, 2> start_size_meters{};
  std::array<double, 2> start_speed_m_per_s{};
  std::map<double, std::string> color_keys;
  std::map<double, double> alpha_keys;
  std::map<double, double> size_keys;
};

using PluginValues = std::variant<
    CommandConfig,
    ReactionWheelConfig,
    EngineConfig,
    ResourceConfig,
    SpriteConfig,
    LightConfig,
    ParticleConfig>;

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
