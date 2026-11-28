// apply independent display effects in one fullscreen pass
#include "tonemap.hlsli"
#include "vignette.hlsli"

[[vk::combinedImageSampler]] Texture2D scene : register(t0, space2);
[[vk::combinedImageSampler]] SamplerState scene_sampler : register(s0, space2);
[[vk::combinedImageSampler]] Texture2D bloom : register(t1, space2);
[[vk::combinedImageSampler]] SamplerState bloom_sampler : register(s1, space2);
cbuffer DisplayParams : register(b0, space3) {
  float4 display_params; // exposure multiplier, linear white point, vignette intensity and smoothness
  float4 mapping_params; // x: tonemapping enabled
};

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

float4 fragment(VertexOutput input) : SV_Target0 {
  float3 color = scene.Sample(scene_sampler, input.uv).rgb + bloom.Sample(bloom_sampler, input.uv).rgb * 1.04;
  float vignette = vignette_factor(input.uv, display_params.z, display_params.w);
  color = max(color * vignette * display_params.x, 0.0);
  if (mapping_params.x > 0.5) color = filmic(color) / filmic(display_params.y.xxx);
  return float4(saturate(color), 1);
}
