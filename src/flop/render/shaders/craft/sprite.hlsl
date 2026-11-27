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
  const float2 corners[6] = {
      float2(0, 0), float2(1, 0), float2(1, 1),
      float2(0, 0), float2(1, 1), float2(0, 1)};
  float2 corner = corners[id];
  float2 pixel = origin_viewport.xy + corner.x * xy_axis.xy + corner.y * xy_axis.zw;
  VertexOutput result;
  result.position = float4(pixel.x / origin_viewport.z * 2.0 - 1.0,
                           1.0 - pixel.y / origin_viewport.w * 2.0, 0.0, 1.0);
  result.uv = float2(corner.x, 1.0 - corner.y);
  return result;
}

float4 fragment(VertexOutput input) : SV_Target0 {
  // keep position in the DXIL input signature for vertex/fragment linkage
  if (input.position.x < 0.0) return 0.0;
  return albedo.Sample(albedo_sampler, input.uv);
}
