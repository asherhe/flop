// Hable filmic curve evaluated on linear scene rgb
float3 filmic(float3 x) {
  const float a = 0.15, b = 0.50, c = 0.10, d = 0.20, e = 0.02, f = 0.30;
  return ((x * (a * x + c * b) + d * e) / (x * (a * x + b) + d * f)) - e / f;
}
