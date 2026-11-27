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
class ParticleScene;
struct FrameBuildContext {
  const world::Universe &universe;
  const std::vector<config::BodyVisualConfig> &body_visuals;
  const ParticleScene *particles{};
};
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

  // collect all presentation data from the world and optional particle scene.
  [[nodiscard]] FrameDrawData build_frame(const FrameBuildContext &context) const;
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
  SpritePlanetShine planet_shine;
};
struct LightDraw {
  world::CraftId craft;
  craft::PartId part;
  craft::PluginId plugin;
  Vec2d camera_relative_meters;
  PointF center_pixels;
  float inner_radius_pixels{};
  float outer_radius_pixels{};
  float falloff{};
  // linear rgb includes authored intensity and the runtime strength
  std::array<float, 3> color{};
  // distance above the sprite plane in drawable pixels
  float z_offset_pixels{};
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
struct ParticleDraw {
  PointF center_pixels;
  float size_pixels{};
  std::array<float, 4> color{};
  float angle_rad{};
};
struct PostProcessSettings {
  bool tonemapping{true};
  float exposure_stops{0.0f}; // exposure adjustment in stops
  float white_point{11.2f}; // linear scene value mapped to display white
  float vignette_intensity{0.308f};
  float vignette_smoothness{0.454f};
};
struct FrameDrawData {
  double ut_sec{};
  CameraState camera;
  PostProcessSettings post_process;
  std::vector<PlanetDraw> planets;
  std::vector<SpriteDraw> sprites;
  std::vector<LightDraw> lights;
  std::vector<ParticleDraw> particles;
  std::vector<TextDraw> text;
};

} // namespace flop::view
