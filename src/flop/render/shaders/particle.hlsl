// draw an untextured particle quad in drawable pixels
cbuffer ParticleGeometry : register(b0, space1) {
  float4 origin_viewport; // origin x/y, viewport width/height
  float4 xy_axis; // x-axis x/y, y-axis x/y
  float4 color;
};

struct VertexOutput {
  float4 position : SV_Position;
  float4 color : COLOR0;
};

VertexOutput vertex(uint id : SV_VertexID) {
  const float2 corners[6] = {
      float2(0, 0), float2(1, 0), float2(1, 1),
      float2(0, 0), float2(1, 1), float2(0, 1)};
  float2 pixel = origin_viewport.xy + corners[id].x * xy_axis.xy +
                 corners[id].y * xy_axis.zw;
  VertexOutput result;
  result.position = float4(pixel.x / origin_viewport.z * 2.0 - 1.0,
                           1.0 - pixel.y / origin_viewport.w * 2.0, 0.0, 1.0);
  result.color = color;
  return result;
}

float4 fragment(VertexOutput input) : SV_Target0 {
  return input.color;
}
