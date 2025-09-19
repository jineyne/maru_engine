Texture2D gTex : register(t0);
SamplerState gSamp : register(s0);

struct VSOut{
	float4 pos : SV_Position;
	float2 uv : TEXCOORD0;
};

VSOut VSMain(uint vid : SV_VertexID){
	VSOut o;
	float2 pos = (vid == 0) ? float2(-1,-1) : (vid == 1) ? float2( 3,-1) : float2(-1, 3);
	float2 uv  = (vid == 0) ? float2( 0, 0) : (vid == 1) ? float2( 2, 0) : float2( 0, 2);
	o.pos = float4(pos,0,1);
	o.uv = float2(uv.x, 1.0 - uv.y);
	return o;
}

float4 PSMain(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target{
    return gTex.Sample(gSamp, uv); 
}