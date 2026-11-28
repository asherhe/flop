#include "../quad_geometry.hlsli"

// draw a unit quad through a camera-relative transform in drawable pixels
cbuffer SpriteGeometry : register(b0, space1) {
  float4 origin_viewport; // origin x/y, viewport width/height
  float4 xy_axis; // x-axis x/y, y-axis x/y
};

[[vk::combinedImageSampler]] Texture2D albedo : register(t0, space2);
[[vk::combinedImageSampler]] SamplerState albedo_sampler : register(s0, space2);

struct VertexOutput {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
};

VertexOutput vertex(uint id : SV_VertexID) {
  VertexOutput result;
  project_quad_vertex(id, origin_viewport, xy_axis, result.position, result.uv);
  return result;
}

float4 fragment(VertexOutput input) : SV_Target0 {
  // keep position in the DXIL input signature for vertex/fragment linkage
  if (input.position.x < 0.0) return 0.0;
  return albedo.Sample(albedo_sampler, input.uv);
}
