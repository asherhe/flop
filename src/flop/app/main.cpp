#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_Init: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Flop SDL_GPU smoke test",
        1280,
        720,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_CreateWindow: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Tell SDL which shader formats the eventual renderer will provide.
    // This clear-only test does not load a shader.
#ifdef _WIN32
    constexpr SDL_GPUShaderFormat SHADER_FORMATS =
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL;
#elif defined(__APPLE__)
    constexpr SDL_GPUShaderFormat SHADER_FORMATS =
        SDL_GPU_SHADERFORMAT_METALLIB;
#else
    constexpr SDL_GPUShaderFormat SHADER_FORMATS =
        SDL_GPU_SHADERFORMAT_SPIRV;
#endif

    SDL_GPUDevice* device =
        SDL_CreateGPUDevice(SHADER_FORMATS, true, nullptr);
    if (!device) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_CreateGPUDevice: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Log("GPU backend: %s", SDL_GetGPUDeviceDriver(device));

    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_ClaimWindowForGPUDevice: %s",
                     SDL_GetError());
        SDL_DestroyGPUDevice(device);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;
    int exit_code = 0;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN &&
                 event.key.key == SDLK_ESCAPE)) {
                running = false;
            }
        }
        if (!running) {
            break;
        }

        SDL_GPUCommandBuffer* command =
            SDL_AcquireGPUCommandBuffer(device);
        if (!command) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL_AcquireGPUCommandBuffer: %s",
                         SDL_GetError());
            exit_code = 1;
            break;
        }

        SDL_GPUTexture* swapchain = nullptr;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(
                command, window, &swapchain, nullptr, nullptr)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL_WaitAndAcquireGPUSwapchainTexture: %s",
                         SDL_GetError());
            SDL_CancelGPUCommandBuffer(command);
            exit_code = 1;
            break;
        }

        // A minimized window can return a null swapchain texture.
        if (swapchain) {
            SDL_GPUColorTargetInfo target{};
            target.texture = swapchain;
            target.clear_color = SDL_FColor{0.02f, 0.05f, 0.12f, 1.0f};
            target.load_op = SDL_GPU_LOADOP_CLEAR;
            target.store_op = SDL_GPU_STOREOP_STORE;

            SDL_GPURenderPass* pass =
                SDL_BeginGPURenderPass(command, &target, 1, nullptr);
            if (!pass) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "SDL_BeginGPURenderPass: %s",
                             SDL_GetError());
                // After acquiring a swapchain texture, submit rather than
                // cancel the command buffer.
                SDL_SubmitGPUCommandBuffer(command);
                exit_code = 1;
                break;
            }

            SDL_EndGPURenderPass(pass);
        }

        // Submitting also presents an acquired swapchain texture.
        if (!SDL_SubmitGPUCommandBuffer(command)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL_SubmitGPUCommandBuffer: %s",
                         SDL_GetError());
            exit_code = 1;
            break;
        }
    }

    SDL_WaitForGPUIdle(device);
    SDL_ReleaseWindowFromGPUDevice(device, window);
    SDL_DestroyGPUDevice(device);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exit_code;
}
