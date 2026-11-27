#include <SDL3/SDL.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(bool ok, const char *what) {
  if (!ok) throw std::runtime_error(std::string(what) + ": " + SDL_GetError());
}
std::vector<Uint8> read(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) throw std::runtime_error("Cannot open shader " + path);
  return {std::istreambuf_iterator<char>(file), {}};
}
SDL_GPUShader *shader(SDL_GPUDevice *device, const char *stage, bool spirv) {
  const auto code = read(std::string(FLOP_CRAFT_FIXTURE_DIR) + "/" + stage + (spirv ? ".spirv" : ".dxil"));
  SDL_GPUShaderCreateInfo info{};
  info.code = code.data();
  info.code_size = code.size();
  info.entrypoint = stage;
  info.format = spirv ? SDL_GPU_SHADERFORMAT_SPIRV : SDL_GPU_SHADERFORMAT_DXIL;
  const bool fragment = std::string(stage) == "fragment";
  info.stage = fragment ? SDL_GPU_SHADERSTAGE_FRAGMENT : SDL_GPU_SHADERSTAGE_VERTEX;
  info.num_samplers = fragment ? 1 : 0;
  info.num_uniform_buffers = fragment ? 0 : 1;
  auto *result = SDL_CreateGPUShader(device, &info);
  check(result != nullptr, "create shader");
  return result;
}
} // namespace

int main(int argc, char **argv) {
  SDL_GPUDevice *device = nullptr;
  try {
    check(SDL_Init(SDL_INIT_VIDEO), "SDL init");
    const char *driver = argc > 1 ? argv[1] : nullptr;
    SDL_GPUShaderFormat formats = SDL_GPU_SHADERFORMAT_SPIRV;
#ifdef FLOP_PROBE_DXIL
    formats |= SDL_GPU_SHADERFORMAT_DXIL;
#endif
    device = SDL_CreateGPUDevice(formats, true, driver);
    check(device != nullptr, "create GPU device");
    std::printf("Backend: %s\n", SDL_GetGPUDeviceDriver(device));
    const bool spirv = (SDL_GetGPUShaderFormats(device) & SDL_GPU_SHADERFORMAT_SPIRV) != 0;
    auto *vertex = shader(device, "vertex", spirv);
    auto *fragment = shader(device, "fragment", spirv);
    SDL_GPUColorTargetDescription target_desc{};
    target_desc.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    target_desc.blend_state.enable_blend = true;
    target_desc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    target_desc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target_desc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    target_desc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    target_desc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target_desc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    SDL_GPUGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.vertex_shader = vertex;
    pipeline_info.fragment_shader = fragment;
    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
    pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipeline_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pipeline_info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    pipeline_info.target_info.color_target_descriptions = &target_desc;
    pipeline_info.target_info.num_color_targets = 1;
    auto *pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeline_info);
    check(pipeline != nullptr, "create pipeline");
    SDL_ReleaseGPUShader(device, vertex);
    SDL_ReleaseGPUShader(device, fragment);

    SDL_GPUTextureCreateInfo texture_info{};
    texture_info.type = SDL_GPU_TEXTURETYPE_2D;
    texture_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    texture_info.width = texture_info.height = texture_info.layer_count_or_depth = texture_info.num_levels = 1;
    texture_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    texture_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    auto *texture = SDL_CreateGPUTexture(device, &texture_info);
    check(texture != nullptr, "create source texture");
    texture_info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    auto *output = SDL_CreateGPUTexture(device, &texture_info);
    check(output != nullptr, "create output texture");
    SDL_GPUSamplerCreateInfo sampler_info{};
    sampler_info.min_filter = sampler_info.mag_filter = SDL_GPU_FILTER_NEAREST;
    sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler_info.address_mode_u = sampler_info.address_mode_v = sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    auto *sampler = SDL_CreateGPUSampler(device, &sampler_info);
    check(sampler != nullptr, "create sampler");
    SDL_GPUTransferBufferCreateInfo transfer_info{};
    transfer_info.size = 4;
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    auto *upload = SDL_CreateGPUTransferBuffer(device, &transfer_info);
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    auto *download = SDL_CreateGPUTransferBuffer(device, &transfer_info);
    check(upload && download, "create transfer buffers");
    auto *mapped = static_cast<Uint8 *>(SDL_MapGPUTransferBuffer(device, upload, false));
    check(mapped != nullptr, "map upload");
    const int alpha = argc > 2 ? std::stoi(argv[2]) : 255;
    if (alpha < 0 || alpha > 255) throw std::runtime_error("alpha must be between 0 and 255");
    const std::array<Uint8, 4> pixel{200, 100, 40, static_cast<Uint8>(alpha)};
    for (int i = 0; i < 4; ++i) mapped[i] = pixel[i];
    SDL_UnmapGPUTransferBuffer(device, upload);
    auto *commands = SDL_AcquireGPUCommandBuffer(device);
    check(commands != nullptr, "acquire commands");
    auto *copy = SDL_BeginGPUCopyPass(commands);
    check(copy != nullptr, "begin upload pass");
    SDL_GPUTextureTransferInfo upload_location{};
    upload_location.transfer_buffer = upload;
    SDL_GPUTextureRegion source_region{};
    source_region.texture = texture;
    source_region.w = source_region.h = source_region.d = 1;
    SDL_UploadToGPUTexture(copy, &upload_location, &source_region, false);
    SDL_EndGPUCopyPass(copy);
    const float geometry[8] = {0, 0, 1, 0, 0, 1, 1, 1};
    SDL_PushGPUVertexUniformData(commands, 0, geometry, sizeof(geometry));
    SDL_GPUColorTargetInfo target{};
    target.texture = output;
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    auto *pass = SDL_BeginGPURenderPass(commands, &target, 1, nullptr);
    check(pass != nullptr, "begin render pass");
    SDL_BindGPUGraphicsPipeline(pass, pipeline);
    SDL_GPUTextureSamplerBinding binding{texture, sampler};
    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
    SDL_DrawGPUPrimitives(pass, 4, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
    copy = SDL_BeginGPUCopyPass(commands);
    check(copy != nullptr, "begin download pass");
    SDL_GPUTextureRegion output_region{};
    output_region.texture = output;
    output_region.w = output_region.h = output_region.d = 1;
    SDL_GPUTextureTransferInfo download_location{};
    download_location.transfer_buffer = download;
    SDL_DownloadFromGPUTexture(copy, &output_region, &download_location);
    SDL_EndGPUCopyPass(copy);
    auto *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
    check(fence != nullptr, "submit commands");
    check(SDL_WaitForGPUFences(device, true, &fence, 1), "wait for GPU");
    mapped = static_cast<Uint8 *>(SDL_MapGPUTransferBuffer(device, download, false));
    check(mapped != nullptr, "map download");
    const auto blended_srgb = [alpha](int value) {
      const double encoded = value / 255.0;
      const double linear = encoded <= 0.04045 ? encoded / 12.92 : std::pow((encoded + 0.055) / 1.055, 2.4);
      const double blended = linear * alpha / 255.0;
      const double output = blended <= 0.0031308 ? 12.92 * blended : 1.055 * std::pow(blended, 1.0 / 2.4) - 0.055;
      return static_cast<int>(std::lround(output * 255));
    };
    const std::array<int, 4> expected{blended_srgb(200), blended_srgb(100), blended_srgb(40), alpha};
    bool match = true;
    for (int i = 0; i < 4; ++i) {
      std::printf("%u%s", static_cast<unsigned>(mapped[i]), i == 3 ? "\n" : ", ");
      if (std::abs(static_cast<int>(mapped[i]) - expected[i]) > 2) match = false;
    }
    SDL_UnmapGPUTransferBuffer(device, download);
    SDL_ReleaseGPUFence(device, fence);
    SDL_ReleaseGPUTransferBuffer(device, upload);
    SDL_ReleaseGPUTransferBuffer(device, download);
    SDL_ReleaseGPUSampler(device, sampler);
    SDL_ReleaseGPUTexture(device, texture);
    SDL_ReleaseGPUTexture(device, output);
    SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
    SDL_DestroyGPUDevice(device);
    device = nullptr;
    SDL_Quit();
    if (!match) throw std::runtime_error("sampled pixel did not match expected value");
    return 0;
  } catch (const std::exception &error) {
    std::fprintf(stderr, "Craft shader probe: %s\n", error.what());
    if (device) SDL_DestroyGPUDevice(device);
    SDL_Quit();
    return 1;
  }
}
