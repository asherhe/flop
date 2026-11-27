#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>

#include "flop/config/asset_path.hpp"
#include "flop/render/renderer.hpp"
#include "flop/view/frame.hpp"
#include "post_process_fixtures.hpp"

namespace {
struct TempDirectory {
  std::filesystem::path path;
  TempDirectory() {
    do { path = std::filesystem::temp_directory_path() / ("flop-render-test-" + std::to_string(std::random_device{}())); }
    while (!std::filesystem::create_directory(path));
  }
  ~TempDirectory() { std::error_code error; std::filesystem::remove_all(path, error); }
};
constexpr std::array<unsigned char, 70> opaque{137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,8,6,0,0,0,31,21,196,137,0,0,0,13,73,68,65,84,120,156,99,56,145,162,241,31,0,5,160,2,84,204,94,113,233,0,0,0,0,73,69,78,68,174,66,96,130};
constexpr std::array<unsigned char, 70> half{137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,8,6,0,0,0,31,21,196,137,0,0,0,13,73,68,65,84,120,156,99,56,145,162,209,0,0,5,33,1,213,160,6,105,249,0,0,0,0,73,69,78,68,174,66,96,130};
constexpr std::array<unsigned char, 70> transparent{137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,8,6,0,0,0,31,21,196,137,0,0,0,13,73,68,65,84,120,156,99,56,145,162,193,0,0,4,161,1,85,115,233,10,242,0,0,0,0,73,69,78,68,174,66,96,130};
constexpr std::array<unsigned char, 70> normal_x{137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,8,6,0,0,0,31,21,196,137,0,0,0,13,73,68,65,84,120,156,99,248,223,208,240,31,0,7,128,2,255,57,243,206,143,0,0,0,0,73,69,78,68,174,66,96,130};
void write_png(const std::filesystem::path &path, const std::array<unsigned char, 70> &data) {
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char *>(data.data()), data.size());
  if (!out) throw std::runtime_error("failed to write fixture PNG");
}
int expected(int srgb, double background, int alpha) {
  const double s = srgb / 255.0;
  const double linear = s <= .04045 ? s / 12.92 : std::pow((s + .055) / 1.055, 2.4);
  const double v = linear * alpha / 255.0 + background * (1.0 - alpha / 255.0);
  return static_cast<int>(std::lround(255 * (v <= .0031308 ? 12.92 * v : 1.055 * std::pow(v, 1.0 / 2.4) - .055)));
}
int lit_expected(int srgb, double multiplier) {
  const double s = srgb / 255.0;
  const double linear = s <= .04045 ? s / 12.92 : std::pow((s + .055) / 1.055, 2.4);
  const double v = std::clamp(linear * multiplier, 0.0, 1.0);
  return static_cast<int>(std::lround(255 * (v <= .0031308 ? 12.92 * v : 1.055 * std::pow(v, 1.0 / 2.4) - .055)));
}
int lit_blended_expected(int srgb, double multiplier, int alpha) {
  const double s = srgb / 255.0;
  const double linear = s <= .04045 ? s / 12.92 : std::pow((s + .055) / 1.055, 2.4);
  const double v = std::clamp(linear * multiplier * alpha / 255.0, 0.0, 1.0);
  return static_cast<int>(std::lround(255 * (v <= .0031308 ? 12.92 * v : 1.055 * std::pow(v, 1.0 / 2.4) - .055)));
}
}

int main() {
  try {
    TempDirectory temp;
    const auto assets = temp.path / "assets";
    std::filesystem::create_directories(assets / "shared" / "shaders");
    { std::ofstream out(assets / "shared" / "shaders" / "craft_sprite.yaml");
      out << "shader: craft/sprite\nproperties:\n"
             "  albedo: {stage: fragment, type: texture_2d}\n"
             "  origin_and_x_axis: {stage: vertex, type: float4, auto: sprite.projected_origin_and_x_axis}\n"
             "  y_axis_and_viewport: {stage: vertex, type: float4, auto: sprite.projected_y_axis_and_viewport}\n"; }
    { std::ofstream out(assets / "shared" / "shaders" / "craft_lit_sprite.yaml");
      out << "shader: craft/lit_sprite\nproperties:\n"
             "  albedo: {stage: fragment, type: texture_2d}\n"
             "  normal_map: {stage: fragment, type: texture_2d}\n"
             "  emission_map: {stage: fragment, type: texture_2d}\n"
             "  local_light: {stage: fragment, type: texture_2d, auto: local_light.texture}\n"
             "  origin_and_x_axis: {stage: vertex, type: float4, auto: sprite.projected_origin_and_x_axis}\n"
             "  y_axis_and_viewport: {stage: vertex, type: float4, auto: sprite.projected_y_axis_and_viewport}\n"
             "  sun_direction: {stage: fragment, type: float4, auto: sun.direction}\n"
             "  sun_color: {stage: fragment, type: float4, auto: sun.color}\n"
             "  planet_shine_direction_and_spread: {stage: fragment, type: float4, auto: planet_shine.direction_and_spread}\n"
             "  planet_shine_color: {stage: fragment, type: float4, auto: planet_shine.color}\n"; }
    write_png(assets / "opaque.png", opaque);
    write_png(assets / "half.png", half);
    write_png(assets / "transparent.png", transparent);
    write_png(assets / "normal_x.png", normal_x);
    { std::ofstream out(assets / "invalid.png"); out << "not a png"; }
    if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
    SDL_Window *window = SDL_CreateWindow("renderer probe", 16, 16, SDL_WINDOW_HIDDEN);
    if (!window) throw std::runtime_error(SDL_GetError());
    {
      flop::render::Renderer renderer(window, flop::config::AssetResolver(assets));
      flop::view::FrameDrawData frame;
      frame.camera.viewport_width = frame.camera.viewport_height = 1;
      frame.particles.push_back({{0.5f, 0.5f}, 1.0f, hdr_red});
      const auto hdr = renderer.readback_scene(frame);
      if (hdr != hdr_red_half)
        throw std::runtime_error("scene target did not retain HDR particle color");
      if (renderer.readback_sanitized(frame) != hdr_red_half)
        throw std::runtime_error("sanitization changed finite HDR color");
      frame.particles[0].color = {std::numeric_limits<float>::infinity(), 0.25f,
                                  std::numeric_limits<float>::quiet_NaN(), 1.0f};
      const auto nonfinite = renderer.readback_scene(frame);
      if ((nonfinite[0] & 0x7c00) != 0x7c00 || (nonfinite[2] & 0x7fff) <= 0x7c00)
        throw std::runtime_error("non-finite fixture did not reach scene target");
      const auto sanitized = renderer.readback_sanitized(frame);
      if (sanitized != std::array<std::uint16_t, 4>{0, 0x3400, 0, 0x3c00})
        throw std::runtime_error("sanitization did not isolate non-finite channels");
      frame.particles.clear();
      frame.particles.push_back({{0.5f, 0.5f}, 1.0f, {1, 0, 0, 0.5f}});
      const auto particle_pixel = renderer.readback_pixel(frame);
      if (std::abs(int(particle_pixel[0]) - 188) > 3 || particle_pixel[1] > 2 || particle_pixel[2] > 2)
        throw std::runtime_error("particle alpha blending mismatch");
      frame.particles[0].center_pixels = {3, 3};
      const auto control_pixel = renderer.readback_pixel(frame);
      if (control_pixel[0] > 2 || control_pixel[1] > 2 || control_pixel[2] > 2)
        throw std::runtime_error("particle control pixel should remain black");
      frame.particles.clear();
      frame.particles.push_back({{1.08f, 0.5f}, 1.0f, {1, 0, 0, 1}, 0.0f});
      const auto unrotated = renderer.readback_pixel(frame);
      if (unrotated[0] > 2) throw std::runtime_error("unrotated particle should miss control pixel");
      frame.particles[0].angle_rad = 0.785398163f;
      const auto rotated = renderer.readback_pixel(frame);
      if (rotated[0] < 250) throw std::runtime_error("rotated particle should cover control pixel");
      frame.particles.clear();
      flop::view::SpriteDraw sprite{};
      sprite.visual.material.shader = "craft/sprite";
      sprite.visual.sprite_source.address = "assets/test_part.yaml";
      sprite.visual.sprite_source.key = "plugins.SpritePlugin.sprite";
      sprite.corners_pixels = {{{0, 1}, {1, 1}, {1, 0}, {0, 0}}};
      frame.sprites.push_back(sprite);
      for (const auto &[name, alpha] : std::array<std::pair<const char *, int>, 3>{{{"opaque.png",255}, {"half.png",128}, {"transparent.png",0}}}) {
        frame.sprites[0].visual.sprite = std::string("assets/") + name;
        const auto pixel = renderer.readback_pixel(frame);
        const std::array<int,4> target{expected(200,0,alpha), expected(100,0,alpha), expected(40,0,alpha),255};
        for (int i = 0; i < 4; ++i) if (std::abs(int(pixel[i]) - target[i]) > 2)
          throw std::runtime_error(std::string(name) + " pixel mismatch at channel " + std::to_string(i) + ": " + std::to_string(pixel[i]) + " expected " + std::to_string(target[i]));
        std::cout << name << ": " << int(pixel[0]) << ", " << int(pixel[1]) << ", " << int(pixel[2]) << ", " << int(pixel[3]) << '\n';
      }
      frame.particles.push_back({{0.5f, 0.5f}, 1.0f, {1, 0, 0, 1}});
      frame.sprites[0].visual.sprite = "assets/opaque.png";
      const auto ordered = renderer.readback_pixel(frame);
      if (std::abs(int(ordered[0]) - 200) > 2 || std::abs(int(ordered[1]) - 100) > 2 ||
          std::abs(int(ordered[2]) - 40) > 2)
        throw std::runtime_error("craft sprite should draw over particles");
      frame.particles.clear();
      frame.sprites[0].visual.material.shader = "craft/lit_sprite";
      frame.sprites[0].visual.sprite = "assets/opaque.png";
      frame.sprites[0].sun.direction = {1, 0, 0, 0};
      frame.sprites[0].sun.color = {0, 0, 0, 0};
      auto normal_pixel = renderer.readback_normal(frame);
      if (std::abs(int(normal_pixel[0]) - 0x3800) > 8 ||
          std::abs(int(normal_pixel[1]) - 0x3800) > 8 || normal_pixel[2] < 0x3bf0)
        throw std::runtime_error("default normal mismatch");
      frame.sprites[0].visual.material.normal_map = "assets/normal_x.png";
      normal_pixel = renderer.readback_normal(frame);
      if (normal_pixel[0] < 0x3bf0) throw std::runtime_error("positive x normal mismatch");
      frame.sprites[0].angle_rad = 3.141592653589793;
      normal_pixel = renderer.readback_normal(frame);
      if (normal_pixel[0] > 0x2000) throw std::runtime_error("rotated normal mismatch");
      frame.sprites[0].angle_rad = 0.0;
      frame.sprites[0].visual.sprite = "assets/half.png";
      normal_pixel = renderer.readback_normal(frame);
      if (std::abs(int(normal_pixel[0]) - 0x3a00) > 8)
        throw std::runtime_error("transparent normal blend mismatch");
      auto foreground = frame.sprites[0];
      foreground.angle_rad = 3.141592653589793;
      frame.sprites[0].visual.sprite = "assets/opaque.png";
      frame.sprites.push_back(foreground);
      normal_pixel = renderer.readback_normal(frame);
      if (std::abs(int(normal_pixel[0]) - 0x3800) > 16)
        throw std::runtime_error("overlapping normal blend mismatch");
      std::swap(frame.sprites[0], frame.sprites[1]);
      normal_pixel = renderer.readback_normal(frame);
      if (normal_pixel[0] < 0x3bf0)
        throw std::runtime_error("normal blend order mismatch");
      frame.sprites.erase(frame.sprites.begin());
      frame.sprites[0].angle_rad = 0.0;
      frame.sprites[0].visual.sprite = "assets/transparent.png";
      normal_pixel = renderer.readback_normal(frame);
      if (normal_pixel[0] != 0x3800 || normal_pixel[2] != 0x3c00)
        throw std::runtime_error("transparent normal clear mismatch");
      frame.camera.viewport_width = frame.camera.viewport_height = 2;
      frame.sprites.clear();
      normal_pixel = renderer.readback_normal(frame);
      if (normal_pixel[0] != 0x3800 || normal_pixel[2] != 0x3c00)
        throw std::runtime_error("resized normal clear mismatch");
      frame.camera.viewport_width = frame.camera.viewport_height = 1;
      frame.sprites.push_back(sprite);
      frame.sprites[0].visual.material.shader = "craft/lit_sprite";
      frame.sprites[0].visual.sprite = "assets/opaque.png";
      frame.sprites[0].sun.direction = {1, 0, 0, 0};
      frame.sprites[0].sun.color = {0, 0, 0, 0};
      auto pixel = renderer.readback_pixel(frame);
      for (int i = 0; i < 3; ++i)
        if (pixel[i] > 2) throw std::runtime_error("unlit default maps should produce black");
      flop::view::LightDraw local{};
      local.center_pixels = {0.5f, 0.5f};
      local.inner_radius_pixels = 0.2f;
      local.outer_radius_pixels = 1.0f;
      local.falloff = 0.5f;
      local.z_offset_pixels = 10000.0f;
      local.color = {0.25f, 0.25f, 0.25f};
      frame.lights.push_back(local);
      pixel = renderer.readback_pixel(frame);
      const int albedo[3]{200, 100, 40};
      for (int i = 0; i < 3; ++i)
        if (std::abs(int(pixel[i]) - lit_expected(albedo[i], .25)) > 3)
          throw std::runtime_error("local light center mismatch");
      frame.sprites[0].visual.material.normal_map = "assets/normal_x.png";
      frame.lights[0].center_pixels = {1.0f, 0.5f};
      frame.lights[0].inner_radius_pixels = 1.0f;
      frame.lights[0].outer_radius_pixels = 2.0f;
      frame.lights[0].z_offset_pixels = 0.0f;
      pixel = renderer.readback_pixel(frame);
      if (std::abs(int(pixel[0]) - lit_expected(200, .25)) > 3)
        throw std::runtime_error("aligned local normal mismatch");
      frame.lights[0].z_offset_pixels = 1.0f;
      pixel = renderer.readback_pixel(frame);
      if (std::abs(int(pixel[0]) - lit_expected(200, .25 / std::sqrt(5.0))) > 3)
        throw std::runtime_error("local light height response mismatch");
      frame.lights[0].z_offset_pixels = 0.0f;
      frame.sprites[0].angle_rad = 3.141592653589793;
      pixel = renderer.readback_pixel(frame);
      if (pixel[0] > 2 || pixel[1] > 2 || pixel[2] > 2)
        throw std::runtime_error("rotated local normal should be opposed");
      frame.sprites[0].angle_rad = 0.0;
      frame.lights[0].center_pixels = {0.0f, 0.5f};
      pixel = renderer.readback_pixel(frame);
      if (pixel[0] > 2 || pixel[1] > 2 || pixel[2] > 2)
        throw std::runtime_error("opposed local normal should be dark");
      frame.sprites[0].visual.material.normal_map.clear();
      frame.lights[0] = local;
      frame.lights[0].center_pixels = {1.1f, 0.5f}; // t = 0.5, u = 0.625
      pixel = renderer.readback_pixel(frame);
      for (int i = 0; i < 3; ++i)
        if (std::abs(int(pixel[i]) - lit_expected(albedo[i], .25 * .31640625)) > 3)
          throw std::runtime_error("local light falloff mismatch");
      frame.lights[0].center_pixels = {2.0f, 0.5f};
      pixel = renderer.readback_pixel(frame);
      if (pixel[0] > 2 || pixel[1] > 2 || pixel[2] > 2)
        throw std::runtime_error("outside-radius control should be dark");
      frame.lights[0] = local;
      frame.lights[0].color = {0, 1, 0};
      frame.lights.push_back(frame.lights[0]);
      pixel = renderer.readback_pixel(frame);
      if (pixel[0] > 2 || std::abs(int(pixel[1]) - lit_expected(100, 2.0)) > 3 || pixel[2] > 2)
        throw std::runtime_error("additive light overlap mismatch");
      frame.sprites[0].visual.sprite = "assets/half.png";
      pixel = renderer.readback_pixel(frame);
      if (std::abs(int(pixel[1]) - lit_blended_expected(100, 2.0, 128)) > 3 || pixel[3] != 255)
        throw std::runtime_error("local light alpha mismatch");
      frame.sprites[0].visual.sprite = "assets/opaque.png";
      frame.sprites[0].visual.material.shader = "craft/sprite";
      pixel = renderer.readback_pixel(frame);
      if (std::abs(int(pixel[0]) - 200) > 2 || std::abs(int(pixel[1]) - 100) > 2)
        throw std::runtime_error("unlit sprite should ignore local lights");
      frame.sprites.clear();
      frame.particles.push_back({{0.5f, 0.5f}, 1.0f, {1, 0, 0, 1}});
      pixel = renderer.readback_pixel(frame);
      if (pixel[0] < 250 || pixel[1] > 2) throw std::runtime_error("particles should ignore local lights");
      frame.particles.clear();
      frame.sprites.push_back(sprite);
      frame.sprites[0].visual.material.shader = "craft/lit_sprite";
      frame.sprites[0].visual.sprite = "assets/opaque.png";
      frame.sprites[0].sun.direction = {1, 0, 0, 0};
      frame.sprites[0].sun.color = {0, 0, 0, 0};
      frame.lights.clear();
      pixel = renderer.readback_pixel(frame);
      if (pixel[0] > 2 || pixel[1] > 2 || pixel[2] > 2)
        throw std::runtime_error("empty light pass must clear without resize");
      frame.lights.push_back(local);
      frame.lights[0].outer_radius_pixels = 0;
      pixel = renderer.readback_pixel(frame);
      if (pixel[0] > 2 || pixel[1] > 2 || pixel[2] > 2)
        throw std::runtime_error("zero-radius light should be dark");
      frame.lights[0] = local;
      frame.lights[0].color = {0, 1, 0};
      frame.lights.push_back(frame.lights[0]);
      frame.camera.viewport_width = frame.camera.viewport_height = 2;
      frame.sprites[0].corners_pixels = {{{0, 2}, {2, 2}, {2, 0}, {0, 0}}};
      pixel = renderer.readback_pixel(frame);
      if (std::abs(int(pixel[1]) - lit_expected(100, 2.0)) > 3)
        throw std::runtime_error("resized light texture mismatch");
      frame.camera.viewport_width = frame.camera.viewport_height = 1;
      frame.sprites[0].corners_pixels = {{{0, 1}, {1, 1}, {1, 0}, {0, 0}}};
      frame.lights.clear();
      pixel = renderer.readback_pixel(frame);
      if (pixel[0] > 2 || pixel[1] > 2 || pixel[2] > 2)
        throw std::runtime_error("empty light pass must clear previous contributions");
      std::cout << "local lights, falloff, overlap, alpha, controls and resize: passed\n";
      frame.sprites[0].visual.material.emission_map = "assets/opaque.png";
      pixel = renderer.readback_pixel(frame);
      const std::array<int, 3> emission{200, 100, 40};
      for (int i = 0; i < 3; ++i)
        if (std::abs(int(pixel[i]) - emission[i]) > 2)
          throw std::runtime_error("lit emission overlay mismatch");
      frame.sprites[0].visual.material.emission_map.clear();
      frame.sprites[0].sun.color = {1, 1, 1, 0};
      pixel = renderer.readback_pixel(frame);
      const double normal_component = 1.0 / 255.0;
      const double multiplier = 2.0 * (normal_component + 0.3) /
          (std::sqrt(1.0 + 2.0 * normal_component * normal_component) * std::sqrt(1.09));
      for (int i = 0; i < 3; ++i)
        if (std::abs(int(pixel[i]) - lit_expected(emission[i], multiplier)) > 3)
          throw std::runtime_error("default normal sunlight pixel mismatch");
      frame.sprites[0].visual.material.normal_map = "assets/normal_x.png";
      frame.sprites[0].sun.color = {.5f, .5f, .5f, 0};
      pixel = renderer.readback_pixel(frame);
      if (std::abs(int(pixel[0]) - 195) > 3) throw std::runtime_error("aligned normal pixel mismatch");
      frame.sprites[0].sun.direction = {-1, 0, 0, 0};
      pixel = renderer.readback_pixel(frame);
      if (pixel[0] > 2 || pixel[1] > 2 || pixel[2] > 2) throw std::runtime_error("opposed normal should be dark");
      frame.sprites[0].sun.color = {0, 0, 0, 0};
      frame.sprites[0].planet_shine.direction_and_spread = {1, 0, 0, 0};
      frame.sprites[0].planet_shine.color = {.5f, .5f, .5f, 0};
      pixel = renderer.readback_pixel(frame);
      if (std::abs(int(pixel[0]) - lit_expected(200, .5)) > 3)
        throw std::runtime_error("planet shine aligned pixel mismatch");
      frame.sprites[0].planet_shine.direction_and_spread = {-1, 0, 0, 0};
      pixel = renderer.readback_pixel(frame);
      if (pixel[0] > 2 || pixel[1] > 2 || pixel[2] > 2)
        throw std::runtime_error("opposed planet shine should be dark");
      frame.sprites[0].planet_shine = {};
      frame.sprites[0].sun.color = {.5f, .5f, .5f, 0};
      // rotate the quad while supplying the corresponding local light direction
      frame.sprites[0].corners_pixels = {{{1, 1}, {1, 0}, {0, 0}, {0, 1}}};
      frame.sprites[0].sun.direction = {1, 0, 0, 0};
      pixel = renderer.readback_pixel(frame);
      if (std::abs(int(pixel[0]) - 195) > 3) throw std::runtime_error("rotated sprite pixel mismatch");
      frame.sprites[0].corners_pixels = {{{2, 2}, {3, 2}, {3, 3}, {2, 3}}};
      pixel = renderer.readback_pixel(frame);
      if (pixel[0] > 2 || pixel[1] > 2 || pixel[2] > 2 || pixel[3] != 255)
        throw std::runtime_error("background control pixel mismatch");
      frame.sprites[0].corners_pixels = {{{0, 1}, {1, 1}, {1, 0}, {0, 0}}};
      frame.sprites[0].visual.material.normal_map = "assets/invalid.png";
      frame.sprites[0].visual.material.normal_map_source.address = "assets/test_part.yaml";
      frame.sprites[0].visual.material.normal_map_source.key = "material.properties.normal_map";
      try { renderer.readback_pixel(frame); throw std::runtime_error("invalid normal accepted"); }
      catch (const std::exception &e) {
        const std::string text = e.what();
        if (text.find("assets/test_part.yaml") == std::string::npos || text.find("material.properties.normal_map") == std::string::npos || text.find("assets/invalid.png") == std::string::npos) throw;
      }
      frame.sprites[0].visual.material.normal_map.clear();
      frame.sprites[0].visual.material.emission_map = "assets/invalid.png";
      frame.sprites[0].visual.material.emission_map_source.address = "assets/test_part.yaml";
      frame.sprites[0].visual.material.emission_map_source.key = "material.properties.emission_map";
      try { renderer.readback_pixel(frame); throw std::runtime_error("invalid emission accepted"); }
      catch (const std::exception &e) {
        const std::string text = e.what();
        if (text.find("assets/test_part.yaml") == std::string::npos || text.find("material.properties.emission_map") == std::string::npos || text.find("assets/invalid.png") == std::string::npos) throw;
      }
      frame.sprites[0].visual.material.emission_map = "assets/half.png";
      frame.sprites[0].visual.sprite = "assets/transparent.png";
      frame.sprites[0].sun.color = {0, 0, 0, 0};
      pixel = renderer.readback_pixel(frame);
      for (int i = 0; i < 3; ++i) {
        const int target = expected(emission[i], 0, 128);
        if (std::abs(int(pixel[i]) - target) > 2) throw std::runtime_error("emission alpha overlay mismatch");
      }
      frame.sprites[0].visual.material.emission_map.clear();
      pixel = renderer.readback_pixel(frame);
      if (pixel[0] > 2 || pixel[1] > 2 || pixel[2] > 2 || pixel[3] != 255)
        throw std::runtime_error("zero alpha control mismatch");
      std::cout << "lit default maps, emission and sunlight: passed\n";
      frame.sprites[0].visual.material.shader = "craft/sprite";
      frame.sprites[0].visual.sprite = "assets/invalid.png";
      try { renderer.readback_pixel(frame); throw std::runtime_error("invalid PNG accepted"); }
      catch (const std::exception &e) {
        const std::string text = e.what();
        if (text.find("assets/test_part.yaml") == std::string::npos || text.find("plugins.SpritePlugin.sprite") == std::string::npos || text.find("assets/invalid.png") == std::string::npos) throw;
        std::cout << "invalid PNG diagnostic: " << text << '\n';
      }
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
  } catch (const std::exception &error) { std::cerr << "renderer probe: " << error.what() << '\n'; SDL_Quit(); return 1; }
}
