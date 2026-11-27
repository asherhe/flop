# GPU sampling probe

Configure a game-enabled preset, then explicitly build the probe:

```sh
cmake --preset windows-debug
cmake --build --preset windows-debug --target flop_gpu_probe
build/windows-debug/tools/gpu_probe/flop_gpu_probe.exe vulkan
```

On Linux, use the `linux-debug` preset and executable without `.exe`. Pass
`vulkan` or `direct3d12` to request that backend, or omit the argument to let
SDL choose. The probe creates an offscreen render
target, uploads a known RGBA texel, samples it with a nearest-filtered sampler,
applies a uniform tint, and checks the readback within one integer per channel.
It exits nonzero if device setup, rendering, or the pixel check fails. A GPU and
working backend are required. It is excluded from the default build and CTest.

This checks one combined image-sampler binding and one uniform buffer. It does
not validate shader reflection metadata, texture loading, interpolation,
multiple resource slots, color-space behavior, or presentation.
# Craft shader probe

Build with `cmake --build build/windows-debug --target flop_craft_probe` and run
`build/windows-debug/tools/gpu_probe/flop_craft_probe.exe direct3d12 128` (use
alpha 0, 128, or 255). It compiles the craft sprite shader and reads back a
single sRGB texel blended over transparent black. This is an offscreen shader
check; it does not exercise the game renderer, YAML recipe, PNG loading, UV
orientation, or presentation.
# Renderer path probe

Build with `cmake --build build/windows-debug --target flop_renderer_probe` and
run `build/windows-debug/tools/gpu_probe/flop_renderer_probe.exe`. It creates
temporary recipe and PNG fixtures, exercises the renderer's recipe loading,
texture cache, pipeline and sprite draw, and reads back one sRGB pixel for
opaque, half-transparent and transparent inputs. It checks local-light center,
falloff, outside-radius, additive overlap above one, sprite alpha, unaffected
unlit sprites and particles, clearing, and target resize. Color comparisons
use a tolerance of up to three byte values. It also checks the source context
in an invalid-PNG error. The offscreen capture uses the selected swapchain
format and normal renderer draw code. It also reads a floating-point scene pixel
and verifies a red value of 2 survives before the identity display pass. It does
not test presentation or
multi-pixel UV orientation. The shaders must be packaged for the selected
backend.

On Windows, set `$env:SDL_GPU_DRIVER = 'direct3d12'` or `'vulkan'` before
running the executable. On Linux, build with
`cmake --build --preset linux-debug --target flop_renderer_probe` and run
`SDL_GPU_DRIVER=vulkan build/linux-debug/tools/gpu_probe/flop_renderer_probe`.

The renderer probe passed on Windows 10.0.26300 with an NVIDIA GeForce RTX 4050
Laptop GPU, using Direct3D 12 (driver 32.0.15.7703) and Vulkan (SDL-reported
driver 577.3.0.0). The readback checks allow up to three byte values of error.
Linux and live-scene appearance have not been verified by this probe.
The normal-aware extension also reads back the floating-point normal texture,
checking flat and rotated normals, alpha blending, both orders of overlapping
sprites, clearing and resize. Its lighting checks include aligned and opposed
normals and different light heights. Both Windows backends above passed.

# Shipped scene lighting probe

Build with `cmake --build --preset windows-debug --target flop_scene_lighting_probe`
and run `build/windows-debug/tools/gpu_probe/flop_scene_lighting_probe.exe assets`
from the repository root. The probe loads the shipped scene and assets, drives
the engine through actual world ticks, and reads back an engine sprite pixel at
idle, under thrust, and after closing throttle. It isolates local lighting by
zeroing sunlight and planet shine in its draw data; this is not a visual scene
comparison. Use `SDL_GPU_DRIVER` as described above to select the backend.

On Windows 10.0.26300, both Direct3D 12 and Vulkan passed on an NVIDIA GeForce
RTX 4050 Laptop GPU (drivers 32.0.15.7703 and 577.3.0.0 respectively). Both
returned RGB `(0, 0, 0)` idle, `(128, 87, 29)` under thrust and `(0, 0, 0)`
after throttle closed. The comparison allows three byte values of tolerance.
The Windows game executable also remained running for four seconds in a hidden
startup smoke test on Direct3D 12; this did not inspect the displayed scene.

For Linux, from the repository root:

```sh
cmake -S . -B build/linux-tests -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=ON -DFLOP_BUILD_GAME=OFF
cmake --build build/linux-tests
ctest --test-dir build/linux-tests --output-on-failure
cmake --preset linux-debug
cmake --build --preset linux-debug --target flop_scene_lighting_probe flop_renderer_probe
SDL_GPU_DRIVER=vulkan build/linux-debug/tools/gpu_probe/flop_renderer_probe
SDL_GPU_DRIVER=vulkan build/linux-debug/tools/gpu_probe/flop_scene_lighting_probe "$PWD/assets"
build/linux-debug/dist/flop
```

The final command opens the interactive scene for visual inspection; hold left
Shift to raise throttle and left Ctrl to lower it. Linux and interactive visual
appearance remain unverified in the Windows probe results above.

# Invalid PNG probe

Build with `cmake --build build/windows-debug --target flop_invalid_png_probe`
and run `build/windows-debug/tools/gpu_probe/flop_invalid_png_probe.exe` to
check texture-cache decoding diagnostics without a GPU.

