#include "quad_geometry.hlsli"

// screen-space point light bounds and parameters, in drawable pixels
cbuffer LightGeometry : register(b0, space1) {
  float4 center_radii; // center xy, inner radius, outer radius
  float4 viewport_falloff; // viewport xy, falloff, z offset in pixels
  float4 light_color; // linear rgb, reserved
};

[[vk::combinedImageSampler]] Texture2D normal_texture : register(t0, space2);
[[vk::combinedImageSampler]] SamplerState normal_sampler : register(s0, space2);

struct VertexOutput {
  float4 position : SV_Position;
  nointerpolation float4 center_radii : TEXCOORD0;
  nointerpolation float falloff : TEXCOORD1;
  nointerpolation float3 color : TEXCOORD2;
  nointerpolation float z_offset : TEXCOORD3;
};

VertexOutput vertex(uint id : SV_VertexID) {

  float r = center_radii.w;
  float4 origin_viewport = float4(center_radii.xy - r, viewport_falloff.xy);
  float4 xy_axis = float4(2.0 * r, 0.0, 0.0, 2.0 * r);
  VertexOutput result;
  result.position = project_quad_vertex(id, origin_viewport, xy_axis);

  result.center_radii = center_radii;
  result.falloff = viewport_falloff.z;
  result.color = light_color.rgb;
  result.z_offset = viewport_falloff.w;
  return result;
}

float4 fragment(VertexOutput input) : SV_Target0 {
  float distance_to_light = length(input.position.xy - input.center_radii.xy);
  float inner_radius = input.center_radii.z;
  float outer_radius = input.center_radii.w;
  float strength = 0.0;
  if (distance_to_light < outer_radius) {
    if (distance_to_light <= inner_radius) strength = 1.0;
    else {
      float t = (distance_to_light - inner_radius) / (outer_radius - inner_radius);
      float u = t + input.falloff * t * (1.0 - t);
      strength = 1.0 - u * u * (3.0 - 2.0 * u);
    }
  }
  uint width, height;
  normal_texture.GetDimensions(width, height);
  float3 normal = normal_texture.Sample(normal_sampler, input.position.xy / float2(width, height)).xyz * 2.0 - 1.0;
  normal = dot(normal, normal) > 1e-12 ? normalize(normal) : float3(0, 0, 1);
  float2 delta = input.center_radii.xy - input.position.xy;
  float3 direction = float3(delta.x, -delta.y, input.z_offset);
  direction = dot(direction, direction) > 1e-12 ? normalize(direction) : float3(0, 0, 1);
  return float4(input.color * strength * saturate(dot(normal, direction)), 0.0);
}
