[[vk::combinedImageSampler]] Texture2D scene : register(t0, space2);
[[vk::combinedImageSampler]] SamplerState scene_sampler : register(s0, space2);

struct VertexOutput {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
};

VertexOutput vertex(uint id : SV_VertexID) {
  float2 uv = float2((id << 1) & 2, id & 2);
  VertexOutput result;
  result.position = float4(uv.x * 2 - 1, 1 - uv.y * 2, 0, 1);
  result.uv = uv;
  return result;
}

float finite_or_zero(float value) {
  return (asuint(value) & 0x7f800000u) == 0x7f800000u ? 0.0 : value;
}

float4 fragment(VertexOutput input) : SV_Target0 {
  float3 color = scene.Sample(scene_sampler, input.uv).rgb;
  return float4(finite_or_zero(color.r), finite_or_zero(color.g), finite_or_zero(color.b), 1);
}
