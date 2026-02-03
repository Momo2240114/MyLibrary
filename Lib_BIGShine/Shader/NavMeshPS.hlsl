

#include"NavMesh.hlsli"

Texture2D texture0 : register(t0);
SamplerState sampler0 : register(s0);

float4 main(VS_OUT pin):SV_TARGET
{
    float4 color = texture0.Sample(sampler0, pin.texcoord) * pin.color;
    color.a = 0.8f;
    return color;
    //return float4(1, 1, 1, 0.1f);
    //return float4(1.0f, 1.0f, 1.0f,1.0f);
}