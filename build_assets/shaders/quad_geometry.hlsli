// Project a quad vertex from drawable pixels.
float4 project_quad_vertex(uint id, float4 origin_viewport, float4 xy_axis) {
  const float2 corners[4] = {
    float2(0, 0), float2(1, 0),
    float2(0, 1), float2(1, 1)
  };
  float2 corner = corners[id];
  float2 pixel = origin_viewport.xy + corner.x * xy_axis.xy + corner.y * xy_axis.zw;
  return float4(pixel.x / origin_viewport.z * 2.0 - 1.0,
                1.0 - pixel.y / origin_viewport.w * 2.0, 0.0, 1.0);
}

struct QuadUvTransform {
  nointerpolation float2 center_pixels : TEXCOORD0;
  nointerpolation float4 gradient : TEXCOORD1;
};

QuadUvTransform quad_uv_transform(float2 origin, float4 xy_axis) {
  float2 x_axis = xy_axis.xy;
  float2 y_axis = xy_axis.zw;
  float determinant = x_axis.x * y_axis.y - x_axis.y * y_axis.x;
  QuadUvTransform result;
  result.center_pixels = origin + (x_axis + y_axis) * 0.5;
  // Invert the pixel-space axes and flip V for sprite texture coordinates.
  result.gradient = float4(y_axis.y, -y_axis.x, x_axis.y, -x_axis.x) / determinant;
  return result;
}

float2 quad_uv_at_pixel(QuadUvTransform transform, float2 pixel) {
  // Both triangles use the same UV calculation, even after viewport clipping.
  float2 delta = pixel - transform.center_pixels;
  return 0.5 + float2(dot(delta, transform.gradient.xy), dot(delta, transform.gradient.zw));
}
