#include "flop/view/particle_scene.hpp"

#include <algorithm>
#include <type_traits>
#include <cmath>

#include "flop/craft/plugins/plugins.hpp"

namespace flop::view {
namespace {

orbit::FrameId root_frame(const world::Universe &universe) {
  const auto frames = universe.reference_frames().frames();
  return std::ranges::find_if(frames, [](const auto &frame) { return !frame.parent; })->id;
}

orbit::StateVectors root_state(const world::Universe &universe, orbit::FrameId frame,
                               orbit::StateVectors local, double ut) {
  const auto offset = universe.reference_frames().offset(frame, root_frame(universe), ut);
  return {offset.position + local.position, offset.velocity + local.velocity};
}

Vec2d focus_position(const world::Universe &universe, const Focus &focus) {
  const double ut = universe.ut_sec();
  return std::visit([&](const auto &id) -> Vec2d {
    using T = std::decay_t<decltype(id)>;
    if constexpr (std::is_same_v<T, std::string>)
      return root_state(universe, universe.frame_id(id), {}, ut).position;
    else {
      const auto &craft = universe.craft(id);
      return root_state(universe, craft.frame_id(), craft.state_at(ut), ut).position;
    }
  }, focus);
}

template <typename T, typename Blend>
T curve_value(const std::map<double, T> &keys, double t, Blend blend) {
  auto upper = keys.lower_bound(t);
  if (upper == keys.begin()) return upper->second;
  if (upper == keys.end()) return std::prev(upper)->second;
  const auto lower = std::prev(upper);
  return blend(lower->second, upper->second, (t - lower->first) / (upper->first - lower->first));
}

std::array<float, 3> color_value(const std::string &value) {
  if (value.size() != 7 || value[0] != '#') return {1, 1, 1};
  try {
    const auto rgb = std::stoul(value.substr(1), nullptr, 16);
    return {static_cast<float>((rgb >> 16) & 255) / 255.0f,
            static_cast<float>((rgb >> 8) & 255) / 255.0f,
            static_cast<float>(rgb & 255) / 255.0f};
  } catch (...) { return {1, 1, 1}; }
}

} // namespace

void ParticleScene::set_focus(const Focus &focus) {
  if (!focus_ || *focus_ != focus) {
    // a physical reference frame can preserve particles across focus changes later.
    particles_.clear();
    emitters_.clear();
    focus_ = focus;
  }
}

void ParticleScene::begin_step(const world::Universe &universe, const Focus &focus) {
  set_focus(focus);
  focus_before_ = focus_position(universe, focus);
}

void ParticleScene::end_step(const world::Universe &universe, const Focus &focus, double dt_sec) {
  set_focus(focus);
  const auto focus_after = focus_position(universe, focus);
  particles_.advance(dt_sec, focus_after - focus_before_);
  const double ut = universe.ut_sec();
  for (const auto id : universe.craft_ids()) {
    const auto &craft = universe.craft(id);
    const auto state = root_state(universe, craft.frame_id(), craft.state_at(ut), ut);
    const auto com = craft.mass_properties().center_of_mass_meters;
    const double angle = craft.newtonian().angle_rad();
    const double omega = craft.newtonian().angular_velocity_rad_per_s();
    for (const auto &part : craft.parts()) {
      for (const auto &plugin : part.installed_plugins()) {
        const auto *particle = dynamic_cast<const craft::plugin::ParticlePlugin *>(plugin.get());
        if (!particle || !particle->enabled() || !(particle->emission_strength() > 0.0)) continue;
        const world::PluginTarget target{id, part.part_id(), plugin->id()};
        auto entry = std::ranges::find(emitters_, target, &Entry::target);
        if (entry == emitters_.end()) {
          emitters_.push_back({target, {}});
          entry = std::prev(emitters_.end());
          entry->emitter.config = std::make_shared<craft::plugin::ParticleConfig>(particle->config());
        }
        const auto &config = particle->config();
        const auto arm = (part.position_meters + config.nozzle_position_meters.rotate(part.angle_rad) - com).rotate(angle);
        entry->emitter.position_meters = state.position - focus_after + arm;
        entry->emitter.velocity_m_per_s = state.velocity + arm.perpendicular() * omega;
        entry->emitter.angle_rad = angle + part.angle_rad;
        entry->emitter.strength = particle->emission_strength();
        particles_.emit(entry->emitter, dt_sec);
      }
    }
  }
}

void ParticleScene::append_draw_data(FrameDrawData &frame) const {
  for (const auto &particle : particles_.particles()) {
    const auto &config = *particle.config;
    if (config.color_keys.empty() || config.alpha_keys.empty() || config.size_keys.empty()) continue;
    const double t = std::clamp(particle.age_sec / particle.lifetime_sec, 0.0, 1.0);
    const auto scalar = [](double a, double b, double f) { return a + (b - a) * f; };
    const double size = particle.start_size_meters * curve_value(config.size_keys, t, scalar);
    const double alpha = curve_value(config.alpha_keys, t, scalar);
    auto upper = config.color_keys.lower_bound(t);
    std::array<float, 3> color;
    if (upper == config.color_keys.begin()) color = color_value(upper->second);
    else if (upper == config.color_keys.end()) color = color_value(std::prev(upper)->second);
    else {
      const auto lower = std::prev(upper);
      const auto a = color_value(lower->second), b = color_value(upper->second);
      const float f = static_cast<float>((t - lower->first) / (upper->first - lower->first));
      for (std::size_t i = 0; i < 3; ++i) color[i] = a[i] + (b[i] - a[i]) * f;
    }
    const auto relative = particle.position_meters - frame.camera.pan_meters;
    frame.particles.push_back({
        {static_cast<float>(frame.camera.viewport_width * 0.5 + relative.x * frame.camera.pixels_per_meter),
         static_cast<float>(frame.camera.viewport_height * 0.5 - relative.y * frame.camera.pixels_per_meter)},
        static_cast<float>(size * frame.camera.pixels_per_meter),
        {color[0], color[1], color[2], static_cast<float>(alpha)}
    });
  }
}

} // namespace flop::view
