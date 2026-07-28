#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <string>

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
    VFXMaterialRenderStateData renderState;
};
