// =============================================================
// Decal_VS.hlsl
// =============================================================
#include "Decal_Common.hlsli"

struct VS_IN
{
    float4 pos : POSITION;
};

VS_OUT main(VS_IN vin)
{
    VS_OUT vout;

    float4 worldPos = mul(vin.pos, decal_world_matrix); // 箱を動かす
    vout.position = mul(worldPos, viewProjection);

    // PSで頂点カラーを使っているなら初期化
    vout.color = float4(1, 1, 1, 1);

    return vout;
}