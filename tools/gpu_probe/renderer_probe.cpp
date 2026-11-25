#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>

#include "flop/config/asset_path.hpp"
#include "flop/render/renderer.hpp"
#include "flop/view/frame.hpp"

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
    write_png(assets / "opaque.png", opaque);
    write_png(assets / "half.png", half);
    write_png(assets / "transparent.png", transparent);
    { std::ofstream out(assets / "invalid.png"); out << "not a png"; }
    if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
    SDL_Window *window = SDL_CreateWindow("renderer probe", 16, 16, SDL_WINDOW_HIDDEN);
    if (!window) throw std::runtime_error(SDL_GetError());
    {
      flop::render::Renderer renderer(window, flop::config::AssetResolver(assets));
      flop::view::FrameDrawData frame;
      frame.camera.viewport_width = frame.camera.viewport_height = 1;
      flop::view::SpriteDraw sprite{};
      sprite.visual.material.shader = "craft/sprite";
      sprite.visual.sprite_source.address = "assets/test_part.yaml";
      sprite.visual.sprite_source.key = "plugins.SpritePlugin.sprite";
      sprite.corners_pixels = {{{0, 1}, {1, 1}, {1, 0}, {0, 0}}};
      frame.sprites.push_back(sprite);
      for (const auto &[name, alpha] : std::array<std::pair<const char *, int>, 3>{{{"opaque.png",255}, {"half.png",128}, {"transparent.png",0}}}) {
        frame.sprites[0].visual.sprite = std::string("assets/") + name;
        const auto pixel = renderer.readback_pixel(frame);
        const std::array<int,4> target{expected(200,.02,alpha), expected(100,.05,alpha), expected(40,.12,alpha),255};
        for (int i = 0; i < 4; ++i) if (std::abs(int(pixel[i]) - target[i]) > 2)
          throw std::runtime_error(std::string(name) + " pixel mismatch at channel " + std::to_string(i) + ": " + std::to_string(pixel[i]) + " expected " + std::to_string(target[i]));
        std::cout << name << ": " << int(pixel[0]) << ", " << int(pixel[1]) << ", " << int(pixel[2]) << ", " << int(pixel[3]) << '\n';
      }
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
