#include "sprite_geometry.hlsli"
cbuffer NormalGeometry : register(b0, space1) {
  float4 origin_viewport;
  float4 xy_axis;
  float4 rotation;
};

[[vk::combinedImageSampler]] Texture2D albedo : register(t0, space2);
[[vk::combinedImageSampler]] SamplerState albedo_sampler : register(s0, space2);
[[vk::combinedImageSampler]] Texture2D normal_map : register(t1, space2);
[[vk::combinedImageSampler]] SamplerState normal_map_sampler : register(s1, space2);

struct VertexOutput {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
  nointerpolation float2 rotation : TEXCOORD1;
};

VertexOutput vertex(uint id : SV_VertexID) {
  VertexOutput result;
  project_sprite_vertex(id, origin_viewport, xy_axis, result.position, result.uv);
  result.rotation = rotation.xy;
  return result;
}

float4 fragment(VertexOutput input) : SV_Target0 {
  float alpha = albedo.Sample(albedo_sampler, input.uv).a;
  float3 n = normalize(normal_map.Sample(normal_map_sampler, input.uv).xyz * 2.0 - 1.0);
  float c = input.rotation.x, s = input.rotation.y;
  n.xy = float2(c * n.x - s * n.y, s * n.x + c * n.y);
  return float4(n * 0.5 + 0.5, alpha);
}
