#pragma once

#include <DirectXMath.h>
#include <d3d11.h>
#include <memory>
#include <string>
#include <vector>
#include <wrl.h>

class ModelResource;
class GltfModelCommponennt;

enum class MeshVFXBlendMode
{
    ALPHA,
    ADDITIVE,
};

enum class MeshVFXMaskBlendMode
{
    MULTIPLY,
    UNION,
};

struct MeshVFXMaskLayer
{
    std::string path;
    MeshVFXMaskBlendMode blendMode = MeshVFXMaskBlendMode::MULTIPLY;
    bool invert = false;
};

struct MeshVFXPart
{
    std::string partAssetPath;
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
    std::string distortionNoiseTexturePath;
    std::string vertexNoiseTexturePath;
    std::vector<MeshVFXMaskLayer> maskLayers;
    std::vector<std::string> noiseTexturePaths;

    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT2 baseUVSpeed = { 0.0f, -0.5f };
    DirectX::XMFLOAT2 maskUVSpeed = { 0.0f, -0.2f };
    DirectX::XMFLOAT2 noiseUVSpeed = { 0.2f, -0.8f };
    DirectX::XMFLOAT2 uvRotationCenter = { 0.5f, 0.5f };
    float baseUVRotation = 0.0f;
    float maskUVRotation = 0.0f;
    float noiseUVRotation = 0.0f;
    float baseUVRotationSpeed = 0.0f;
    float maskUVRotationSpeed = 0.0f;
    float noiseUVRotationSpeed = 0.0f;
    float uvRadialScale = 1.0f;
    float uvRadialScaleSpeed = 0.0f;

    float alpha = 1.0f;
    float emissiveStrength = 3.0f;
    float emissivePulseSpeed = 2.0f;
    float emissivePulseAmount = 0.35f;
    float normalStrength = 1.0f;
    float noiseStrength = 0.05f;
    float vertexNoiseHorizontalStrength = 0.1f;
    float vertexNoiseVerticalStrength = 0.1f;
    float vertexNoiseHorizontalScale = 1.0f;
    float vertexNoiseVerticalScale = 1.0f;
    DirectX::XMFLOAT3 vertexNoiseSpeed = { 0.0f, 1.0f, 0.0f };
    float maskDistortionStrength = 8.0f;
    float maskDistortionOpacity = 0.5f;
    DirectX::XMFLOAT4 edgeColor = { 0.25f, 0.75f, 1.0f, 1.0f };
    float silhouetteEdgeStrength = 0.0f;
    float silhouetteEdgePower = 3.0f;
    float intersectionEdgeStrength = 0.0f;
    float intersectionEdgeWidth = 0.002f;
    float fresnelPower = 2.0f;
    float fresnelStrength = 1.0f;
    float dissolveThreshold = 0.0f;

    bool useMask = false;
    bool useNoise = false;
    bool useVertexNoise = false;
    bool useMaskDistortion = false;
    bool useEdgeHighlight = false;
    bool useNormal = false;
    bool useFresnel = false;
    MeshVFXBlendMode blendMode = MeshVFXBlendMode::ALPHA;
};

struct MeshVFXRuntimePart
{
    std::shared_ptr<ModelResource> modelResource;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> baseTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalTexture;
    std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> maskTextures;
    std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> noiseTextures;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> distortionNoiseTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> vertexNoiseTexture;
};

// Runtime-generated mesh view rendered with MeshVFX settings.
// The component retains ownership of these buffers.
struct MeshVFXDynamicMesh
{
    ID3D11Buffer* vertexBuffer = nullptr;
    ID3D11Buffer* indexBuffer = nullptr;
    UINT indexCount = 0;
    UINT vertexStride = 0;
};

struct MeshVFXModelPose
{
    std::shared_ptr<GltfModelCommponennt> model;
    std::vector<std::vector<DirectX::XMFLOAT4X4>> meshTransforms;
    float time = 0.0f;
    float alpha = 1.0f;
};

struct MeshVFXModelPoseView
{
    std::shared_ptr<GltfModelCommponennt> model;
    const std::vector<std::vector<DirectX::XMFLOAT4X4>>* meshTransforms = nullptr;
    float time = 0.0f;
    float alpha = 1.0f;
};

struct MeshVFXAsset
{
    std::vector<MeshVFXPart> parts;
};
