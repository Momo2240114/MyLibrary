#include "MeshVFX.hlsli"

VS_OUT main(VS_IN input)
{
    VS_OUT output = (VS_OUT)0;
    float4 worldPosition = mul(float4(input.position, 1.0f), world);
    output.position = mul(worldPosition, viewProjection);
    output.worldPosition = worldPosition.xyz;
    output.normal = normalize(mul(float4(input.normal, 0.0f), world).xyz);
    output.texcoord = input.texcoord;
    return output;
}
