#define FLOP_SMAA_EDGE
#include "common.hlsli"
[[vk::combinedImageSampler]] Texture2D source_texture : register(t0, space2);
[[vk::combinedImageSampler]] SamplerState source_sampler : register(s0, space2);
float4 fragment(SmaaVertex input) : SV_Target0 {
  float4 offsets[3];
  SMAAEdgeDetectionVS(input.uv, offsets);
  return float4(SMAAColorEdgeDetectionPS(input.uv, offsets, source_texture, source_sampler), 0, 0);
}
