#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

#include "flop/config/asset_path.hpp"

struct SDL_GPUDevice;
struct SDL_GPUTexture;

namespace flop::render {

enum class TextureColorSpace { linear, srgb };
enum class DefaultTexture { flat_normal, transparent };

// owns uploaded GPU textures
class TextureCache {
public:
  TextureCache(SDL_GPUDevice *device, config::AssetResolver resolver);
  TextureCache(const TextureCache &) = delete;
  TextureCache &operator=(const TextureCache &) = delete;
  ~TextureCache();

  SDL_GPUTexture *get(const std::string &address, TextureColorSpace color_space);
  SDL_GPUTexture *get_default(DefaultTexture kind);
  SDL_GPUTexture *get_embedded(const std::string &name, const unsigned char *bytes, std::size_t size);

private:
  SDL_GPUTexture *load_png(const std::string &key, const std::string &address,
                           const unsigned char *bytes, std::size_t size, bool srgb);
  SDL_GPUDevice *device_;
  config::AssetResolver resolver_;
  std::unordered_map<std::string, SDL_GPUTexture *> textures_;
};

} // namespace flop::render
