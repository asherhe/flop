# SPIR-V reflection output (prototype)

`flop_shader_reflect <shader.spirv>` writes one JSON object to standard output.
Errors go to standard error with a nonzero exit code. This version is diagnostic:
it is not consumed by the shader manifest and does not prove a shader works on
SDL, Vulkan, Direct3D or Metal.

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
| `name` | string | Nonempty reflected sampled-image name. |
| `binding` | integer | Sampled-image slot. |

The tool checks the expected stage-specific descriptor sets, consecutiveness
within each resource class, matching texture/sampler binding numbers, and some
unsupported resource types. It rejects arrays, matrices, and unsupported scalar
types. The result does not include texture dimensionality, sampler names,
material semantics or backend-specific layouts. In particular, separate SPIR-V
image and sampler declarations still require SDL/Vulkan compatibility validation;
matching slot numbers alone do not establish it. Optimized-out resources may be
absent. Do not apply these offsets or bindings to DXIL or Metal without independent
validation. This prototype contract may change as reflection is integrated.
