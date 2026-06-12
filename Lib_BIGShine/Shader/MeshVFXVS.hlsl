#include "MeshVFX.hlsli"

VS_OUT main(VS_IN input)
{
    VS_OUT output = (VS_OUT)0;
    float4 worldPosition = mul(float4(input.position, 1.0f), world);
    output.position = mul(worldPosition, viewProjection);
    output.worldPosition = worldPosition.xyz;
    output.normal = normalize(mul(float4(input.normal, 0.0f), world).xyz);
    output.tangent = float4(normalize(mul(float4(input.tangent.xyz, 0.0f), world).xyz), input.tangent.w);
    output.texcoord = input.texcoord;
    return output;
}
