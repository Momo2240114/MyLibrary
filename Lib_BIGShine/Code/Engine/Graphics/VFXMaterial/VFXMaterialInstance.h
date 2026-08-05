#pragma once

#include <DirectXMath.h>

struct VFXMaterialInstance
{
    // インスタンス単位で透明度を上書きする倍率。
    float alphaMultiplier = 1.0f;

    // 発光強度の倍率。
    float emissiveMultiplier = 1.0f;

    // Dissolveしきい値へ加算する値。
    float dissolveAdd = 0.0f;

    // Main UVスクロール速度への倍率と加算。
    DirectX::XMFLOAT2 mainUVScrollMultiplier = { 1.0f, 1.0f };
    DirectX::XMFLOAT2 mainUVScrollAdd = { 0.0f, 0.0f };
    float mainUVRotationMultiplier = 1.0f;
    float mainUVRotationAdd = 0.0f;

    // Main UVの最終オフセットへ加算する値。
    DirectX::XMFLOAT2 mainUVOffsetAdd = { 0.0f, 0.0f };

    // Noise UVスクロール速度への倍率と加算。
    DirectX::XMFLOAT2 noiseUVScrollMultiplier = { 1.0f, 1.0f };
    DirectX::XMFLOAT2 noiseUVScrollAdd = { 0.0f, 0.0f };
    float noiseUVRotationMultiplier = 1.0f;
    float noiseUVRotationAdd = 0.0f;
    DirectX::XMFLOAT2 noiseUVOffsetAdd = { 0.0f, 0.0f };

    // Materialの再生時間を外部から指定する場合の時間。
    float animationTime = 0.0f;

    // 個体ごとにMaterial時間をずらすためのオフセット。
    float timeOffset = 0.0f;
};
