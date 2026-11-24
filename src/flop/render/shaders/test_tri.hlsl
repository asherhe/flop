struct VertexOutput {
  float4 position : SV_Position;
  float3 color : TEXCOORD0;
};

VertexOutput vertex_main(uint id : SV_VertexID) {
  const float2 positions[3] = {float2(0.0, 0.6), float2(0.6, -0.6), float2(-0.6, -0.6)};
  const float3 colors[3] = {float3(1, 0, 0), float3(0, 1, 0), float3(0, 0, 1)};
  VertexOutput result;
  result.position = float4(positions[id], 0, 1);
  result.color = colors[id];
  return result;
}

float4 fragment_main(VertexOutput input) : SV_Target0 {
  return float4(input.color, 1);
}
