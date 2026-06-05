#include "ParticleCommon.hlsli"

RWByteAddressBuffer indirectDataBuffer : register(u2);

[numthreads(1, 1, 1)]
void main()
{
    uint deadCount = indirectDataBuffer.Load(IndirectArgumentsNumDeadParticle);
    indirectDataBuffer.Store(IndirectArgumentsNumDeadParticle, 0);

    uint currentNumParticle = indirectDataBuffer.Load(IndirectArgumentsNumCurrentParticle);
    uint liveCount = currentNumParticle >= deadCount ? currentNumParticle - deadCount : 0;
    indirectDataBuffer.Store(IndirectArgumentsNumCurrentParticle, liveCount);
    indirectDataBuffer.Store(IndirectArgumentsDrawIndirect, liveCount);
}

