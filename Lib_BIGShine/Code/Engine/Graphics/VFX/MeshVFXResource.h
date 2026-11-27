#pragma once

#include <DirectXMath.h>
#include <d3d11.h>
#include <memory>
#include <string>
#include <vector>
#include <wrl.h>

#include "Engine/Graphics/VFXMaterial/VFXMaterialData.h"

class ModelResource;
class GltfModelCommponennt;
class VFXMaterialResource;

// MeshVFX blend mode.
enum class MeshVFXBlendMode
{
    // 通常半透明描画。色とアルファを見た目通り重ねる。
    ALPHA,
    // 加算合成。炎や発光のように明るさを足したいパーツで使う。
    ADDITIVE,
};

// How additional mask textures are combined.
enum class MeshVFXMaskBlendMode
{
    // 既存マスクへ掛け合わせ、重なった部分だけを残す。
    MULTIPLY,
    // 複数マスクを合成し、どれかが有効な領域を残す。
    UNION,
};

// Texture source for edge highlight patterns.
enum class MESH_VFX_EDGE_PATTERN_MODE
{
    // エッジパターンを使わず、色や強度だけで輪郭を作る。
    NONE,
    // ベーステクスチャを輪郭パターンとして再利用する。
    BASE_TEXTURE,
    // マスクテクスチャを輪郭パターンとして使う。
    MASK_TEXTURE,
};

enum class MESH_VFX_GRADIENT_SAMPLE_MODE
{
    HORIZONTAL,
    VERTICAL,
};

enum class MESH_VFX_GRADIENT_DIRECTION
{
    FORWARD,
    REVERSE,
};

enum class MESH_VFX_SAMPLER_MODE
{
    WRAP,
    CLAMP,
    BORDER,
};

enum class TRAIL_PARTICLE_EMISSION_MODE
{
    TIME_INTERVAL,
    DISTANCE_INTERVAL,
    SAMPLE_INTERVAL,
};

// One mask texture layer.
struct MeshVFXMaskLayer
{
    // 複数マスクを重ねる時の1レイヤー分の設定。
    std::string path;
    MeshVFXMaskBlendMode blendMode = MeshVFXMaskBlendMode::MULTIPLY;
    bool invert = false;
};

// Parameters for built-in generated meshes.
struct MeshVFXInternalMeshSettings
{
    // 内蔵生成メッシュの分割数。円弧や円錐の滑らかさに影響する。
    int segments = 96;

    // 円弧・壁・ビームなど、MeshVFXComponent側で生成する形状の細かい寸法設定。
    float startAngleDegrees = -78.0f;
    float endAngleDegrees = 78.0f;
    float taperPower = 2.0f;
    float tipRate = 0.02f;
    float baseRadius = 1.12f;
    float radiusTaperAmount = 0.05f;
    float tipWidth = 0.01f;
    float centerWidth = 0.56f;
    float innerWidthScale = 0.35f;
    float outerWidthScale = 1.25f;
    float wallPositionRate = 0.50f;
    float wallHeight = 1.10f;
    float wallDepth = 0.50f;

    float coneTipRadiusRate = 0.035f;
    float coneLength = 1.0f;

    float capsuleLength = 2.0f;

    float beamLength = 1.0f;
    float beamHalfHeight = 0.45f;
    float beamHalfWidth = 0.12f;
    float beamDepthThickness = 0.018f;
    float beamSidePanelRate = 0.55f;
};

struct MeshVFXPart
{
    // 1つのVFXを構成する描画パーツ。モデル、材質、テクスチャ、時間変化をここへ集約する。
    std::string partAssetPath;
    std::string materialPath;
    std::vector<std::string> materialPaths;
    std::string modelPath;
    int meshIndex = -1;
    bool renderEnabled = false;
    bool useMaterialBaseTexture = true;
    bool useMaterialNormalTexture = true;

    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    // モデルパーツ自体のローカルTransform。OwnerのTransformとはRenderSystem側で合成される。
    DirectX::XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
    float shapeCylinderRadius = 1.0f;
    MeshVFXInternalMeshSettings internalMeshSettings;
    DirectX::XMFLOAT3 meshRotationSpeed = { 0.0f, 0.0f, 0.0f };

    float duration = 0.0f;
    // durationがある場合はパーツ単位の寿命として扱い、loop=falseなら寿命後は描画しない。
    bool loop = true;
    bool animateScale = false;
    float scaleTransitionDuration = 0.0f;
    DirectX::XMFLOAT3 scaleStart = { 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 scaleEnd = { 1.0f, 1.0f, 1.0f };
    bool animateAlpha = false;
    float alphaStart = 1.0f;
    float alphaEnd = 1.0f;

    std::string baseTexturePath;
    // テクスチャは役割ごとに分け、シェーダ側でマスク・ノイズ・歪み・輪郭へ使い分ける。
    std::string normalTexturePath;
    std::string maskTexturePath;
    std::string noiseTexturePath;
    std::string distortionNoiseTexturePath;
    std::string vertexNoiseTexturePath;
    std::string edgePatternTexturePath;
    std::string emissiveTexturePath;
    std::string gradientTexturePath;
    std::vector<MeshVFXMaskLayer> maskLayers;
    std::vector<std::string> noiseTexturePaths;

    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    // UV速度とスケールはテクスチャ種別ごとに持ち、炎や衝撃波の流れを個別に制御する。
    DirectX::XMFLOAT2 baseUVSpeed = { 0.0f, -0.5f };
    DirectX::XMFLOAT2 maskUVSpeed = { 0.0f, -0.2f };
    DirectX::XMFLOAT2 noiseUVSpeed = { 0.2f, -0.8f };
    MESH_VFX_SAMPLER_MODE baseSamplerMode = MESH_VFX_SAMPLER_MODE::WRAP;
    MESH_VFX_SAMPLER_MODE normalSamplerMode = MESH_VFX_SAMPLER_MODE::WRAP;
    MESH_VFX_SAMPLER_MODE maskSamplerMode = MESH_VFX_SAMPLER_MODE::WRAP;
    MESH_VFX_SAMPLER_MODE noiseSamplerMode = MESH_VFX_SAMPLER_MODE::WRAP;
    MESH_VFX_SAMPLER_MODE distortionSamplerMode = MESH_VFX_SAMPLER_MODE::WRAP;
    MESH_VFX_SAMPLER_MODE edgePatternSamplerMode = MESH_VFX_SAMPLER_MODE::WRAP;
    MESH_VFX_SAMPLER_MODE emissiveSamplerMode = MESH_VFX_SAMPLER_MODE::WRAP;
    MESH_VFX_SAMPLER_MODE gradientSamplerMode = MESH_VFX_SAMPLER_MODE::WRAP;
    DirectX::XMFLOAT2 baseUVScale = { 1.0f, 1.0f };
    DirectX::XMFLOAT2 maskUVScale = { 1.0f, 1.0f };
    DirectX::XMFLOAT2 noiseUVScale = { 1.0f, 1.0f };
    DirectX::XMFLOAT2 normalUVScale = { 1.0f, 1.0f };
    DirectX::XMFLOAT2 distortionNoiseUVScale = { 1.0f, 1.0f };
    DirectX::XMFLOAT2 edgePatternUVScale = { 1.0f, 1.0f };
    DirectX::XMFLOAT2 emissiveUVScale = { 1.0f, 1.0f };
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
    // 発光、法線、ノイズ、フレネル、ディゾルブなどの見た目の強度パラメータ。
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
    float edgePatternStrength = 0.0f;
    float fresnelPower = 2.0f;
    float fresnelStrength = 1.0f;
    float dissolveThreshold = 0.0f;
    float gradientFixedUv = 0.5f;
    float gradientStartUv = 0.0f;
    float gradientEndUv = 1.0f;
    float gradientPadding;
    DirectX::XMFLOAT3 gradientRegisterPadding{};

    DirectX::XMFLOAT4 trailOldRootColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    // Trail用の古い点/新しい点の色設定。軌跡の先端と根元で見た目を分ける。
    DirectX::XMFLOAT4 trailOldTipColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 trailNewRootColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 trailNewTipColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    float trailRecencyPower = 1.0f;
    float trailTipPower = 1.0f;
    float trailRecencyIntensity = 1.0f;
    float trailTipIntensity = 1.0f;
    float trailOldAlpha = 1.0f;
    float trailNewAlpha = 1.0f;
    bool useTrailColorRamp = false;

    DirectX::XMFLOAT4 dissolveEdgeColor;
    DirectX::XMFLOAT4 backFaceColor;
    float dissolveEdgeWidth = 0.05f;
    float dissolveEdgeStrength = 3.0f;
    float fresnelNoiseStrength = 0.0f;
    float backFaceEmissiveStrength = 0.0f;
    std::string alphaCurveName;
    // CurveComponentで評価する名前。時間に応じた透明度・スケール・ディゾルブなどを外部カーブで変化させる。
    std::string scaleCurveName;
    std::string dissolveCurveName;
    std::string emissiveCurveName;
    std::string uvScrollCurveName;
    std::string uvRotationCurveName;

    bool useMask = false;
    // 各機能の使用フラグ。無効なものはシェーダ側で軽い経路に寄せる。
    bool useNoise = false;
    bool useVertexNoise = false;
    bool useMaskDistortion = false;
    bool useEdgeHighlight = false;
    bool useNormal = false;
    bool useFresnel = false;
    bool useGradientTexture = false;
    bool useDissolveEdge = false;
    bool useFresnelNoise = false;
    bool useBackFaceBoost = false;
    MESH_VFX_EDGE_PATTERN_MODE edgePatternMode = MESH_VFX_EDGE_PATTERN_MODE::NONE;
    MESH_VFX_GRADIENT_SAMPLE_MODE gradientSampleMode = MESH_VFX_GRADIENT_SAMPLE_MODE::HORIZONTAL;
    MESH_VFX_GRADIENT_DIRECTION gradientDirection = MESH_VFX_GRADIENT_DIRECTION::FORWARD;
    MeshVFXBlendMode blendMode = MeshVFXBlendMode::ALPHA;
    VFX_MATERIAL_CULL_MODE cullMode = VFX_MATERIAL_CULL_MODE::NONE;
};

// Runtime cache generated from MeshVFXPart data.
struct MeshVFXRuntimePart
{
    // MeshVFXPartのパス指定から実際に読み込んだGPUリソースを保持するランタイムキャッシュ。
    std::shared_ptr<ModelResource> modelResource;
    std::shared_ptr<VFXMaterialResource> materialResource;
    std::vector<std::shared_ptr<VFXMaterialResource>> materialResources;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> baseTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalTexture;
    std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> maskTextures;
    std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> noiseTextures;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> distortionNoiseTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> vertexNoiseTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> edgePatternTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> emissiveTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gradientTexture;
};

// Dynamic mesh data owned by a producer component and consumed by MeshVFX rendering.
struct MeshVFXDynamicMesh
{
    ID3D11Buffer* vertexBuffer = nullptr;
    ID3D11Buffer* indexBuffer = nullptr;
    UINT indexCount = 0;
    UINT vertexStride = 0;
};

// Snapshot pose data used by afterimage-style MeshVFX.
struct MeshVFXModelPose
{
    std::shared_ptr<GltfModelCommponennt> model;
    std::vector<std::vector<DirectX::XMFLOAT4X4>> meshTransforms;
    float time = 0.0f;
    float alpha = 1.0f;
};

// Lightweight view into existing pose data.
struct MeshVFXModelPoseView
{
    std::shared_ptr<GltfModelCommponennt> model;
    const std::vector<std::vector<DirectX::XMFLOAT4X4>>* meshTransforms = nullptr;
    float time = 0.0f;
    float alpha = 1.0f;
};

// Full MeshVFX asset data. Multiple parts are treated as one effect.
struct MeshVFXAsset
{
    std::vector<MeshVFXPart> parts;
};
