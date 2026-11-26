#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "flop/config/body_visual_config.hpp"
#include "flop/math/vec2.hpp"
#include "flop/platform/frame_input.hpp"
#include "flop/view/lighting.hpp"
#include "flop/world/universe.hpp"

namespace flop::view {

// coordinates are metres relative to the focus; zoom is drawable pixels per metre.
using Focus = std::variant<std::string, world::CraftId>;
struct FrameDrawData;
struct CameraState {
  Focus focus{std::string{}};
  Vec2d pan_meters{};
  double pixels_per_meter{50.0};
  int viewport_width{1280};
  int viewport_height{720};

  // Pan input uses logical pixels; the viewport uses drawable pixels.
  void update(
      const platform::CameraInput &input,
      int logical_width,
      int logical_height,
      int pixel_width,
      int pixel_height
  );

  // Body visuals are stored separately from the simulation in GameScene.
  [[nodiscard]] FrameDrawData build_frame(
      const world::Universe &universe, const std::vector<config::BodyVisualConfig> &visuals
  ) const;
};
enum class ViewMode : std::uint8_t { flight, map, minimap };
struct ViewState {
  CameraState flight;
  CameraState map;
  CameraState minimap;
  ViewMode active{ViewMode::flight};
  std::optional<Focus> map_target;
};

struct PointF {
  float x{}, y{};
};
struct SpriteDraw {
  world::CraftId craft;
  craft::PartId part;
  craft::PluginId plugin;
  Vec2d camera_relative_meters;
  PointF center_pixels;
  double angle_rad{};
  craft::plugin::SpriteConfig visual;
  // lower-left, lower-right, upper-right, upper-left in sprite-local coordinates
  std::array<PointF, 4> corners_pixels{};
  SpriteSunlight sun;
};
struct LightDraw {
  world::CraftId craft;
  craft::PartId part;
  craft::PluginId plugin;
  Vec2d camera_relative_meters;
  PointF center_pixels;
  craft::plugin::LightConfig visual;
};
struct PlanetDraw {
  std::string name;
  Vec2d camera_relative_meters;
  PointF center_pixels;
  double radius_meters{};
  float radius_pixels{};
  config::BodyVisualConfig visual;
};
struct TextDraw {
  std::string text;
  PointF position_pixels;
};
struct FrameDrawData {
  double ut_sec{};
  CameraState camera;
  std::vector<PlanetDraw> planets;
  std::vector<SpriteDraw> sprites;
  std::vector<LightDraw> lights;
  std::vector<TextDraw> text;
};

} // namespace flop::view
