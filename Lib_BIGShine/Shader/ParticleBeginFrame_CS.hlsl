#include "ParticleCommon.hlsli"

RWByteAddressBuffer indirectDataBuffer : register(u2);

[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint previousNumParticle = indirectDataBuffer.Load(IndirectArgumentsNumCurrentParticle);

    uint availableCount = systemNumParticles > previousNumParticle
        ? systemNumParticles - previousNumParticle
        : 0;

    uint emitCount = min(totalEmitCount, availableCount);
    uint currentNumParticle = previousNumParticle + emitCount;

    indirectDataBuffer.Store(IndirectArgumentsNumCurrentParticle, currentNumParticle);
    indirectDataBuffer.Store(IndirectArgumentsNumPreviousParticle, previousNumParticle);
    indirectDataBuffer.Store(IndirectArgumentsNumDeadParticle, 0);

    uint3 emitDispatch;
    emitDispatch.x = emitCount;
    emitDispatch.y = 1;
    emitDispatch.z = 1;
    indirectDataBuffer.Store3(IndirectArgumentsEmitParticleDispatchIndirect, emitDispatch);

    indirectDataBuffer.Store(IndirectArgumentsNumEmitParticleIndex, previousNumParticle);

    uint3 updateDispatch;
    updateDispatch.x = (currentNumParticle + (NumParticleThread - 1)) / NumParticleThread;
    updateDispatch.y = 1;
    updateDispatch.z = 1;
    indirectDataBuffer.Store3(IndirectArgumentsUpdateParticleDispatchIndirect, updateDispatch);
}