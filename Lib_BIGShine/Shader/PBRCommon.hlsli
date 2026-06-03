#include "Scene.hlsli"
// PBRCommon.hlsli

struct VS_OUT
{
    float4 vertex : SV_POSITION;
    float2 texcoord : TEXCOORD;
    float3 normal : NORMAL;
    float3 position : POSITION;
    float3 tangent : TANGENT;
};


// -----------------------------------------------
// MRT 出力
// -----------------------------------------------
struct PS_OUT
{
    float4 color : SV_TARGET0; // シーンカラー
    float4 normal : SV_TARGET1; // 法線バッファ
    float depth : SV_TARGET2;
};
