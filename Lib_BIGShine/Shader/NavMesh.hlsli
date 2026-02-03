
struct VS_OUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 texcoord : TEXCOORD;
};

struct VS_IN
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD;
    int color : COLOR;
};
    
cbuffer ConstantBuffert : register(b0)
{
    row_major float4x4 wvp;
}