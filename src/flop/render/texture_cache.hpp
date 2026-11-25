#pragma once

#include <string>
#include <unordered_map>

#include "flop/config/asset_path.hpp"

struct SDL_GPUDevice;
struct SDL_GPUTexture;

namespace flop::render {

// owns uploaded GPU textures
class TextureCache {
public:
  TextureCache(SDL_GPUDevice *device, config::AssetResolver resolver);
  TextureCache(const TextureCache &) = delete;
  TextureCache &operator=(const TextureCache &) = delete;
  ~TextureCache();

  SDL_GPUTexture *get_srgb(const std::string &address);

private:
  SDL_GPUDevice *device_;
  config::AssetResolver resolver_;
  std::unordered_map<std::string, SDL_GPUTexture *> textures_;
};

} // namespace flop::render
