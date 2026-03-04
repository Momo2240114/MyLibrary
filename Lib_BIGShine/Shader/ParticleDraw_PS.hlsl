#include "ParticleCommon.Hlsli"

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

struct PS_OUT
{
    float4 color : SV_Target0;
};

float4 main(GS_OUT input) : SV_TARGET
{
    
    float4 texColor = g_texture.Sample(g_sampler, input.tex);
    float4 finalColor = texColor * input.color;


    if (finalColor.a < 0.01)
        discard;
    return finalColor;
}