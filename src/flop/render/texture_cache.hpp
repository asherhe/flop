#pragma once

#include <string>
#include <unordered_map>

#include "flop/config/asset_path.hpp"

struct SDL_GPUDevice;
struct SDL_GPUTexture;

namespace flop::render {

enum class TextureColorSpace { linear, srgb };

// owns uploaded GPU textures
class TextureCache {
public:
  TextureCache(SDL_GPUDevice *device, config::AssetResolver resolver);
  TextureCache(const TextureCache &) = delete;
  TextureCache &operator=(const TextureCache &) = delete;
  ~TextureCache();

  SDL_GPUTexture *get(const std::string &address, TextureColorSpace color_space);

private:
  SDL_GPUDevice *device_;
  config::AssetResolver resolver_;
  std::unordered_map<std::string, SDL_GPUTexture *> textures_;
};

} // namespace flop::render
