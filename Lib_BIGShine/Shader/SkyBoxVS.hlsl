// SkyBoxVS.hlsl
// スカイボックス用頂点シェーダー
// スプライト (フルスクリーンクワッド) をそのまま使い、
// ピクセルシェーダー側で視線ベクトルを計算するシンプルな構成

#include "Scene.hlsli"  // viewProjection, cameraPosition などが入っている想定

struct VS_IN
{
    float3 position  : POSITION;
    float4 color     : COLOR;
    float2 texcoord  : TEXCOORD;
};

struct VS_OUT
{
    float4 sv_position   : SV_POSITION;
    float3 world_position : TEXCOORD0;   // ワールド空間での位置（視線ベクトル計算用）
    float4 color         : COLOR;
};

VS_OUT main(VS_IN vin)
{
    VS_OUT vout;

    // NDC空間でフルスクリーンクワッドとして出力（深度を1.0にして最背面に描画）
    vout.sv_position = float4(vin.position.xy, 1.0f, 1.0f);

    // NDC座標からビュー空間の方向ベクトルを逆算してワールド空間に変換
    // invViewProjection があれば正確に求められるが、
    // ここでは Scene.hlsli の invViewProjection を利用する
    float4 worldPos = mul(float4(vin.position.xy, 1.0f, 1.0f), invViewProjection);
    worldPos.xyz /= worldPos.w;
    vout.world_position = worldPos.xyz;

    vout.color = vin.color;
    return vout;
}
