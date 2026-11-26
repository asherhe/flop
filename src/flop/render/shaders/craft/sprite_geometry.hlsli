// shared geometry and uv projection for sprite color and normal passes
void project_sprite_vertex(uint id, float4 origin_and_x_axis, float4 y_axis_and_viewport,
                           out float4 position, out float2 uv) {
  const float2 corners[6] = {
      float2(0, 0), float2(1, 0), float2(1, 1),
      float2(0, 0), float2(1, 1), float2(0, 1)};
  float2 corner = corners[id];
  float2 pixel = origin_and_x_axis.xy + corner.x * origin_and_x_axis.zw + corner.y * y_axis_and_viewport.xy;
  position = float4(pixel.x / y_axis_and_viewport.z * 2.0 - 1.0,
                    1.0 - pixel.y / y_axis_and_viewport.w * 2.0, 0.0, 1.0);
  uv = float2(corner.x, 1.0 - corner.y);
}
