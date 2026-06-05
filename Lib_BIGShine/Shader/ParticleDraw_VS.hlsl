#include "ParticleCommon.hlsli"

VS_OUT main(uint id : SV_VertexID)
{
    VS_OUT output;
    output.vertexID = id;
    return output;
}

