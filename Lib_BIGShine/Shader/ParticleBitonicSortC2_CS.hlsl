#include "ParticleBitonicSort.hlsli"

groupshared ParticleHeader sharedData[BitonicSortC2Thread * 2];

[numthreads(BitonicSortC2Thread, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int t = dispatchThreadID.x;
    int wgBits = 2 * BitonicSortC2Thread - 1;

    for (int inc = increment; inc > 0; inc >>= 1)
    {
        int low = t & (inc - 1);
        int i = (t << 1) - low;
        bool reverse = ((direction & i) == 0);
        ParticleHeader x0, x1;

        if (inc == (int)increment)
        {
            x0 = particleHeaderBuffer[i];
            x1 = particleHeaderBuffer[i + inc];
        }
        else
        {
            GroupMemoryBarrierWithGroupSync();
            x0 = sharedData[i & wgBits];
            x1 = sharedData[(i + inc) & wgBits];
        }

        if (reverse ^ ParticleHeaderComparer(x0, x1))
        {
            ParticleHeader temp = x0;
            x0 = x1;
            x1 = temp;
        }

        if (inc == 1)
        {
            particleHeaderBuffer[i] = x0;
            particleHeaderBuffer[i + inc] = x1;
        }
        else
        {
            GroupMemoryBarrierWithGroupSync();
            sharedData[i & wgBits] = x0;
            sharedData[(i + inc) & wgBits] = x1;
        }
    }
}

