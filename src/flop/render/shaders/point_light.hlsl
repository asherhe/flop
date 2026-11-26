// screen-space point light bounds and parameters, in drawable pixels
cbuffer LightGeometry : register(b0, space1) {
  float4 center_radii; // center xy, inner radius, outer radius
  float4 viewport_falloff; // viewport xy, falloff, reserved
  float4 light_color; // linear rgb, reserved
};

struct VertexOutput {
  float4 position : SV_Position;
  nointerpolation float4 center_radii : TEXCOORD0;
  nointerpolation float falloff : TEXCOORD1;
  nointerpolation float3 color : TEXCOORD2;
};

VertexOutput vertex(uint id : SV_VertexID) {
  const float2 corners[6] = {
      float2(-1, -1), float2(1, -1), float2(1, 1),
      float2(-1, -1), float2(1, 1), float2(-1, 1)};
  float2 pixel = center_radii.xy + corners[id] * center_radii.w;
  VertexOutput result;
  result.position = float4(pixel.x / viewport_falloff.x * 2.0 - 1.0,
                           1.0 - pixel.y / viewport_falloff.y * 2.0, 0.0, 1.0);
  result.center_radii = center_radii;
  result.falloff = viewport_falloff.z;
  result.color = light_color.rgb;
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
  return float4(input.color * strength, 0.0);
}
