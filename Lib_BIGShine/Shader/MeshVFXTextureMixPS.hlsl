Texture2D CurrentTexture : register(t0);
Texture2D LayerTexture : register(t1);
SamplerState LinearClampSampler : register(s0);

cbuffer MixConstants : register(b0)
{
    uint blendMode;
    uint invertLayer;
    float blendWeight;
    float padding;
};

struct PS_IN
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(PS_IN input) : SV_TARGET
{
    float4 currentValue = CurrentTexture.Sample(LinearClampSampler, input.uv);
    float4 layerValue = LayerTexture.Sample(LinearClampSampler, input.uv);
    if (invertLayer != 0)
    {
        layerValue = 1.0f - layerValue;
    }

    if (blendMode == 1)
    {
        return max(currentValue, layerValue);
    }
    if (blendMode == 2)
    {
        return lerp(currentValue, layerValue, blendWeight);
    }
    return currentValue * layerValue;
}
