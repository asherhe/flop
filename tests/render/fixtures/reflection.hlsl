cbuffer Material : register(b0, space3) {
  float4 tint;
};
Texture2D albedo : register(t0, space2);
SamplerState albedo_sampler : register(s0, space2);

float4 fragment(float2 uv : TEXCOORD0) : SV_Target0 {
  return albedo.Sample(albedo_sampler, uv) * tint;
}
