#include "ModelCommon.hlsli"

struct GBUFFER_OUT
{
    float4 baseColorAndModel : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 material : SV_TARGET2;
    float4 emissive : SV_TARGET3;
};

GBUFFER_OUT main(VS_OUT input_)
{
    GBUFFER_OUT output_;

    output_.baseColorAndModel = float4(1.0f, 0.0f, 1.0f, 1.0f);
    output_.normal = float4(normalize(input_.normal) * 0.5f + 0.5f, 1.0f);
    output_.material = float4(1.0f, 0.5f, 0.0f, 1.0f);
    output_.emissive = float4(0.0f, 0.0f, 0.0f, 1.0f);

    return output_;
}
