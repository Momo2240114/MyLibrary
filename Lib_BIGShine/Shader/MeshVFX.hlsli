#ifndef MESH_VFX_HLSLI
#define MESH_VFX_HLSLI

#include "Scene.hlsli"

// MeshVFXのPartごとにC++から渡される定数。
// MeshVFXShader::CbMeshVFXと並び順を必ず合わせる。
cbuffer CbMeshVFX : register(b0)
{
    row_major float4x4 world; // Partのワールド行列。
    float4 vfxColor; // BaseTextureへ掛ける色。
    float2 baseUVSpeed; // Base UVのスクロール速度。
    float2 maskUVSpeed; // Mask UVのスクロール速度。
    float2 noiseUVSpeed; // Noise UVのスクロール速度。
    float2 baseUVScale;
    float2 maskUVScale;
    float2 noiseUVScale;
    float2 normalUVScale;
    float2 distortionNoiseUVScale;
    float2 edgePatternUVScale;
    float2 emissiveUVScale;
    float2 uvRotationCenter; // UV回転・放射拡縮の中心。
    float time; // Partの再生時間。
    float alpha; // Partの最終Alpha。
    float normalStrength; // Normal Textureの凹凸強度。
    float noiseStrength; // NoiseでBase/Mask UVを歪ませる強さ。
    float fresnelPower; // Fresnelの細さ。
    float fresnelStrength; // Fresnelの強さ。
    float dissolveThreshold; // Mask/Alphaで破棄する閾値。
    float baseUVRotation; // Base UV回転。ラジアン。
    float maskUVRotation; // Mask UV回転。ラジアン。
    float noiseUVRotation; // Noise UV回転。ラジアン。
    float uvRadialScale; // UV中心からの拡縮倍率。
    float uvRadialScaleSpeed; // UV拡縮の時間変化。
    float vertexNoiseHorizontalStrength; // 頂点横揺れ強度。
    float vertexNoiseVerticalStrength; // 頂点縦揺れ強度。
    float vertexNoiseHorizontalScale; // 横揺れNoiseの細かさ。
    float vertexNoiseVerticalScale; // 縦揺れNoiseの細かさ。
    float2 vertexNoisePadding;
    float3 vertexNoiseSpeed; // 頂点Noiseのスクロール速度。
    uint useVertexNoise; // 頂点Noiseを使うか。
    uint useMask; // Mask Textureを使うか。
    uint useNoise; // UV Noiseを使うか。
    uint useNormal; // Normal Textureを使うか。
    uint useFresnel; // Fresnel発光を使うか。
    float4 edgeColor; // Edge Highlightの色。
    float maskDistortionStrength; // 背景歪みの強さ。
    float maskDistortionOpacity; // 背景歪み部分のAlpha。
    float silhouetteEdgeStrength; // シルエットEdgeの強さ。
    float silhouetteEdgePower; // シルエットEdgeの細さ。
    float intersectionEdgeStrength; // 深度交差Edgeの強さ。
    float intersectionEdgeWidth; // 深度交差Edgeの幅。
    uint useMaskDistortion; // Mask Distortionを使うか。
    uint useEdgeHighlight; // Edge Highlightを使うか。
    uint hasDistortionNoiseTexture; // 歪みNoiseがあるか。
    uint hasVertexNoiseTexture; // 頂点Noiseがあるか。
    uint edgePatternMode; // 0:なし 1:BaseTexture 2:MaskTexture。
    float edgePatternStrength; // Edge模様を混ぜる割合。
    float emissiveStrength;
    float emissivePulseSpeed;
    float emissivePulseAmount;
    uint hasEdgePatternTexture;
    uint hasEmissiveTexture;
    uint useGradientTexture;
    uint hasGradientTexture;
    uint gradientSampleMode;
    uint gradientDirection;
    float gradientFixedUv;
    float gradientStartUv;
    float gradientEndUv;
    float gradientPadding;
    float3 gradientRegisterPadding;

    float4 trailOldRootColor;
    float4 trailOldTipColor;
    float4 trailNewRootColor;
    float4 trailNewTipColor;

    float trailRecencyPower;
    float trailTipPower;
    float trailRecencyIntensity;
    float trailTipIntensity;

    float trailOldAlpha;
    float trailNewAlpha;
    uint useTrailColorRamp;
    uint trailColorRampPadding;
    
    float4 dissolveEdgeColor;
    float4 backFaceColor;
    
    float dissolveEdgeWidth;
    float dissolveEdgeStrength;
    float fresnelNoiseStrength;
    float backFaceEmissiveStrength;
    
    uint useDissolveEdge;
    uint useFresnelNoise;
    uint useBackFaceBoost;
    
    uint layerMaskPass;

    uint baseSamplerMode;
    uint maskSamplerMode;
    uint noiseSamplerMode;
    uint samplerPadding;

    uint normalSamplerMode;
    uint distortionSamplerMode;
    uint edgePatternSamplerMode;
    uint emissiveSamplerMode;

    uint gradientSamplerMode;
    uint3 samplerRegisterPadding;
};

// GltfModelの頂点入力。
struct VS_IN
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 texcoord : TEXCOORD0;
    float4 boneWeights : BONE_WEIGHTS;
    uint4 boneIndices : BONE_INDICES;
};

// VertexShaderからPixelShaderへ渡す値。
struct VS_OUT
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 texcoord : TEXCOORD2;
    float4 tangent : TEXCOORD3;
    float instanceTime : TEXCOORD4;
    float instanceAlpha : TEXCOORD5;
};

#endif
