#include "quad_geometry.hlsli"

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
  VertexOutput result;
  result.position = project_quad_vertex(id, origin_viewport, xy_axis);
  result.color = color;
  return result;
}

float4 fragment(VertexOutput input) : SV_Target0 {
  return input.color;
}
