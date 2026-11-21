#include "ParticleCommon.hlsli"

RWStructuredBuffer<Particle> particleDataBuffer : register(u0);
ConsumeStructuredBuffer<uint> unusedIndexBuffer : register(u1);
StructuredBuffer<EmitParticleData> emitParticleBuffer : register(t0);
RWByteAddressBuffer indirectDataBuffer : register(u2);
RWStructuredBuffer<ParticleHeader> particleHeaderBuffer : register(u3);

[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint emitIndex = dispatchThreadID.x;
    uint particleIndex = unusedIndexBuffer.Consume();

    uint headerIndex = 0;
    indirectDataBuffer.InterlockedAdd(IndirectArgumentsNumEmitParticleIndex, 1, headerIndex);

    EmitParticleData emitData = emitParticleBuffer[emitIndex];

    ParticleHeader header;
    header.alive = 1;
    header.particleIndex = particleIndex;
    header.depth = 0.0f;
    header.dummy = 0;
    particleHeaderBuffer[headerIndex] = header;

    Particle p;
    p.parameter = emitData.parameter;
    p.parameter.z = max(emitData.parameter.y, 0.0001f);
    p.position = emitData.position;
    p.colorRandomRates = emitData.colorRandomRates;
    p.rotation = emitData.rotation;
    p.angularSpeed = emitData.angularSpeed;
    p.scale = emitData.scale;
    p.velocity = emitData.velocity;
    p.acceleration = emitData.acceleration;
    p.texcoord = float4(0, 0, 1, 1);
    p.color = emitData.color;
    particleDataBuffer[particleIndex] = p;
}

