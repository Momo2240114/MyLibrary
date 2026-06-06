#include "ParticleCommon.hlsli"

RWStructuredBuffer<Particle> particleDataBuffer : register(u0);
AppendStructuredBuffer<uint> unusedIndexBuffer : register(u1);
RWByteAddressBuffer indirectDataBuffer : register(u2);
RWStructuredBuffer<ParticleHeader> particleHeaderBuffer : register(u3);

[numthreads(NumParticleThread, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint headerIndex = dispatchThreadID.x;
    if (headerIndex >= systemNumParticles) return;

    ParticleHeader header = particleHeaderBuffer[headerIndex];
    if (header.alive == 0) return;

    uint dataIndex = header.particleIndex;
    Particle data = particleDataBuffer[dataIndex];

    data.parameter.y -= elapsedTime;
    if (data.parameter.y <= 0.0f)
    {
        header.alive = 0;
        particleHeaderBuffer[headerIndex] = header;
        unusedIndexBuffer.Append(dataIndex);

        uint originalDeadCount;
        indirectDataBuffer.InterlockedAdd(IndirectArgumentsNumDeadParticle, 1u, originalDeadCount);
        return;
    }

    data.velocity.xyz += data.acceleration.xyz * elapsedTime;
    data.position.xyz += data.velocity.xyz * elapsedTime;
    data.rotation.z += data.rotation.w * elapsedTime;

    uint type = (uint)(data.parameter.x + 0.5f);
    float w = 1.0f / max(1u, textureSplitCount.x);
    float h = 1.0f / max(1u, textureSplitCount.y);
    data.texcoord.xy = float2((type % textureSplitCount.x) * w, (type / textureSplitCount.x) * h);
    data.texcoord.zw = float2(w, h);

    float lifeRate = saturate(data.parameter.y / max(data.parameter.z, 0.0001f));
    if (useColorOverLife != 0)
    {
        float colorRate = 1.0f - lifeRate;
        float4 lifeColor = lerp(colorStart, colorEnd, colorRate);
        data.color.rgb = lifeColor.rgb;
        data.color.a = lifeColor.a * lifeRate;
    }
    else
    {
        data.color.rgb = manualColor.rgb;
        data.color.a = manualColor.a * lifeRate;
    }

    float4 clip = mul(float4(data.position.xyz, 1.0f), viewProjection);
    header.depth = clip.w;

    particleDataBuffer[dataIndex] = data;
    particleHeaderBuffer[headerIndex] = header;
}


