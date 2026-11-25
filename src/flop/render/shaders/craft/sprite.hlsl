// Draw one sprite from six procedural vertices. Coordinates are drawable pixels;
// the CPU has already projected the world position relative to the camera.
cbuffer SpriteGeometry : register(b0, space1) {
  float4 center_and_size_pixels; // center x/y, width, height
  float4 viewport_and_rotation;   // width, height, cos(angle), sin(angle)
};

[[vk::combinedImageSampler]] Texture2D albedo : register(t0, space2);
[[vk::combinedImageSampler]] SamplerState albedo_sampler : register(s0, space2);

struct VertexOutput {
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
};

VertexOutput vertex(uint id : SV_VertexID) {
  const float2 corners[6] = {
      float2(-0.5, -0.5), float2(0.5, -0.5), float2(0.5, 0.5),
      float2(-0.5, -0.5), float2(0.5, 0.5), float2(-0.5, 0.5)};
  float2 local = corners[id] * center_and_size_pixels.zw;
  float c = viewport_and_rotation.z, s = viewport_and_rotation.w;
  float2 pixel = center_and_size_pixels.xy + float2(c * local.x - s * local.y, s * local.x + c * local.y);
  VertexOutput result;
  result.position = float4(pixel.x / viewport_and_rotation.x * 2.0 - 1.0,
                           1.0 - pixel.y / viewport_and_rotation.y * 2.0, 0.0, 1.0);
  result.uv = corners[id] + 0.5;
  return result;
}

float4 fragment(VertexOutput input) : SV_Target0 {
  // Keep SV_Position in the DXIL input signature for vertex/fragment linkage.
  if (input.position.x < 0.0) return 0.0;
  return albedo.Sample(albedo_sampler, input.uv);
}
