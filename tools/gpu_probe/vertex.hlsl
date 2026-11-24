struct Output { float4 position : SV_Position; float2 uv : TEXCOORD0; };
Output vertex(uint id : SV_VertexID) {
  const float2 positions[3] = {float2(-1, -1), float2(3, -1), float2(-1, 3)};
  Output o;
  o.position = float4(positions[id], 0, 1);
  o.uv = float2(0.5, 0.5);
  return o;
}
