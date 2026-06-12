#pragma once

#include <DirectXMath.h>
#include <d3d11.h>
#include <memory>
#include <string>
#include <vector>
#include <wrl.h>

class ModelResource;

enum class MeshVFXBlendMode
{
    ALPHA,
    ADDITIVE,
};

struct MeshVFXPart
{
    std::string modelPath;
    int meshIndex = -1;
    bool renderEnabled = false;
    bool useMaterialBaseTexture = true;
    bool useMaterialNormalTexture = true;

    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 meshRotationSpeed = { 0.0f, 0.0f, 0.0f };

    float duration = 0.0f;
    bool loop = true;
    bool animateScale = false;
    DirectX::XMFLOAT3 scaleStart = { 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 scaleEnd = { 1.0f, 1.0f, 1.0f };
    bool animateAlpha = false;
    float alphaStart = 1.0f;
    float alphaEnd = 1.0f;

    std::string baseTexturePath;
    std::string normalTexturePath;
    std::string maskTexturePath;
    std::string noiseTexturePath;

    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT2 baseUVSpeed = { 0.0f, -0.5f };
    DirectX::XMFLOAT2 maskUVSpeed = { 0.0f, -0.2f };
    DirectX::XMFLOAT2 noiseUVSpeed = { 0.2f, -0.8f };
    DirectX::XMFLOAT2 uvRotationCenter = { 0.5f, 0.5f };
    float baseUVRotationSpeed = 0.0f;
    float maskUVRotationSpeed = 0.0f;
    float noiseUVRotationSpeed = 0.0f;

    float alpha = 1.0f;
    float normalStrength = 1.0f;
    float noiseStrength = 0.05f;
    float fresnelPower = 2.0f;
    float fresnelStrength = 1.0f;
    float dissolveThreshold = 0.0f;

    bool useMask = false;
    bool useNoise = false;
    bool useNormal = false;
    bool useFresnel = false;
    MeshVFXBlendMode blendMode = MeshVFXBlendMode::ALPHA;
};

struct MeshVFXRuntimePart
{
    std::shared_ptr<ModelResource> modelResource;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> baseTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> maskTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> noiseTexture;
};

struct MeshVFXAsset
{
    std::vector<MeshVFXPart> parts;
};
