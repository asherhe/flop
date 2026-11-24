cbuffer Material : register(b1, space3) { float4 tint; };
float4 fragment() : SV_Target0 { return tint; }
