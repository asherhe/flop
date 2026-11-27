#include "../quad_geometry.hlsli"
// geometry and uv conventions match the unlit sprite shader
cbuffer SpriteGeometry : register(b0, space1) {
  float4 origin_viewport;
  float4 xy_axis;
};

cbuffer SpriteLighting : register(b0, space3) {
  float4 sun_direction;
  float4 sun_color;
  float4 planet_shine_direction_and_spread;
  float4 planet_shine_color;
};

[[vk::combinedImageSampler]] Texture2D albedo : register(t0, space2);
[[vk::combinedImageSampler]] SamplerState albedo_sampler : register(s0, space2);
[[vk::combinedImageSampler]] Texture2D normal_map : register(t1, space2);
[[vk::combinedImageSampler]] SamplerState normal_map_sampler : register(s1, space2);
[[vk::combinedImageSampler]] Texture2D emission_map : register(t2, space2);
[[vk::combinedImageSampler]] SamplerState emission_map_sampler : register(s2, space2);
[[vk::combinedImageSampler]] Texture2D local_light : register(t3, space2);
[[vk::combinedImageSampler]] SamplerState local_light_sampler : register(s3, space2);

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
  if (input.position.x < 0.0) return 0.0;
  float4 main = albedo.Sample(albedo_sampler, input.uv);
  float3 normal = normalize(normal_map.Sample(normal_map_sampler, input.uv).xyz * 2.0 - 1.0);
  // set z to 0.3 so that parts are better illuminated
  float3 light_direction = normalize(float3(sun_direction.xy, 0.3));
  // we scale by 2 because that makes the lighting look better
  float3 shaded = main.rgb * sun_color.rgb * 2.0 * saturate(dot(normal, light_direction));
  // planet shine
  float spread = planet_shine_direction_and_spread.w;
  float shine = saturate(spread + (1.0 - spread) * dot(normal.xy, planet_shine_direction_and_spread.xy));
  shaded += main.rgb * planet_shine_color.rgb * shine;
  uint light_width, light_height;
  local_light.GetDimensions(light_width, light_height);
  shaded += main.rgb * local_light.Sample(local_light_sampler, input.position.xy / float2(light_width, light_height)).rgb;
  float4 emission = emission_map.Sample(emission_map_sampler, input.uv);
  // overlay rgba of emission onto shaded
  float alpha = emission.a + main.a * (1.0 - emission.a);
  if (alpha <= 0.0) return 0.0;
  return float4((emission.rgb * emission.a + shaded * main.a * (1.0 - emission.a)) / alpha, alpha);
}
