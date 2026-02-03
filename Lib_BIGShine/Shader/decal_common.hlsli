#ifndef __DECAL_COMMON_HLSLI__
#define __DECAL_COMMON_HLSLI__

#include "Scene.hlsli"

// デカール個別定数バッファ (b10)
cbuffer GBUFFER_DECAL_CONSTANT_BUFFER : register(b10)
{
    row_major float4x4 decal_world_matrix; // VS用: ボリュームボックスの配置
    row_major float4x4 decal_inverse_transform; // PS用: 世界座標をデカール空間へ
    float4 decal_direction; // 投影方向（法線判定用）
    float4 decalBaseColor; // デカールの色・不透明度
};

// 必要なリソースのみに絞る
// t0: シーンのベースカラー（必要に応じて合成に使用。不要なら削除OK）
Texture2D<float4> scene_color_texture : register(t0);
// t4: 深度バッファ（これを使って座標を復元する）
Texture2D<float> scene_depth_texture : register(t4);
// t2: 法線を取得
Texture2D<float4> scene_normal_texture : register(t2);
// t10: デカールとして貼り付けるテクスチャ
Texture2D<float4> decal_texture : register(t10);
// サンプラー (s0: PointClamp for Depth, s1: LinearBorder for Decal)
SamplerState sampler_states[2] : register(s0);


// VS -> PS 構造体
struct VS_OUT
{
    float4 position : SV_POSITION; // クリップ空間座標
    float4 screenPos : TEXCOORD0; // 座標復元用のスクリーン位置
    float4 color : COLOR; // 頂点カラー
};
#endif