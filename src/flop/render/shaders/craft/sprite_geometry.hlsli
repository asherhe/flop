// shared geometry and uv projection for sprite color and normal passes
void project_sprite_vertex(uint id, float4 origin_viewport, float4 xy_axis,
                           out float4 position, out float2 uv) {
  const float2 corners[6] = {
      float2(0, 0), float2(1, 0), float2(1, 1),
      float2(0, 0), float2(1, 1), float2(0, 1)};
  float2 corner = corners[id];
  float2 pixel = origin_viewport.xy + corner.x * xy_axis.xy + corner.y * xy_axis.zw;
  position = float4(pixel.x / origin_viewport.z * 2.0 - 1.0,
                    1.0 - pixel.y / origin_viewport.w * 2.0, 0.0, 1.0);
  uv = float2(corner.x, 1.0 - corner.y);
}
