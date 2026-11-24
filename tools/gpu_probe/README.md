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
