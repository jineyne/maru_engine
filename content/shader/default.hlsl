cbuffer PerFrame : register(b0)
{
    row_major float4x4 uMVP;
};

struct VSIn {
    float3 pos : POSITION;
    float3 col : COLOR;
    float2 uv  : TEXCOORD0;
};

struct VSOut {
    float4 pos : SV_Position;
    float3 col : COLOR;
    float2 uv  : TEXCOORD0;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.pos = mul(float4(i.pos, 1.0), uMVP);
    o.col = i.col;
    o.uv  = i.uv;
    return o;
}

Texture2D gAlbedo  : register(t0);
SamplerState gSamp : register(s0);

float4 PSMain(VSOut i) : SV_Target {
    float4 tex = gAlbedo.Sample(gSamp, i.uv);
    return tex;
}
