#pragma once

#include "VFXMaterialData.h"

class VFXMaterialColorUtility
{
public:
    // ColorRamp設定から、寿命率や時間に応じた色をサンプリングする。
    static DirectX::XMFLOAT4 SampleColorRamp(
        const VFXMaterialColorRampData& colorRamp_,
            float lifeTime_,
            float time_);

    // seedから決まるランダム色をmin/max範囲内で作る。
    static DirectX::XMFLOAT4 SampleRandomColor(
            const VFXMaterialRandomColorData& randomColor_,
            uint32_t seed_);

    // baseColorへRamp色を、設定されたBlendMode/Influenceで合成する。
    static DirectX::XMFLOAT4 ApplyColorRamp(
        const DirectX::XMFLOAT4& baseColor_,
            const VFXMaterialColorRampData& colorRamp_,
            const DirectX::XMFLOAT4& rampColor_);

    // baseColorへランダム色を、RandomColorDataのModeに従って合成する。
    static DirectX::XMFLOAT4 ApplyRandomColor(
        const DirectX::XMFLOAT4& baseColor_,
        const VFXMaterialRandomColorData& randomColorData_,
        const DirectX::XMFLOAT4& randomColor_);

private:
    // Override/Add/Multiplyを共通処理し、必要ならAlphaも合成対象にする。
    static DirectX::XMFLOAT4 BlendColor(
        const DirectX::XMFLOAT4& baseColor_,
        const DirectX::XMFLOAT4& blendColor_,
        VFX_MATERIAL_COLOR_RAMP_BLEND_MODE blendMode_,
        float influence_,
        bool affectAlpha_);
};
