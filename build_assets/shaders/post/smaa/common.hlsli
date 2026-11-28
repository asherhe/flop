// adapt the upstream texture abstraction to SDL's paired texture/sampler bindings
cbuffer SmaaParams : register(b0, space3) {
  float4 smaa_metrics; // inverse dimensions xy, dimensions zw
};
#define SMAA_PRESET_MEDIUM
#define SMAA_CUSTOM_SL
#define SMAA_RT_METRICS smaa_metrics
#define SMAATexture2D(tex) Texture2D tex, SamplerState tex##_sampler
#define SMAATexturePass2D(tex) tex, tex##_sampler
#define SMAASampleLevelZero(tex, coord) tex.SampleLevel(tex##_sampler, coord, 0)
#define SMAASampleLevelZeroPoint(tex, coord) tex.SampleLevel(tex##_sampler, coord, 0)
#define SMAASampleLevelZeroOffset(tex, coord, offset) tex.SampleLevel(tex##_sampler, coord, 0, offset)
#define SMAASample(tex, coord) tex.Sample(tex##_sampler, coord)
#ifdef FLOP_SMAA_EDGE
float4 smaa_edge_color(float4 color) {
  float3 c = max(color.rgb, 0.0);
  color.rgb = lerp(12.92 * c, 1.055 * pow(c, 1.0 / 2.4) - 0.055, step(0.0031308, c));
  return color;
}
#define SMAASamplePoint(tex, coord) smaa_edge_color(tex.Sample(tex##_sampler, coord))
#else
#define SMAASamplePoint(tex, coord) tex.Sample(tex##_sampler, coord)
#endif
#define SMAASampleOffset(tex, coord, offset) tex.Sample(tex##_sampler, coord, offset)
#define SMAA_FLATTEN [flatten]
#define SMAA_BRANCH [branch]
#include "vendor/smaa.hlsli"

struct SmaaVertex { float4 position : SV_Position; float2 uv : TEXCOORD0; };
SmaaVertex vertex(uint id : SV_VertexID) {
  float2 uv = float2((id << 1) & 2, id & 2);
  SmaaVertex result;
  result.position = float4(uv.x * 2 - 1, 1 - uv.y * 2, 0, 1);
  result.uv = uv;
  return result;
}
