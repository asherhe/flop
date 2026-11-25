#include "flop/view/frame.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <type_traits>

#include "flop/craft/plugins/plugins.hpp"

namespace flop::view {
namespace {
// A focus can be a craft or a body. This is its position expressed in an
// orbital frame, not its identity; craft need not be reference frames.
struct FramePosition {
  orbit::FrameId frame;
  Vec2d position;
};
FramePosition focus_position(const world::Universe &world, const Focus &focus, double ut) {
  return std::visit(
      [&](const auto &id) -> FramePosition {
        using T = std::decay_t<decltype(id)>;
        if constexpr (std::is_same_v<T, std::string>) {
          return {world.frame_id(id), {}};
        } else {
          const auto &craft = world.craft(id);
          return {craft.frame_id(), craft.state_at(ut).position};
        }
      },
      focus
  );
}
PointF project(Vec2d relative, const CameraState &camera) {
  return {
      static_cast<float>(camera.viewport_width * 0.5 + relative.x * camera.pixels_per_meter),
      static_cast<float>(camera.viewport_height * 0.5 - relative.y * camera.pixels_per_meter),
  };
}
} // namespace

void CameraState::update(
    const platform::CameraInput &input,
    int logical_width,
    int logical_height,
    int pixel_width,
    int pixel_height
) {
  viewport_width = std::max(0, pixel_width);
  viewport_height = std::max(0, pixel_height);
  const double sx = logical_width > 0 ? static_cast<double>(pixel_width) / logical_width : 1.0;
  const double sy = logical_height > 0 ? static_cast<double>(pixel_height) / logical_height : 1.0;
  pan_meters.x -= input.pan_x * sx / pixels_per_meter;
  pan_meters.y += input.pan_y * sy / pixels_per_meter;
  pixels_per_meter =
      std::clamp(pixels_per_meter * std::exp(input.zoom * 0.15), 1e-12, 1e12);
}

FrameDrawData CameraState::build_frame(
    const world::Universe &universe,
    const std::vector<config::BodyVisualConfig> &visuals
) const {
  const CameraState &camera = *this;
  FrameDrawData result;
  result.ut_sec = universe.ut_sec();
  result.camera = camera;
  const FramePosition focus = focus_position(universe, camera.focus, result.ut_sec);
  const auto relative = [&](orbit::FrameId frame, Vec2d position) {
    const Vec2d offset =
        universe.reference_frames().offset(frame, focus.frame, result.ut_sec).position;
    return (offset - focus.position) + position - camera.pan_meters;
  };
  for (const auto &body : universe.bodies()) {
    const auto visual =
        std::ranges::find_if(visuals, [&](const auto &v) { return v.name == body.name(); });
    if (visual == visuals.end())
      throw std::invalid_argument("missing visual for body " + body.name());
    const Vec2d pos = relative(universe.frame_id(body.name()), {});
    result.planets.push_back({
        body.name(),
        pos,
        project(pos, camera),
        body.config().radius_meters,
        static_cast<float>(body.config().radius_meters * camera.pixels_per_meter),
        *visual,
    });
  }
  for (const auto id : universe.craft_ids()) {
    const auto &craft = universe.craft(id);
    const Vec2d center = relative(craft.frame_id(), craft.state_at(result.ut_sec).position);
    const Vec2d com = craft.mass_properties().center_of_mass_meters;
    const double angle = craft.newtonian().angle_rad();
    for (const auto &part : craft.parts()) {
      for (const auto &plugin : part.installed_plugins()) {
        if (!plugin->enabled())
          continue;
        if (const auto *sprite = dynamic_cast<const craft::plugin::SpritePlugin *>(plugin.get())) {
          const Vec2d pos = center + (part.position_meters - com).rotate(angle);
          result.sprites.push_back({
              id,
              part.part_id(),
              plugin->id(),
              pos,
              project(pos, camera),
              angle + part.angle_rad,
              sprite->config(),
          });
        } else if (
            const auto *light = dynamic_cast<const craft::plugin::LightPlugin *>(plugin.get())
        ) {
          const Vec2d local =
              part.position_meters + light->config().light_position_meters.rotate(part.angle_rad);
          const Vec2d pos = center + (local - com).rotate(angle);
          result.lights.push_back(
              {id, part.part_id(), plugin->id(), pos, project(pos, camera), light->config()}
          );
        }
      }
    }
  }
  result.text.push_back({"UT: " + std::to_string(result.ut_sec), {10.0f, 10.0f}});
  return result;
}
} // namespace flop::view

