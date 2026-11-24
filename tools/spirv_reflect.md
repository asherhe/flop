# SPIR-V reflection output (prototype)

On Windows, `flop_dxil_reflect <path-to-dxcompiler.dll> <shader.dxil>` is a
separate diagnostic prototype. It emits JSON containing DXIL resource bindings
and constant-buffer member layouts using DXC's native reflection API. Its output
is not the SPIR-V JSON contract below. Windows builds reflect each DXIL stage,
compare the supported resource layouts with SPIR-V, and embed the DXIL metadata
with its variant. Linux DXIL reflection remains unimplemented; Linux builds
still require explicit `RESOURCE_FREE` declarations for DXIL packaging.

`flop_spirv_reflect <shader.spirv>` writes one JSON object to standard output.
Errors go to standard error with a nonzero exit code. The build embeds this
metadata with SPIR-V shader variants and uses its resource counts. Validation
is partial and does not prove a shader works on SDL, Vulkan, Direct3D or Metal.

Example:

```json
{
  "version": 1,
  "binding_validation": "partial",
  "stage": "fragment",
  "entrypoint": "fragment",
  "uniform_buffers": [{
    "name": "type.Material",
    "binding": 0,
    "size": 16,
    "members": [{"name": "tint", "type": "float", "components": 4, "offset": 0, "size": 16}]
  }],
  "textures": [{"name": "albedo", "binding": 0}]
}
```

All fields below are required. Numbers are nonnegative integers; JSON object
and array ordering is not a contract. Names come from compiled SPIR-V and may
differ from source-level names.

| Field | Type | Meaning |
| --- | --- | --- |
| `version` | integer | Output format version; currently `1`. |
| `binding_validation` | string | Always `partial` in this prototype. |
| `stage` | string | `vertex` or `fragment`. |
| `entrypoint` | string | Nonempty SPIR-V entry point name. |
| `uniform_buffers` | array | Uniform buffer records; may be empty. |
| `textures` | array | Sampled-image records; may be empty. |

Each `uniform_buffers` record has these fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `name` | string | Nonempty reflected buffer name. |
| `binding` | integer | Uniform buffer slot. |
| `size` | integer | Declared buffer size in bytes. |
| `members` | array | Uniform member records; may be empty. |

Each uniform member has these fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `name` | string | Nonempty reflected member name. |
| `type` | string | Scalar type: `float`, `int`, or `uint`. |
| `components` | integer | Scalar or vector component count, from 1 to 4. |
| `offset` | integer | Member offset in bytes within its buffer. |
| `size` | integer | Declared member size in bytes. |

Each `textures` record has these fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `name` | string | Nonempty reflected combined image-sampler name. |
| `binding` | integer | Combined image-sampler slot. |

The tool checks the expected stage-specific descriptor sets, consecutiveness
within each resource class, combined image-sampler bindings, and some
unsupported resource types. It rejects arrays, matrices, and unsupported scalar
types. The result does not include texture dimensionality, sampler names,
material semantics or backend-specific layouts. Separate SPIR-V image and sampler
resources are rejected: annotate HLSL texture and sampler declarations with
`[[vk::combinedImageSampler]]` and use matching register indices and spaces.
The combined form still requires an SDL/Vulkan GPU test. Optimized-out resources may be
absent. Do not apply these offsets or bindings to DXIL or Metal without independent
validation. This prototype contract may change as reflection is integrated.
