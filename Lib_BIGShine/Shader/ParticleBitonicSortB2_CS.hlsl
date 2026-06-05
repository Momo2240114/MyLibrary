#include "ParticleBitonicSort.hlsli"

[numthreads(BitonicSortB2Thread, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint t = dispatchThreadID.x;
    uint low = t & (increment - 1);
    uint i = (t << 1) - low;
    bool reverse = ((direction & i) == 0);

    ParticleHeader x0 = particleHeaderBuffer[i];
    ParticleHeader x1 = particleHeaderBuffer[i + increment];

    if (reverse ^ ParticleHeaderComparer(x0, x1))
    {
        ParticleHeader temp = x0;
        x0 = x1;
        x1 = temp;
    }

    particleHeaderBuffer[i] = x0;
    particleHeaderBuffer[i + increment] = x1;
}

