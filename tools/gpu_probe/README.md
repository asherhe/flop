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
opaque, half-transparent and transparent inputs. It also checks the source
context in an invalid-PNG error. The offscreen capture uses the selected
swapchain format and normal renderer draw code; it does not test presentation,
resize, or multi-pixel UV orientation. The craft shader must be packaged for
the selected backend.

# Invalid PNG probe

Build with `cmake --build build/windows-debug --target flop_invalid_png_probe`
and run `build/windows-debug/tools/gpu_probe/flop_invalid_png_probe.exe` to
check texture-cache decoding diagnostics without a GPU.

