#pragma once

#include "VFXMaterialData.h"

class VFXMaterialColorUtility
{
public:
    static DirectX::XMFLOAT4 SampleColorRamp(
        const VFXMaterialColorRampData& colorRamp_,
        float lifeTime_,
        float time_);
    static DirectX::XMFLOAT4 SampleRandomColor(
        const VFXMaterialRandomColorData& randomColor_,
        uint32_t seed_);
    static DirectX::XMFLOAT4 ApplyColorRamp(
        const DirectX::XMFLOAT4& baseColor_,
        const VFXMaterialColorRampData& colorRamp_,
        const DirectX::XMFLOAT4& rampColor_);
    static DirectX::XMFLOAT4 ApplyRandomColor(
        const DirectX::XMFLOAT4& baseColor_,
        const VFXMaterialRandomColorData& randomColorData_,
        const DirectX::XMFLOAT4& randomColor_);

private:
    static DirectX::XMFLOAT4 BlendColor(
        const DirectX::XMFLOAT4& baseColor_,
        const DirectX::XMFLOAT4& blendColor_,
        VFX_MATERIAL_COLOR_RAMP_BLEND_MODE blendMode_,
        float influence_,
        bool affectAlpha_);
};
