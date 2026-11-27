#include "common.hlsli"
[[vk::combinedImageSampler]] Texture2D source_texture : register(t0, space2);
[[vk::combinedImageSampler]] SamplerState source_sampler : register(s0, space2);
[[vk::combinedImageSampler]] Texture2D weights : register(t1, space2);
[[vk::combinedImageSampler]] SamplerState weights_sampler : register(s1, space2);
float4 fragment(SmaaVertex input) : SV_Target0 {
  float4 offset;
  SMAANeighborhoodBlendingVS(input.uv, offset);
  return SMAANeighborhoodBlendingPS(input.uv, offset, source_texture, source_sampler, weights, weights_sampler);
}
