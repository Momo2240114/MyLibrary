#pragma once

#include "MeshVFXResource.h"
#include "Engine/Graphics/Resource/GLTFModel.h"

#include <DirectXMath.h>
#include <cstdint>
#include <d3d11.h>
#include <vector>
#include <wrl.h>

struct RenderContext;

// MeshVFX用のShaderとConstantBuffer更新をまとめるクラス
// RenderSystemは描画するPartごとにUpdatePartを呼び、このクラスがHLSLへ値を渡す
class MeshVFXShader
{
public:
    bool Initialize(ID3D11Device* device_);

    void Begin(
        const RenderContext& renderContext_,
        ID3D11ShaderResourceView* sceneColor_,
        ID3D11ShaderResourceView* sceneDepth_,
        bool useMaterialOverlay_,
        bool useInstancedPose_ = false);

    void BeginDistortion(
        const RenderContext& renderContext_,
        bool useMaterialOverlay_,
        bool useInstancedPose_ = false);

    void UpdateSkeleton(
        const RenderContext& renderContext_,
        const GltfModelCommponennt::Mesh& mesh_);

    void UpdateSkeleton(
        const RenderContext& renderContext_,
        const std::vector<DirectX::XMFLOAT4X4>& transforms_);

    void UpdateScene(const RenderContext& renderContext_);

    void UpdatePart(
        const RenderContext& renderContext_,
        const MeshVFXPart& part_,
        const MeshVFXRuntimePart& runtimePart_,
        ID3D11ShaderResourceView* baseTexture_,
        ID3D11ShaderResourceView* normalTexture_,
        ID3D11ShaderResourceView* maskTexture_,
        ID3D11ShaderResourceView* noiseTexture_,
        const DirectX::XMMATRIX& world_,
        float partTime_,
        float alpha_);

    void End(const RenderContext& renderContext_);

private:
    // Scene.hlsli側へ渡すカメラ・ライト用ConstantBuffer
    struct CbScene
    {
        DirectX::XMFLOAT4X4 viewProjection;      // View * Projection行列
        DirectX::XMFLOAT4 lightDirection{};      // 平行光源の方向
        DirectX::XMFLOAT4 lightColor{};          // 平行光源の色
        DirectX::XMFLOAT4 cameraPosition{};      // カメラのワールド座標
        DirectX::XMFLOAT4 zBufferParameters{};   // 深度復元用パラメータ

        int numPointLights = 0;                  // 点光源数
        int numSpotLights = 0;                   // スポットライト数
        DirectX::XMFLOAT2 scenePadding{};        // 16byteアライン調整

        DirectX::XMFLOAT4X4 invViewProjection;   // Screen/DepthからWorldへ戻すための逆行列
    };

    // MeshVFX.hlsliのCbMeshVFXと同じ順番で並べるPart用ConstantBuffer
    struct CbMeshVFX
    {
        DirectX::XMFLOAT4X4 world;       // Partのワールド行列
        DirectX::XMFLOAT4 color;         // BaseTextureに掛ける色

        DirectX::XMFLOAT2 baseUVSpeed;   // Base UVスクロール速度
        DirectX::XMFLOAT2 maskUVSpeed;   // Mask UVスクロール速度
        DirectX::XMFLOAT2 noiseUVSpeed;  // Noise UVスクロール速度

        DirectX::XMFLOAT2 baseUVScale;   // Base UVスケール
        DirectX::XMFLOAT2 maskUVScale;   // Mask UVスケール
        DirectX::XMFLOAT2 noiseUVScale;  // Noise UVスケール
        DirectX::XMFLOAT2 normalUVScale; // Normal UVスケール

        DirectX::XMFLOAT2 distortionNoiseUVScale; // 歪みNoise UVスケール
        DirectX::XMFLOAT2 edgePatternUVScale;     // Edge模様 UVスケール
        DirectX::XMFLOAT2 emissiveUVScale;        // Emissive UVスケール
        DirectX::XMFLOAT2 uvRotationCenter;       // UV回転・放射スケールの中心

        float time;                      // Partごとの再生時間
        float alpha;                     // Partごとの最終Alpha
        float normalStrength;            // Normal Textureの強度
        float noiseStrength;             // UV Noiseの強度

        float fresnelPower;              // Fresnelの鋭さ
        float fresnelStrength;           // Fresnelの強さ
        float dissolveThreshold;         // Mask/Alpha破棄しきい値
        float baseUVRotation;            // Base UV回転。ラジアン

        float maskUVRotation;            // Mask UV回転。ラジアン
        float noiseUVRotation;           // Noise UV回転。ラジアン
        float uvRadialScale;             // UV放射スケール倍率
        float uvRadialScaleSpeed;        // UV放射スケール速度

        float vertexNoiseHorizontalStrength; // 頂点横揺れ強度
        float vertexNoiseVerticalStrength;   // 頂点縦揺れ強度
        float vertexNoiseHorizontalScale;    // 頂点横Noiseの細かさ
        float vertexNoiseVerticalScale;      // 頂点縦Noiseの細かさ

        DirectX::XMFLOAT2 vertexNoisePadding; // 16byteアライン調整
        DirectX::XMFLOAT3 vertexNoiseSpeed;   // 頂点Noiseスクロール速度
        uint32_t useVertexNoise;              // 頂点Noiseを使うか

        uint32_t useMask;               // Maskを使うか
        uint32_t useNoise;              // UV Noiseを使うか
        uint32_t useNormal;             // Normal Textureを使うか
        uint32_t useFresnel;            // Fresnelを使うか

        DirectX::XMFLOAT4 edgeColor;    // Edge Highlight色

        float maskDistortionStrength;   // 背景歪み強度
        float maskDistortionOpacity;    // 背景歪みAlpha
        float silhouetteEdgeStrength;   // シルエット強調強度
        float silhouetteEdgePower;      // シルエット強調の細さ

        float intersectionEdgeStrength; // 深度交差Edge強度
        float intersectionEdgeWidth;    // 深度交差Edge幅
        uint32_t useMaskDistortion;     // 背景歪みを使うか
        uint32_t useEdgeHighlight;      // Edge Highlightを使うか

        uint32_t hasDistortionNoiseTexture; // 歪みNoise Textureがあるか
        uint32_t hasVertexNoiseTexture;     // 頂点Noise Textureがあるか
        uint32_t edgePatternMode;           // Edge模様の参照方法
        float edgePatternStrength;          // Edge模様の反映割合

        float emissiveStrength;        // Emissiveの強さ
        float emissivePulseSpeed;      // Emissive点滅速度
        float emissivePulseAmount;     // Emissive点滅量
        uint32_t hasEdgePatternTexture; // Edge Pattern Textureがあるか

        uint32_t hasEmissiveTexture;   // Emissive Textureがあるか

        uint32_t useGradientTexture;   // Gradient Textureを使うか
        uint32_t hasGradientTexture;   // Gradient Textureがあるか
        uint32_t gradientSampleMode;   // 0:U方向 1:V方向
        uint32_t gradientDirection;    // 0:順方向 1:逆方向

        float gradientFixedUv;         // Gradient Textureの固定UV
        float gradientStartUv;         // Gradientの開始UV
        float gradientEndUv;           // Gradientの終了UV
        float gradientPadding;         // HLSL cbuffer境界合わせ用
        DirectX::XMFLOAT3 gradientRegisterPadding{}; // gradientPadding後の12byte埋め

        DirectX::XMFLOAT4 dissolveEdgeColor; // Dissolve境界発光色
        DirectX::XMFLOAT4 backFaceColor;     // 背面Boost色

        float dissolveEdgeWidth;       // Dissolve境界幅
        float dissolveEdgeStrength;    // Dissolve境界発光強度
        float fresnelNoiseStrength;    // FresnelにNoiseを混ぜる強さ
        float backFaceEmissiveStrength;// 背面Boost強度

        uint32_t useDissolveEdge;      // Dissolve境界発光を使うか
        uint32_t useFresnelNoise;      // Fresnel Noiseを使うか
        uint32_t useBackFaceBoost;     // 背面Boostを使うか
        uint32_t meshVFXPadding;       // 16byteアライン調整

        uint32_t baseSamplerMode;
        uint32_t maskSamplerMode;
        uint32_t noiseSamplerMode;
        uint32_t samplerPadding;

        uint32_t normalSamplerMode;
        uint32_t distortionSamplerMode;
        uint32_t edgePatternSamplerMode;
        uint32_t emissiveSamplerMode;

        uint32_t gradientSamplerMode;
        DirectX::XMUINT3 samplerRegisterPadding;
    };

    static_assert(sizeof(CbMeshVFX) % 16 == 0, "CbMeshVFX size must be aligned to 16 bytes.");

private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;                  // 通常MeshVFX用VS
    Microsoft::WRL::ComPtr<ID3D11VertexShader> materialOverlayVertexShader;   // MaterialOverlay用VS
    Microsoft::WRL::ComPtr<ID3D11VertexShader> afterimageInstancedVertexShader; // Afterimage Instancing用VS

    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;            // 通常MeshVFX用PS
    Microsoft::WRL::ComPtr<ID3D11PixelShader> distortionPixelShader;  // 背景歪み用PS

    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout; // GltfModel頂点形式用InputLayout

    Microsoft::WRL::ComPtr<ID3D11Buffer> sceneConstantBuffer;    // Scene用CB
    Microsoft::WRL::ComPtr<ID3D11Buffer> partConstantBuffer;     // Part用CB
    Microsoft::WRL::ComPtr<ID3D11Buffer> skeletonConstantBuffer; // Skinning用Bone行列CB
};