#include "PrecompiledHeader.h"

#include "VFXMaterialColorUtility.h"

#include "Engine/Debug/TracyProfiler.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
    float Clamp01(float value_)
    {
        return std::clamp(value_, 0.0f, 1.0f);
    }

    float WrapRampPosition(
        float position_,
        VFX_MATERIAL_COLOR_RAMP_WRAP_MODE wrapMode_)
    {
        switch (wrapMode_)
        {
        case VFX_MATERIAL_COLOR_RAMP_WRAP_MODE::REPEAT:
            position_ = std::fmod(position_, 1.0f);
            return position_ < 0.0f ? position_ + 1.0f : position_;
        case VFX_MATERIAL_COLOR_RAMP_WRAP_MODE::MIRROR:
        {
            position_ = std::fmod(position_, 2.0f);
            if (position_ < 0.0f) position_ += 2.0f;
            return position_ > 1.0f ? 2.0f - position_ : position_;
        }
        default:
            return Clamp01(position_);
        }
    }

    float SmoothStep(float value_)
    {
        value_ = Clamp01(value_);
        return value_ * value_ * (3.0f - 2.0f * value_);
    }

    DirectX::XMFLOAT4 LerpColor(
        const DirectX::XMFLOAT4& startColor_,
        const DirectX::XMFLOAT4& endColor_,
        float amount_)
    {
        return
        {
            startColor_.x + (endColor_.x - startColor_.x) * amount_,
            startColor_.y + (endColor_.y - startColor_.y) * amount_,
            startColor_.z + (endColor_.z - startColor_.z) * amount_,
            startColor_.w + (endColor_.w - startColor_.w) * amount_,
        };
    }

    uint32_t HashColorSeed(uint32_t seed_)
    {
        seed_ ^= seed_ >> 16;
        seed_ *= 0x7feb352du;
        seed_ ^= seed_ >> 15;
        seed_ *= 0x846ca68bu;
        seed_ ^= seed_ >> 16;
        return seed_;
    }

    float Random01(uint32_t& seed_)
    {
        seed_ = HashColorSeed(seed_ + 0x9e3779b9u);
        return static_cast<float>(seed_ & 0x00FFFFFFu) / static_cast<float>(0x00FFFFFFu);
    }

    float SelectRampBasePosition(
        const VFXMaterialColorRampData& colorRamp_,
        float lifeTime_,
        float time_)
    {
        switch (colorRamp_.sampleSource)
        {
        case VFX_MATERIAL_COLOR_RAMP_SAMPLE_SOURCE::TIME:
            return time_ * colorRamp_.scrollSpeed;
        case VFX_MATERIAL_COLOR_RAMP_SAMPLE_SOURCE::FIXED:
            return colorRamp_.fixedPosition + time_ * colorRamp_.scrollSpeed;
        default:
            return lifeTime_ + time_ * colorRamp_.scrollSpeed;
        }
    }
}

DirectX::XMFLOAT4 VFXMaterialColorUtility::SampleColorRamp(
    const VFXMaterialColorRampData& colorRamp_,
    float lifeTime_,
    float time_)
{
    BS_PROFILE_SCOPE("VFXMaterialColorUtility::SampleColorRamp");

    if (colorRamp_.stops.empty())
    {
        return { 1.0f, 1.0f, 1.0f, 1.0f };
    }

    const float position_ = WrapRampPosition(
        SelectRampBasePosition(colorRamp_, lifeTime_, time_),
        colorRamp_.wrapMode);

    auto stops_ = colorRamp_.stops;
    std::sort(stops_.begin(), stops_.end(),
        [](const VFXMaterialColorRampStopData& left_, const VFXMaterialColorRampStopData& right_)
        {
            return left_.position < right_.position;
        });

    if (position_ <= stops_.front().position) return stops_.front().color;
    if (position_ >= stops_.back().position) return stops_.back().color;

    for (size_t stopIndex_ = 1u; stopIndex_ < stops_.size(); ++stopIndex_)
    {
        const VFXMaterialColorRampStopData& nextStop_ = stops_[stopIndex_];
        if (position_ > nextStop_.position) continue;

        const VFXMaterialColorRampStopData& previousStop_ = stops_[stopIndex_ - 1u];
        const float range_ = (std::max)(nextStop_.position - previousStop_.position, 0.0001f);
        float amount_ = Clamp01((position_ - previousStop_.position) / range_);
        if (colorRamp_.interpolation == VFX_MATERIAL_COLOR_RAMP_INTERPOLATION::CONSTANT)
        {
            return previousStop_.color;
        }
        if (colorRamp_.interpolation == VFX_MATERIAL_COLOR_RAMP_INTERPOLATION::SMOOTH)
        {
            amount_ = SmoothStep(amount_);
        }
        return LerpColor(previousStop_.color, nextStop_.color, amount_);
    }

    return stops_.back().color;
}

DirectX::XMFLOAT4 VFXMaterialColorUtility::SampleRandomColor(
    const VFXMaterialRandomColorData& randomColor_,
    uint32_t seed_)
{
    BS_PROFILE_SCOPE("VFXMaterialColorUtility::SampleRandomColor");

    seed_ ^= randomColor_.seed;
    const float red_ = Random01(seed_);
    const float green_ = Random01(seed_);
    const float blue_ = Random01(seed_);
    const float alpha_ = Random01(seed_);

    return
    {
        randomColor_.minColor.x + (randomColor_.maxColor.x - randomColor_.minColor.x) * red_,
        randomColor_.minColor.y + (randomColor_.maxColor.y - randomColor_.minColor.y) * green_,
        randomColor_.minColor.z + (randomColor_.maxColor.z - randomColor_.minColor.z) * blue_,
        randomColor_.minColor.w + (randomColor_.maxColor.w - randomColor_.minColor.w) * alpha_,
    };
}

DirectX::XMFLOAT4 VFXMaterialColorUtility::ApplyColorRamp(
    const DirectX::XMFLOAT4& baseColor_,
    const VFXMaterialColorRampData& colorRamp_,
    const DirectX::XMFLOAT4& rampColor_)
{
    BS_PROFILE_SCOPE("VFXMaterialColorUtility::ApplyColorRamp");

    if (!colorRamp_.enabled) return baseColor_;
    return BlendColor(
        baseColor_,
        rampColor_,
        colorRamp_.blendMode,
        colorRamp_.influence,
        colorRamp_.affectAlpha);
}

DirectX::XMFLOAT4 VFXMaterialColorUtility::ApplyRandomColor(
    const DirectX::XMFLOAT4& baseColor_,
    const VFXMaterialRandomColorData& randomColorData_,
    const DirectX::XMFLOAT4& randomColor_)
{
    BS_PROFILE_SCOPE("VFXMaterialColorUtility::ApplyRandomColor");

    if (!randomColorData_.enabled) return baseColor_;
    const VFX_MATERIAL_COLOR_RAMP_BLEND_MODE blendMode_ =
        randomColorData_.mode == VFX_MATERIAL_RANDOM_COLOR_MODE::OVERRIDE
            ? VFX_MATERIAL_COLOR_RAMP_BLEND_MODE::OVERRIDE
            : (randomColorData_.mode == VFX_MATERIAL_RANDOM_COLOR_MODE::ADD
                ? VFX_MATERIAL_COLOR_RAMP_BLEND_MODE::ADD
                : VFX_MATERIAL_COLOR_RAMP_BLEND_MODE::MULTIPLY);
    return BlendColor(
        baseColor_,
        randomColor_,
        blendMode_,
        randomColorData_.influence,
        randomColorData_.affectAlpha);
}

DirectX::XMFLOAT4 VFXMaterialColorUtility::BlendColor(
    const DirectX::XMFLOAT4& baseColor_,
    const DirectX::XMFLOAT4& blendColor_,
    VFX_MATERIAL_COLOR_RAMP_BLEND_MODE blendMode_,
    float influence_,
    bool affectAlpha_)
{
    BS_PROFILE_SCOPE("VFXMaterialColorUtility::BlendColor");

    const float influenceClamped_ = Clamp01(influence_);
    DirectX::XMFLOAT4 targetColor_ = baseColor_;
    switch (blendMode_)
    {
    case VFX_MATERIAL_COLOR_RAMP_BLEND_MODE::OVERRIDE:
        targetColor_ = blendColor_;
        break;
    case VFX_MATERIAL_COLOR_RAMP_BLEND_MODE::ADD:
        targetColor_ =
        {
            baseColor_.x + blendColor_.x,
            baseColor_.y + blendColor_.y,
            baseColor_.z + blendColor_.z,
            affectAlpha_ ? baseColor_.w + blendColor_.w : baseColor_.w,
        };
        break;
    default:
        targetColor_ =
        {
            baseColor_.x * blendColor_.x,
            baseColor_.y * blendColor_.y,
            baseColor_.z * blendColor_.z,
            affectAlpha_ ? baseColor_.w * blendColor_.w : baseColor_.w,
        };
        break;
    }

    if (!affectAlpha_) targetColor_.w = baseColor_.w;
    return LerpColor(baseColor_, targetColor_, influenceClamped_);
}
