// black vignette with a centered, unrounded falloff
float vignette_factor(float2 uv, float intensity, float smoothness) {
  float2 distance = abs(uv - 0.5) * (intensity * 3.0);
  return pow(saturate(1.0 - dot(distance, distance)), smoothness * 5.0);
}
