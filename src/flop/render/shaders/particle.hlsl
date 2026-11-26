// draw an untextured particle quad in drawable pixels
cbuffer ParticleGeometry : register(b0, space1) {
  float4 rect; // origin x/y, width/height
  float4 viewport_and_padding;
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
  float2 pixel = rect.xy + corners[id] * rect.zw;
  VertexOutput result;
  result.position = float4(pixel.x / viewport_and_padding.x * 2.0 - 1.0,
                           1.0 - pixel.y / viewport_and_padding.y * 2.0, 0.0, 1.0);
  result.color = color;
  return result;
}

float4 fragment(VertexOutput input) : SV_Target0 {
  return input.color;
}
