#pragma once

#include <DirectXMath.h>

struct VFXMaterialInstance
{
    float alphaMultiplier = 1.0f;
    float emissiveMultiplier = 1.0f;
    float dissolveAdd = 0.0f;
    DirectX::XMFLOAT2 mainUVScrollMultiplier = { 1.0f, 1.0f };
    DirectX::XMFLOAT2 mainUVScrollAdd = { 0.0f, 0.0f };
    float mainUVRotationMultiplier = 1.0f;
    float mainUVRotationAdd = 0.0f;
    DirectX::XMFLOAT2 mainUVOffsetAdd = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 noiseUVScrollMultiplier = { 1.0f, 1.0f };
    DirectX::XMFLOAT2 noiseUVScrollAdd = { 0.0f, 0.0f };
    float noiseUVRotationMultiplier = 1.0f;
    float noiseUVRotationAdd = 0.0f;
    DirectX::XMFLOAT2 noiseUVOffsetAdd = { 0.0f, 0.0f };
    float animationTime = 0.0f;
    float timeOffset = 0.0f;
};
