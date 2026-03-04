#include "ParticleCommon.hlsli"

// 入力(id)はシステム値 SV_VertexID
VS_OUT main(uint id : SV_VertexID)
{
    VS_OUT output;
    output.vertexID = id; // ここでカスタムセマンティクス(TEXCOORD0)にコピーされる
    return output;
}