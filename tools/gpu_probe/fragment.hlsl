cbuffer Material : register(b0, space3) {
  float4 tint;
};
[[vk::combinedImageSampler]] Texture2D albedo : register(t0, space2);
[[vk::combinedImageSampler]] SamplerState albedo_sampler : register(s0, space2);

float4 fragment(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target0 {
  if (position.x < 0) return 0;
  return albedo.Sample(albedo_sampler, uv) * tint;
}
