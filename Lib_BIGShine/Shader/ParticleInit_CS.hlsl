#include "ParticleCommon.hlsli"

RWStructuredBuffer<Particle> particleDataBuffer : register(u0);
AppendStructuredBuffer<uint> unusedIndexBuffer : register(u1);
RWStructuredBuffer<ParticleHeader> particleHeaderBuffer : register(u3);

[numthreads(NumParticleThread, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint index = dispatchThreadID.x;
    if (index >= systemNumParticles) return;

    Particle p = (Particle)0;
    p.scale = float4(1, 1, 1, 0);
    p.color = float4(1, 1, 1, 1);
    p.texcoord = float4(0, 0, 1, 1);
    particleDataBuffer[index] = p;

    ParticleHeader header;
    header.alive = 0;
    header.particleIndex = index;
    header.depth = 0.0f;
    header.dummy = 0;
    particleHeaderBuffer[index] = header;

    unusedIndexBuffer.Append(index);
}

