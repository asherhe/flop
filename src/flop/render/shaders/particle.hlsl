// draw an untextured particle quad in drawable pixels
cbuffer ParticleGeometry : register(b0, space1) {
  float4 origin_and_x_axis; // origin x/y, x-axis x/y
  float4 y_axis_and_viewport; // y-axis x/y, viewport width/height
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
  float2 pixel = origin_and_x_axis.xy + corners[id].x * origin_and_x_axis.zw +
                 corners[id].y * y_axis_and_viewport.xy;
  VertexOutput result;
  result.position = float4(pixel.x / y_axis_and_viewport.z * 2.0 - 1.0,
                           1.0 - pixel.y / y_axis_and_viewport.w * 2.0, 0.0, 1.0);
  result.color = color;
  return result;
}

float4 fragment(VertexOutput input) : SV_Target0 {
  return input.color;
}
