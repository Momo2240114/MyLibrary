#include "MeshVFX.hlsli"

Texture2D VertexNoiseTexture : register(t0);
SamplerState VertexNoiseSampler : register(s0);

float Hash(float3 position)
{
    position = frac(position * 0.3183099f + 0.1f);
    position *= 17.0f;
    return frac(position.x * position.y * position.z * (position.x + position.y + position.z));
}

float ValueNoise(float3 position)
{
    float3 cell = floor(position);
    float3 localPosition = frac(position);
    localPosition = localPosition * localPosition * (3.0f - 2.0f * localPosition);

    return lerp(
        lerp(
            lerp(Hash(cell + float3(0, 0, 0)), Hash(cell + float3(1, 0, 0)), localPosition.x),
            lerp(Hash(cell + float3(0, 1, 0)), Hash(cell + float3(1, 1, 0)), localPosition.x),
            localPosition.y),
        lerp(
            lerp(Hash(cell + float3(0, 0, 1)), Hash(cell + float3(1, 0, 1)), localPosition.x),
            lerp(Hash(cell + float3(0, 1, 1)), Hash(cell + float3(1, 1, 1)), localPosition.x),
            localPosition.y),
        localPosition.z);
}

VS_OUT main(VS_IN input)
{
    VS_OUT output = (VS_OUT)0;
    float3 localPosition = input.position;
    if (useVertexNoise != 0)
    {
        float3 noisePosition = float3(
            input.position.x * vertexNoiseHorizontalScale,
            input.position.y * vertexNoiseVerticalScale,
            input.position.z * vertexNoiseHorizontalScale) + vertexNoiseSpeed * time;
        float horizontalX = ValueNoise(noisePosition + float3(17.0f, 3.0f, 11.0f)) * 2.0f - 1.0f;
        float horizontalZ = ValueNoise(noisePosition + float3(5.0f, 19.0f, 23.0f)) * 2.0f - 1.0f;
        float verticalY = ValueNoise(noisePosition + float3(29.0f, 7.0f, 13.0f)) * 2.0f - 1.0f;
        if (hasVertexNoiseTexture != 0)
        {
            horizontalX = VertexNoiseTexture.SampleLevel(
                VertexNoiseSampler, noisePosition.xz + float2(0.17f, 0.31f), 0.0f).r * 2.0f - 1.0f;
            horizontalZ = VertexNoiseTexture.SampleLevel(
                VertexNoiseSampler, noisePosition.zy + float2(0.43f, 0.59f), 0.0f).r * 2.0f - 1.0f;
            verticalY = VertexNoiseTexture.SampleLevel(
                VertexNoiseSampler, noisePosition.xy + float2(0.71f, 0.83f), 0.0f).r * 2.0f - 1.0f;
        }
        localPosition += float3(
            horizontalX * vertexNoiseHorizontalStrength,
            verticalY * vertexNoiseVerticalStrength,
            horizontalZ * vertexNoiseHorizontalStrength);
    }

    float4 worldPosition = mul(float4(localPosition, 1.0f), world);
    output.position = mul(worldPosition, viewProjection);
    output.worldPosition = worldPosition.xyz;
    output.normal = normalize(mul(float4(input.normal, 0.0f), world).xyz);
    output.tangent = float4(normalize(mul(float4(input.tangent.xyz, 0.0f), world).xyz), input.tangent.w);
    output.texcoord = input.texcoord;
    output.instanceTime = 0.0f;
    output.instanceAlpha = 1.0f;
    return output;
}
