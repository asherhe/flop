#include "flop/render/texture_cache.hpp"

#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>

#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace flop::render {
namespace {
std::runtime_error failure(const std::string &address, const std::string &reason) {
  return std::runtime_error("texture '" + address + "': " + reason);
}
} // namespace

TextureCache::TextureCache(SDL_GPUDevice *device, config::AssetResolver resolver) :
    device_(device), resolver_(std::move(resolver)) {}

TextureCache::~TextureCache() {
  for (const auto &[address, texture] : textures_) {
    (void)address;
    SDL_ReleaseGPUTexture(device_, texture);
  }
}

SDL_GPUTexture *TextureCache::get_default(DefaultTexture kind) {
  const std::string key = kind == DefaultTexture::flat_normal ? "default:normal" : "default:transparent";
  if (const auto found = textures_.find(key); found != textures_.end())
    return found->second;
  SDL_GPUTextureCreateInfo info{};
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.format = kind == DefaultTexture::flat_normal ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM
                                                     : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
  info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  info.width = info.height = info.layer_count_or_depth = info.num_levels = 1;
  info.sample_count = SDL_GPU_SAMPLECOUNT_1;
  auto *texture = SDL_CreateGPUTexture(device_, &info);
  if (!texture) throw failure(key, SDL_GetError());
  SDL_GPUTransferBufferCreateInfo transfer{};
  transfer.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  transfer.size = 4;
  auto *upload = SDL_CreateGPUTransferBuffer(device_, &transfer);
  if (!upload) {
    const auto error = failure(key, SDL_GetError());
    SDL_ReleaseGPUTexture(device_, texture);
    throw error;
  }
  void *mapped = SDL_MapGPUTransferBuffer(device_, upload, false);
  if (!mapped) {
    const auto error = failure(key, SDL_GetError());
    SDL_ReleaseGPUTransferBuffer(device_, upload);
    SDL_ReleaseGPUTexture(device_, texture);
    throw error;
  }
  const unsigned char normal[4]{128, 128, 255, 255};
  const unsigned char transparent[4]{0, 0, 0, 0};
  SDL_memcpy(mapped, kind == DefaultTexture::flat_normal ? normal : transparent, 4);
  SDL_UnmapGPUTransferBuffer(device_, upload);
  auto *commands = SDL_AcquireGPUCommandBuffer(device_);
  auto *pass = commands ? SDL_BeginGPUCopyPass(commands) : nullptr;
  if (!pass) {
    const auto error = failure(key, SDL_GetError());
    if (commands) SDL_CancelGPUCommandBuffer(commands);
    SDL_ReleaseGPUTransferBuffer(device_, upload);
    SDL_ReleaseGPUTexture(device_, texture);
    throw error;
  }
  SDL_GPUTextureTransferInfo source{};
  source.transfer_buffer = upload;
  SDL_GPUTextureRegion destination{};
  destination.texture = texture;
  destination.w = destination.h = destination.d = 1;
  SDL_UploadToGPUTexture(pass, &source, &destination, false);
  SDL_EndGPUCopyPass(pass);
  const bool submitted = SDL_SubmitGPUCommandBuffer(commands);
  const std::string error = submitted ? "" : SDL_GetError();
  SDL_ReleaseGPUTransferBuffer(device_, upload);
  if (!submitted) {
    SDL_ReleaseGPUTexture(device_, texture);
    throw failure(key, error);
  }
  try { textures_.emplace(key, texture); }
  catch (...) { SDL_ReleaseGPUTexture(device_, texture); throw; }
  return texture;
}

SDL_GPUTexture *TextureCache::get(const std::string &address, TextureColorSpace color_space) {
  const bool srgb = color_space == TextureColorSpace::srgb;
  const std::string key = (srgb ? "srgb:" : "linear:") + address;
  if (const auto found = textures_.find(key); found != textures_.end())
    return found->second;
  try {
    const auto path = resolver_.resolve(config::AssetPath(address));
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input)
      throw failure(address, "cannot open file");
    const auto length = input.tellg();
    if (length <= 0 || length > std::numeric_limits<int>::max())
      throw failure(address, "invalid or oversized file");
    std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char *>(bytes.data()), length))
      throw failure(address, "cannot read file");
    int width = 0, height = 0, channels = 0;
    if (!stbi_info_from_memory(
            bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels
        ))
      throw failure(address, stbi_failure_reason() ? stbi_failure_reason() : "invalid PNG");
    if (width <= 0 || height <= 0 || width > 8192 || height > 8192)
      throw failure(address, "image dimensions exceed the supported limit");
    using Pixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>;
    Pixels pixels(
        stbi_load_from_memory(
            bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels, STBI_rgb_alpha
        ),
        stbi_image_free
    );
    if (!pixels)
      throw failure(address, stbi_failure_reason() ? stbi_failure_reason() : "invalid PNG");
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = srgb ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
                       : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = static_cast<Uint32>(width);
    info.height = static_cast<Uint32>(height);
    info.layer_count_or_depth = info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    SDL_GPUTexture *texture = SDL_CreateGPUTexture(device_, &info);
    if (!texture)
      throw failure(address, SDL_GetError());
    SDL_GPUTransferBufferCreateInfo upload_info{};
    upload_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    upload_info.size = static_cast<Uint32>(width * height * 4);
    SDL_GPUTransferBuffer *upload = SDL_CreateGPUTransferBuffer(device_, &upload_info);
    if (!upload) {
      const auto error = failure(address, SDL_GetError());
      SDL_ReleaseGPUTexture(device_, texture);
      throw error;
    }
    void *mapped = SDL_MapGPUTransferBuffer(device_, upload, false);
    if (!mapped) {
      const auto error = failure(address, SDL_GetError());
      SDL_ReleaseGPUTransferBuffer(device_, upload);
      SDL_ReleaseGPUTexture(device_, texture);
      throw error;
    }
    SDL_memcpy(mapped, pixels.get(), upload_info.size);
    SDL_UnmapGPUTransferBuffer(device_, upload);
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(device_);
    if (!commands) {
      const auto error = failure(address, SDL_GetError());
      SDL_ReleaseGPUTransferBuffer(device_, upload);
      SDL_ReleaseGPUTexture(device_, texture);
      throw error;
    }
    SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass(commands);
    if (!pass) {
      const auto error = failure(address, SDL_GetError());
      SDL_CancelGPUCommandBuffer(commands);
      SDL_ReleaseGPUTransferBuffer(device_, upload);
      SDL_ReleaseGPUTexture(device_, texture);
      throw error;
    }
    SDL_GPUTextureTransferInfo source{};
    source.transfer_buffer = upload;
    SDL_GPUTextureRegion destination{};
    destination.texture = texture;
    destination.w = info.width;
    destination.h = info.height;
    destination.d = 1;
    SDL_UploadToGPUTexture(pass, &source, &destination, false);
    SDL_EndGPUCopyPass(pass);
    const bool submitted = SDL_SubmitGPUCommandBuffer(commands);
    const std::string error = submitted ? "" : SDL_GetError();
    SDL_ReleaseGPUTransferBuffer(device_, upload);
    if (!submitted) {
      SDL_ReleaseGPUTexture(device_, texture);
      throw failure(address, error);
    }
    try {
      textures_.emplace(key, texture);
    } catch (...) {
      SDL_ReleaseGPUTexture(device_, texture);
      throw;
    }
    return texture;
  } catch (const config::AssetError &error) {
    throw failure(address, error.what());
  }
}
} // namespace flop::render
