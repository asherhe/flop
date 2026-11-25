// draw a unit quad through a camera-relative transform in drawable pixels
cbuffer SpriteGeometry : register(b0, space1) {
  float4 origin_and_x_axis; // origin x/y, x-axis x/y
  float4 y_axis_and_viewport; // y-axis x/y, viewport width/height
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
  float2 pixel = origin_and_x_axis.xy + corner.x * origin_and_x_axis.zw + corner.y * y_axis_and_viewport.xy;
  VertexOutput result;
  result.position = float4(pixel.x / y_axis_and_viewport.z * 2.0 - 1.0,
                           1.0 - pixel.y / y_axis_and_viewport.w * 2.0, 0.0, 1.0);
  result.uv = float2(corner.x, 1.0 - corner.y);
  return result;
}

float4 fragment(VertexOutput input) : SV_Target0 {
  // keep position in the DXIL input signature for vertex/fragment linkage
  if (input.position.x < 0.0) return 0.0;
  return albedo.Sample(albedo_sampler, input.uv);
}
