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
  pixels_per_meter = std::clamp(pixels_per_meter * std::exp(input.zoom * 0.15), 1e-12, 1e12);
}

FrameDrawData CameraState::build_frame(
    const world::Universe &universe, const std::vector<config::BodyVisualConfig> &visuals
) const {
  const CameraState &camera = *this;
  FrameDrawData result;
  result.ut_sec = universe.ut_sec();
  result.camera = camera;
  const auto sun = std::ranges::find_if(universe.bodies(), [](const auto &body) {
    return body.name() == "Sun";
  });
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
    const Vec2d craft_position = craft.state_at(result.ut_sec).position;
    const Vec2d center = relative(craft.frame_id(), craft_position);
    Vec2d to_sun{};
    if (sun != universe.bodies().end()) {
      // subtract positions in double before the lighting calculation narrows to float
      to_sun = universe.reference_frames()
                   .offset(universe.frame_id("Sun"), craft.frame_id(), result.ut_sec)
                   .position -
               craft_position;
    }
    auto sunlight = compute_sunlight(to_sun);
    PlanetShine shine;
    std::array<float, 3> shine_rgb{};
    const auto &parent = universe.reference_frames().frame(craft.frame_id());
    if (sun != universe.bodies().end() && parent.parent) {
      const float visibility = sun_visibility(to_sun, craft_position,
          static_cast<float>(sun->config().radius_meters),
          static_cast<float>(universe.body(parent.name).config().radius_meters));
      for (std::size_t i = 0; i < 3; ++i) sunlight.color[i] *= visibility;
    }
    if (sun != universe.bodies().end() && parent.parent) {
      const auto visual = std::ranges::find_if(visuals, [&](const auto &v) { return v.name == parent.name; });
      if (visual != visuals.end() && visual->planet_shine) {
        const auto &body = universe.body(parent.name);
        const auto body_to_sun = universe.reference_frames().offset(
            universe.frame_id("Sun"), craft.frame_id(), result.ut_sec).position;
        shine = compute_planet_shine({static_cast<float>(body_to_sun.x), static_cast<float>(body_to_sun.y)},
            {static_cast<float>(craft_position.x), static_cast<float>(craft_position.y)},
            static_cast<float>(body.config().radius_meters), static_cast<float>(visual->planet_shine->intensity));
        const auto color = visual->planet_shine->color;
        shine_rgb = {color.r, color.g, color.b};
      }
    }
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
          const auto &visual = sprite->config();
          const auto size = visual.size_meters;
          const auto pivot = visual.pivot;
          const double sprite_angle = angle + part.angle_rad;
          result.sprites.back().sun = to_sprite_local(sunlight, sprite_angle);
          const float a = static_cast<float>(sprite_angle);
          const float c = std::cos(a), s = std::sin(a);
          auto &sprite_shine = result.sprites.back().planet_shine;
          sprite_shine.direction_and_spread = {c * shine.direction[0] + s * shine.direction[1],
              -s * shine.direction[0] + c * shine.direction[1], 0, shine.spread};
          sprite_shine.color = {shine_rgb[0] * shine.intensity, shine_rgb[1] * shine.intensity,
              shine_rgb[2] * shine.intensity, 0};
          const std::array<Vec2d, 4> corners{{
              {-pivot.x * size.x, -pivot.y * size.y},
              {(1.0 - pivot.x) * size.x, -pivot.y * size.y},
              {(1.0 - pivot.x) * size.x, (1.0 - pivot.y) * size.y},
              {-pivot.x * size.x, (1.0 - pivot.y) * size.y},
          }};
          for (std::size_t i = 0; i < corners.size(); ++i)
            result.sprites.back().corners_pixels[i] =
                project(pos + corners[i].rotate(sprite_angle), camera);
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
