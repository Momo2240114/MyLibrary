// SkyBoxPS.hlsl
// スカイボックス用ピクセルシェーダー
// specular_pmrem キューブマップ（IBL用）をそのまま空として描画する

#include "Scene.hlsli"

struct VS_OUT
{
    float4 sv_position    : SV_POSITION;
    float3 world_position : TEXCOORD0;
    float4 color          : COLOR;
};

// IBL鏡面反射キューブマップ（specular_pmrem.dds と同じもの）
// GraphicsDeviceManager 側で t10 にバインドする
TextureCube  skyboxCubeMap : register(t10);
SamplerState linearSampler : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    // カメラ位置から各ピクセルへの視線ベクトルを計算
    float3 eyeDir = normalize(pin.world_position - cameraPosition.xyz);

    // MipMap を使うと壊れるため lod=0 で固定サンプリング
    float3 color = skyboxCubeMap.SampleLevel(linearSampler, eyeDir, 0).rgb;

    // 環境光の明るさを乗算（GraphicsDeviceManager の ambientColor を渡してもよい）
    return float4(color, 1.0f) * pin.color;
}
