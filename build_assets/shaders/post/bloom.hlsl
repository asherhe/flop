// Linear RGB input: the HDR scene for prefiltering, or the higher-resolution
// pyramid level for downsampling and upsampling. Both samplers use linear clamp.
[[vk::combinedImageSampler]] Texture2D source_texture : register(t0, space2);
[[vk::combinedImageSampler]] SamplerState source_sampler : register(s0, space2);
// Previously combined lower-resolution level during upsampling; bound but not
// used by the prefilter and downsample modes.
[[vk::combinedImageSampler]] Texture2D low_texture : register(t1, space2);
[[vk::combinedImageSampler]] SamplerState low_sampler : register(s1, space2);
cbuffer BloomParams : register(b0, space3) {
  // x: mode (0 = prefilter, 1 = downsample, 2 = upsample and combine)
  // y: linear brightness threshold; brightness is the maximum RGB channel
  // z: soft-knee width in linear brightness units
  // w: scatter, the blend weight of the lower-resolution level on upsampling
  float4 params;
};
struct VertexOutput { float4 position : SV_Position; float2 uv : TEXCOORD0; };
VertexOutput vertex(uint id : SV_VertexID) {
  float2 uv = float2((id << 1) & 2, id & 2);
  VertexOutput result;
  result.position = float4(uv.x * 2 - 1, 1 - uv.y * 2, 0, 1);
  result.uv = uv;
  return result;
}
float4 fragment(VertexOutput input) : SV_Target0 {
  if (params.x > 1.5) {
    uint width, height;
    low_texture.GetDimensions(width, height);
    float2 pixel = 1.0 / float2(width, height);
    // A normalized 3x3 tent reconstructs the lower level smoothly.
    float3 low = low_texture.Sample(low_sampler, input.uv).rgb * 4.0;
    low += low_texture.Sample(low_sampler, input.uv + pixel * float2(-1, 0)).rgb * 2.0;
    low += low_texture.Sample(low_sampler, input.uv + pixel * float2(1, 0)).rgb * 2.0;
    low += low_texture.Sample(low_sampler, input.uv + pixel * float2(0, -1)).rgb * 2.0;
    low += low_texture.Sample(low_sampler, input.uv + pixel * float2(0, 1)).rgb * 2.0;
    low += low_texture.Sample(low_sampler, input.uv + pixel * float2(-1, -1)).rgb;
    low += low_texture.Sample(low_sampler, input.uv + pixel * float2(1, -1)).rgb;
    low += low_texture.Sample(low_sampler, input.uv + pixel * float2(-1, 1)).rgb;
    low += low_texture.Sample(low_sampler, input.uv + pixel * float2(1, 1)).rgb;
    low *= 1.0 / 16.0;
    float3 high = source_texture.Sample(source_sampler, input.uv).rgb;
    return float4(lerp(high, low, params.w), 1);
  }

  uint width, height;
  source_texture.GetDimensions(width, height);
  float2 pixel = 1.0 / float2(width, height);
  // The same normalized tent filters before reducing the source resolution.
  float3 color = source_texture.Sample(source_sampler, input.uv).rgb * 4.0;
  color += source_texture.Sample(source_sampler, input.uv + pixel * float2(-1, 0)).rgb * 2.0;
  color += source_texture.Sample(source_sampler, input.uv + pixel * float2(1, 0)).rgb * 2.0;
  color += source_texture.Sample(source_sampler, input.uv + pixel * float2(0, -1)).rgb * 2.0;
  color += source_texture.Sample(source_sampler, input.uv + pixel * float2(0, 1)).rgb * 2.0;
  color += source_texture.Sample(source_sampler, input.uv + pixel * float2(-1, -1)).rgb;
  color += source_texture.Sample(source_sampler, input.uv + pixel * float2(1, -1)).rgb;
  color += source_texture.Sample(source_sampler, input.uv + pixel * float2(-1, 1)).rgb;
  color += source_texture.Sample(source_sampler, input.uv + pixel * float2(1, 1)).rgb;
  color *= 1.0 / 16.0;
  if (params.x < 0.5) {
    color = min(max(color, 0), 65472.0);
    float brightness = max(color.r, max(color.g, color.b));
    float soft = clamp(brightness - params.y + params.z, 0.0, 2.0 * params.z);
    soft = soft * soft / max(4.0 * params.z, 1e-5);
    color *= max(soft, brightness - params.y) / max(brightness, 1e-5);
  }
  return float4(color, 1);
}
