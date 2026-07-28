#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <string>
#include <vector>

enum class VFX_MATERIAL_BLEND_MODE
{
    ALPHA,
    ADDITIVE
};

enum class VFX_MATERIAL_CULL_MODE
{
    NONE,
    BACK,
    FRONT
};

struct VFXMaterialUVData
{
    DirectX::XMFLOAT2 scale = { 1.0f, 1.0f };
    DirectX::XMFLOAT2 offset = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 scrollSpeed = { 0.0f, 0.0f };
    float rotation = 0.0f;
    float rotationSpeed = 0.0f;
    DirectX::XMFLOAT2 pivot = { 0.5f, 0.5f };
    float padding = 0.0f;
};

struct VFXMaterialDissolveData
{
    bool enabled = false;
    float threshold = 0.0f;
    float edgeWidth = 0.0f;
    DirectX::XMFLOAT4 edgeColor = { 1.0f, 1.0f, 1.0f, 1.0f };
};

struct VFXMaterialDistortionData
{
    bool enabled = false;
    float strength = 0.0f;
};

enum class VFX_MATERIAL_COLOR_RAMP_SAMPLE_SOURCE
{
    LIFE,
    TIME,
    FIXED
};

enum class VFX_MATERIAL_COLOR_RAMP_BLEND_MODE
{
    MULTIPLY,
    OVERRIDE,
    ADD
};

enum class VFX_MATERIAL_COLOR_RAMP_INTERPOLATION
{
    LINEAR,
    CONSTANT,
    SMOOTH
};

enum class VFX_MATERIAL_COLOR_RAMP_WRAP_MODE
{
    CLAMP,
    REPEAT,
    MIRROR
};

enum class VFX_MATERIAL_RANDOM_COLOR_MODE
{
    MULTIPLY,
    OVERRIDE,
    ADD
};

struct VFXMaterialColorRampStopData
{
    float position = 0.0f;
    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
};

struct VFXMaterialColorRampData
{
    bool enabled = false;
    VFX_MATERIAL_COLOR_RAMP_SAMPLE_SOURCE sampleSource = VFX_MATERIAL_COLOR_RAMP_SAMPLE_SOURCE::LIFE;
    VFX_MATERIAL_COLOR_RAMP_BLEND_MODE blendMode = VFX_MATERIAL_COLOR_RAMP_BLEND_MODE::MULTIPLY;
    VFX_MATERIAL_COLOR_RAMP_INTERPOLATION interpolation = VFX_MATERIAL_COLOR_RAMP_INTERPOLATION::LINEAR;
    VFX_MATERIAL_COLOR_RAMP_WRAP_MODE wrapMode = VFX_MATERIAL_COLOR_RAMP_WRAP_MODE::CLAMP;
    float fixedPosition = 0.0f;
    float scrollSpeed = 0.0f;
    float influence = 1.0f;
    bool affectAlpha = true;
    std::vector<VFXMaterialColorRampStopData> stops =
    {
        { 0.0f, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { 1.0f, { 1.0f, 1.0f, 1.0f, 1.0f } },
    };
};

struct VFXMaterialRandomColorData
{
    bool enabled = false;
    VFX_MATERIAL_RANDOM_COLOR_MODE mode = VFX_MATERIAL_RANDOM_COLOR_MODE::MULTIPLY;
    DirectX::XMFLOAT4 minColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 maxColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    float influence = 1.0f;
    bool affectAlpha = false;
    uint32_t seed = 0u;
};

struct VFXMaterialRenderStateData
{
    VFX_MATERIAL_BLEND_MODE blendMode = VFX_MATERIAL_BLEND_MODE::ALPHA;
    VFX_MATERIAL_CULL_MODE cullMode = VFX_MATERIAL_CULL_MODE::NONE;
    bool depthTest = true;
    bool depthWrite = false;
};

struct VFXMaterialData
{
    uint32_t version = 1u;
    std::string materialName = "VFXMaterial";
    std::string baseTexturePath;
    std::string maskTexturePath;
    std::string noiseTexturePath;
    std::string normalTexturePath;
    std::string distortionTexturePath;
    DirectX::XMFLOAT4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 emissiveColor = { 1.0f, 1.0f, 1.0f };
    float emissiveIntensity = 0.0f;
    float alpha = 1.0f;
    VFXMaterialUVData mainUV;
    VFXMaterialUVData noiseUV;
    VFXMaterialDissolveData dissolve;
    VFXMaterialDistortionData distortion;
    VFXMaterialColorRampData colorRamp;
    VFXMaterialRandomColorData randomColor;
    VFXMaterialRenderStateData renderState;
};
