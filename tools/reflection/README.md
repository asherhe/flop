# Shader reflection tools

The reflection tools inspect **compiled shader stages** and describe their
resources and uniform layouts as JSON. `flop_spirv_reflect` reads SPIR-V using
SPIRV-Cross; `flop_dxil_reflect` reads DXIL using DXC's native reflection API.
They run on the build host and are not shipped as part of the game.

Both write one JSON object to standard output. Errors go to standard error and
produce a nonzero exit code. The formats below are prototypes: successful
reflection does not by itself prove that a shader renders correctly.

## Build and run manually

Configure a game-enabled preset, then build the desired tools. For example, on
Windows:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug --target flop_spirv_reflect flop_dxil_reflect
```

Invoke the executables with an already compiled shader stage:

```powershell
build/windows-debug/flop_spirv_reflect.exe path/to/shader.spirv
build/windows-debug/flop_dxil_reflect.exe path/to/dxcompiler.dll path/to/shader.dxil
```

The DLL must be the `dxcompiler.dll` from the pinned DXC package used by the
build; it is under `bin/x64/` within that package. FetchContent normally places
the package under the build directory's `_deps/flop_dxc-src/`; the CMake cache
variable `FETCHCONTENT_SOURCE_DIR_FLOP_DXC` can override that location. The normal game build
also produces compiled stages under `build/windows-debug/generated/shaders/`.

On Linux x64, use the `linux-debug` preset and `flop_spirv_reflect` without an
`.exe` suffix. The DXIL reflector currently builds on Windows only. Metal
reflection is not implemented.

The CMake shader pipeline runs SPIR-V reflection automatically. On Windows it
also runs DXIL reflection, compares the supported resource layouts, and embeds
each variant's metadata in the shader manifest. Linux DXIL packaging currently
retains the explicit `RESOURCE_FREE` fallback.

## JSON schema

Both formats share the uniform-member records below but have different
backend-specific resource fields. All listed fields are required for the
relevant format; arrays may be empty. Numbers are nonnegative integers. Object
and array ordering is not a contract. Names come from the compiled binary and
may differ between formats or from source-level names.

| Field | SPIR-V | DXIL | Meaning |
| --- | --- | --- | --- |
| `version` | yes | yes | Integer format version, currently `1`. |
| `stage` | yes | yes | `vertex` or `fragment`. |
| `binding_validation` | yes | no | Always `partial`; not proof of backend compatibility. |
| `entrypoint` | yes | no | Nonempty SPIR-V entry point name. |
| `uniform_buffers` | yes | yes | Array of uniform buffer records. |
| `textures` | yes | no | Array of combined image-sampler records. |
| `resources` | no | yes | Array of Direct3D resource binding records. |

### Uniform buffers and members

| Buffer field | Type | Meaning |
| --- | --- | --- |
| `name` | string | Nonempty reflected buffer name. |
| `binding` | integer | Uniform slot; present in SPIR-V records only. DXIL bindings are in `resources`. |
| `size` | integer | Declared buffer size in bytes for this backend. |
| `members` | array | Uniform member records. |

| Member field | Type | Meaning |
| --- | --- | --- |
| `name` | string | Nonempty reflected member name. |
| `type` | string | `float`, `int`, or `uint`. |
| `components` | integer | Scalar or vector component count, 1–4. |
| `offset` | integer | Member offset in bytes within the buffer. |
| `size` | integer | Declared member size in bytes. |

### Backend-specific resources

| SPIR-V texture field | Type | Meaning |
| --- | --- | --- |
| `name` | string | Nonempty combined image-sampler name. |
| `binding` | integer | Combined image-sampler slot. |

| DXIL resource field | Type | Meaning |
| --- | --- | --- |
| `name` | string | Nonempty resource name. |
| `type` | string | `uniform_buffer`, `texture`, or `sampler`. |
| `space` | integer | Direct3D register space. |
| `binding` | integer | Register index within that resource type and space. |

For a fragment stage, examples of the two outputs are:

```json
{"version":1,"binding_validation":"partial","stage":"fragment","entrypoint":"fragment",
 "uniform_buffers":[{"name":"type.Material","binding":0,"size":16,
   "members":[{"name":"tint","type":"float","components":4,"offset":0,"size":16}]}],
 "textures":[{"name":"albedo","binding":0}]}
```

```json
{"version":1,"stage":"fragment",
 "resources":[{"name":"Material","type":"uniform_buffer","space":3,"binding":0},
   {"name":"albedo_sampler","type":"sampler","space":2,"binding":0},
   {"name":"albedo","type":"texture","space":2,"binding":0}],
 "uniform_buffers":[{"name":"Material","size":16,
   "members":[{"name":"tint","type":"float","components":4,"offset":0,"size":16}]}]}
```

## Current limits

The tools support 32-bit scalar and vector uniforms and non-arrayed,
non-multisampled 2D float textures. Arrays, matrices, storage resources,
comparison samplers, and other unsupported types are rejected. The SPIR-V
tool checks stage-specific descriptor sets and consecutive slots; separate
image and sampler resources are rejected in favor of combined bindings.
The DXIL tool checks stage-specific register spaces. Cross-format comparison
checks the supported bindings and uniform members but is not a GPU test.

The output does not describe material semantics, texture color space, sampling
policy, or every backend-specific property. Optimized-out resources may be
absent. Do not apply one backend's offsets or bindings to another without
validation. The format may change as reflection support expands.
