#include "color_filter.hlsli" 
 
VS_OUT main(uint vID : SV_VertexID)
{
    VS_OUT vout;

    // 0,1,2,3 の頂点番号から全画面の四角形（Triangle Strip）のUVを作る
    // 0:(0,0), 1:(1,0), 2:(0,1), 3:(1,1)
    float2 uv = float2((vID << 1) & 2, vID & 2);

    // UVをスクリーン座標系（-1 ～ 1）に変換
    // Zは0.0、Wは1.0に固定
    vout.position = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);

    // ピクセルシェーダーに渡す値
    vout.color = float4(1.0f, 1.0f, 1.0f, 1.0f); // 基本色（白）
    vout.texcoord = uv;

    return vout;
}