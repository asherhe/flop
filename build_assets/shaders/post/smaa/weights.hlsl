#include "common.hlsli"
[[vk::combinedImageSampler]] Texture2D edges : register(t0, space2);
[[vk::combinedImageSampler]] SamplerState edges_sampler : register(s0, space2);
[[vk::combinedImageSampler]] Texture2D area : register(t1, space2);
[[vk::combinedImageSampler]] SamplerState area_sampler : register(s1, space2);
[[vk::combinedImageSampler]] Texture2D search : register(t2, space2);
[[vk::combinedImageSampler]] SamplerState search_sampler : register(s2, space2);
float4 fragment(SmaaVertex input) : SV_Target0 {
  float2 pixcoord;
  float4 offsets[3];
  SMAABlendingWeightCalculationVS(input.uv, pixcoord, offsets);
  return SMAABlendingWeightCalculationPS(input.uv, pixcoord, offsets,
      edges, edges_sampler, area, area_sampler, search, search_sampler, 0);
}
