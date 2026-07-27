#pragma once

#include <DirectXMath.h>
#include <d3d11.h>
#include <memory>
#include <string>
#include <vector>
#include <wrl.h>

class ModelResource;
class GltfModelCommponennt;

// MeshVFX縺ｮ蜷域・譁ｹ豕輔・
enum class MeshVFXBlendMode
{
    ALPHA,      // 騾壼ｸｸ蜊企乗・縲る乗・縺ｪ閹懊ｄ繧ｪ繝ｼ繝舌・繝ｬ繧､蜷代″縲・
    ADDITIVE,   // 蜉邂怜粋謌舌ら匱蜈峨お繝輔ぉ繧ｯ繝医ｄ譁ｬ謦・・蜷代″縲・
};

// 隍・焚Mask Texture繧貞粋謌舌☆繧九→縺阪・譁ｹ豕輔・
enum class MeshVFXMaskBlendMode
{
    MULTIPLY,   // 譌｢蟄弄ask縺ｫ謗帙￠蜷医ｏ縺帙ｋ縲ゅ￥繧頑栢縺阪ｒ蠑ｷ繧√ｋ縲・
    UNION,      // 譌｢蟄弄ask縺ｨ譛螟ｧ蛟､蜷域・縺吶ｋ縲らｯ・峇繧貞ｺ・￡繧九・
};

// Edge Highlight縺ｫTexture讓｡讒倥ｒ豺ｷ縺懊ｋ譁ｹ豕輔・
enum class MESH_VFX_EDGE_PATTERN_MODE
{
    NONE,           // 蜊倩牡縺ｮEdge Highlight縲・
    BASE_TEXTURE,   // Base Texture縺ｮ譏主ｺｦ縺ｧEdge縺ｫ讓｡讒倥ｒ蜈･繧後ｋ縲・
    MASK_TEXTURE,   // Mask Texture縺ｮ蛟､縺ｧEdge縺ｫ讓｡讒倥ｒ蜈･繧後ｋ縲・
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

// Mask Texture 1譫壼・縺ｮ諠・ｱ縲・
struct MeshVFXMaskLayer
{
    std::string path;   // Texture Asset縺ｮ繝代せ縲・
    MeshVFXMaskBlendMode blendMode = MeshVFXMaskBlendMode::MULTIPLY; // 蜷域・譁ｹ豕輔・
    bool invert = false; // true縺ｪ繧臥區鮟偵ｒ蜿崎ｻ｢縺励※縺九ｉ蜷域・縺吶ｋ縲・
};

// MeshVFX縺ｮ1繝代・繝・・縺ｮ邱ｨ髮・ョ繝ｼ繧ｿ縲ゅΔ繝・Ν縲ゝexture縲ゞV縲∫匱蜈峨∵ｭｪ縺ｿ縲・dge縺ｪ縺ｩ繧偵∪縺ｨ繧√ｋ縲・
struct MeshVFXInternalMeshSettings
{
    int segments = 96;

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

    float beamLength = 1.0f;
    float beamHalfHeight = 0.45f;
    float beamHalfWidth = 0.12f;
    float beamDepthThickness = 0.018f;
    float beamSidePanelRate = 0.55f;
};

struct MeshVFXPart
{
    std::string partAssetPath; // 蜊倅ｽ撤art Asset縺ｨ縺励※菫晏ｭ倥＠縺櫟SON繝代せ縲・
    std::string modelPath;     // 陦ｨ遉ｺ縺ｫ菴ｿ縺・Δ繝・ΝAsset繝代せ縲・
    int meshIndex = -1;        // 謠冗判縺吶ｋMesh逡ｪ蜿ｷ縲・1縺ｪ繧牙・Mesh蟇ｾ雎｡縲・
    bool renderEnabled = false;// false縺ｪ繧峨％縺ｮPart縺ｯ謠冗判縺励↑縺・・
    bool useMaterialBaseTexture = true;  // BaseTexture譛ｪ謖・ｮ壽凾縺ｫ繝｢繝・ΝMaterial縺ｮBaseMap繧剃ｽｿ縺・・
    bool useMaterialNormalTexture = true;// NormalTexture譛ｪ謖・ｮ壽凾縺ｫ繝｢繝・ΝMaterial縺ｮNormalMap繧剃ｽｿ縺・・

    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };          // Part縺ｮ繝ｭ繝ｼ繧ｫ繝ｫ菴咲ｽｮ縲・
    DirectX::XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f };          // Part縺ｮ繝ｭ繝ｼ繧ｫ繝ｫ蝗櫁ｻ｢縲ょｺｦ謨ｰ豕輔・
    DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };             // Part縺ｮ繝ｭ繝ｼ繧ｫ繝ｫ諡｡邵ｮ縲・
    float shapeCylinderRadius = 1.0f;
    MeshVFXInternalMeshSettings internalMeshSettings;
    DirectX::XMFLOAT3 meshRotationSpeed = { 0.0f, 0.0f, 0.0f }; // 譎る俣縺ｧ蜉邂励☆繧句屓霆｢騾溷ｺｦ縲ょｺｦ/遘偵・

    float duration = 0.0f;            // 0縺ｪ繧画凾髢灘宛髯舌↑縺励よｭ｣縺ｮ蛟､縺ｪ繧臼art縺ｮ蜀咲函髟ｷ縲・
    bool loop = true;                 // duration蛻ｰ驕泌ｾ後↓譎る俣繧偵Ν繝ｼ繝励☆繧九°縲・
    bool animateScale = false;        // 譎る俣縺ｧscaleStart縺九ｉscaleEnd縺ｸ陬憺俣縺吶ｋ縺九・
    DirectX::XMFLOAT3 scaleStart = { 1.0f, 1.0f, 1.0f }; // Scale繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ髢句ｧ句､縲・
    DirectX::XMFLOAT3 scaleEnd = { 1.0f, 1.0f, 1.0f };   // Scale繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ邨ゆｺ・､縲・
    bool animateAlpha = false;        // 譎る俣縺ｧalphaStart縺九ｉalphaEnd縺ｸ陬憺俣縺吶ｋ縺九・
    float alphaStart = 1.0f;          // Alpha繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ髢句ｧ句､縲・
    float alphaEnd = 1.0f;            // Alpha繧｢繝九Γ繝ｼ繧ｷ繝ｧ繝ｳ邨ゆｺ・､縲・

    std::string baseTexturePath;              // 濶ｲ繝ｻ讓｡讒倥・荳ｻTexture繝代せ縲・
    std::string normalTexturePath;            // 豕慕ｷ啜exture繝代せ縲・
    std::string maskTexturePath;              // 譌ｧ莠呈鋤逕ｨMask Texture繝代せ縲・
    std::string noiseTexturePath;             // 譌ｧ莠呈鋤逕ｨNoise Texture繝代せ縲・
    std::string distortionNoiseTexturePath;   // 閭梧勹豁ｪ縺ｿ縺ｫ菴ｿ縺・oise Texture繝代せ縲・
    std::string vertexNoiseTexturePath;       // 鬆らせ謠ｺ繧峨＠縺ｫ菴ｿ縺・oise Texture繝代せ縲・
    std::string edgePatternTexturePath;
    std::string emissiveTexturePath;
    std::string gradientTexturePath;
    std::vector<MeshVFXMaskLayer> maskLayers; // 隍・焚Mask縺ｮ蜷域・險ｭ螳壹・
    std::vector<std::string> noiseTexturePaths;// 隍・焚Noise縺ｮ蟷ｳ蝮・粋謌千畑Texture繝代せ縲・

    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f }; // BaseTexture縺ｫ謗帙￠繧玖牡縲・
    DirectX::XMFLOAT2 baseUVSpeed = { 0.0f, -0.5f };      // BaseTexture縺ｮUV繧ｹ繧ｯ繝ｭ繝ｼ繝ｫ騾溷ｺｦ縲・
    DirectX::XMFLOAT2 maskUVSpeed = { 0.0f, -0.2f };      // MaskTexture縺ｮUV繧ｹ繧ｯ繝ｭ繝ｼ繝ｫ騾溷ｺｦ縲・
    DirectX::XMFLOAT2 noiseUVSpeed = { 0.2f, -0.8f };     // NoiseTexture縺ｮUV繧ｹ繧ｯ繝ｭ繝ｼ繝ｫ騾溷ｺｦ縲・
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
    DirectX::XMFLOAT2 uvRotationCenter = { 0.5f, 0.5f };  // UV蝗櫁ｻ｢繝ｻ謾ｾ蟆・僑邵ｮ縺ｮ荳ｭ蠢・・
    float baseUVRotation = 0.0f;          // BaseTexture縺ｮ蛻晄悄UV蝗櫁ｻ｢縲ょｺｦ謨ｰ豕輔・
    float maskUVRotation = 0.0f;          // MaskTexture縺ｮ蛻晄悄UV蝗櫁ｻ｢縲ょｺｦ謨ｰ豕輔・
    float noiseUVRotation = 0.0f;         // NoiseTexture縺ｮ蛻晄悄UV蝗櫁ｻ｢縲ょｺｦ謨ｰ豕輔・
    float baseUVRotationSpeed = 0.0f;     // BaseTexture縺ｮUV蝗櫁ｻ｢騾溷ｺｦ縲ょｺｦ/遘偵・
    float maskUVRotationSpeed = 0.0f;     // MaskTexture縺ｮUV蝗櫁ｻ｢騾溷ｺｦ縲ょｺｦ/遘偵・
    float noiseUVRotationSpeed = 0.0f;    // NoiseTexture縺ｮUV蝗櫁ｻ｢騾溷ｺｦ縲ょｺｦ/遘偵・
    float uvRadialScale = 1.0f;           // UV荳ｭ蠢・°繧峨・謾ｾ蟆・僑邵ｮ蛟咲紫縲・
    float uvRadialScaleSpeed = 0.0f;      // 謾ｾ蟆・僑邵ｮ蛟咲紫縺ｮ譎る俣螟牙喧縲・

    float alpha = 1.0f;                  // Part蜈ｨ菴薙・騾乗・蠎ｦ縲・
    float emissiveStrength = 3.0f;       // 莠育ｴ・､縲ら匱蜈牙ｼｷ蠎ｦ縺ｨ縺励※Asset縺ｸ菫晄戟縺吶ｋ縲・
    float emissivePulseSpeed = 2.0f;     // 莠育ｴ・､縲ら匱蜈峨ヱ繝ｫ繧ｹ騾溷ｺｦ縺ｨ縺励※Asset縺ｸ菫晄戟縺吶ｋ縲・
    float emissivePulseAmount = 0.35f;   // 莠育ｴ・､縲ら匱蜈峨ヱ繝ｫ繧ｹ驥上→縺励※Asset縺ｸ菫晄戟縺吶ｋ縲・
    float normalStrength = 1.0f;         // Normal Texture縺ｮXY謌仙・蠑ｷ蠎ｦ縲・
    float noiseStrength = 0.05f;         // Noise縺ｧBase/Mask UV繧呈ｭｪ縺ｾ縺帙ｋ蠑ｷ縺輔・
    float vertexNoiseHorizontalStrength = 0.1f; // 鬆らせ繧呈ｳ慕ｷ壽ｨｪ譁ｹ蜷代∈謠ｺ繧峨☆蠑ｷ縺輔・
    float vertexNoiseVerticalStrength = 0.1f;   // 鬆らせ繧炭譁ｹ蜷代∈謠ｺ繧峨☆蠑ｷ縺輔・
    float vertexNoiseHorizontalScale = 1.0f;    // 讓ｪ譁ｹ蜷鷹らせNoise縺ｮ邏ｰ縺九＆縲・
    float vertexNoiseVerticalScale = 1.0f;      // 邵ｦ譁ｹ蜷鷹らせNoise縺ｮ邏ｰ縺九＆縲・
    DirectX::XMFLOAT3 vertexNoiseSpeed = { 0.0f, 1.0f, 0.0f }; // 鬆らせNoise縺ｮ繧ｹ繧ｯ繝ｭ繝ｼ繝ｫ騾溷ｺｦ縲・
    float maskDistortionStrength = 8.0f; // Mask螟門・縺ｧ閭梧勹繧呈ｭｪ縺ｾ縺帙ｋ逕ｻ邏蜊倅ｽ阪・蠑ｷ縺輔・
    float maskDistortionOpacity = 0.5f;  // 豁ｪ縺ｿ驛ｨ蛻・・Alpha縲・
    DirectX::XMFLOAT4 edgeColor = { 0.25f, 0.75f, 1.0f, 1.0f }; // Mesh螟門捉蠑ｷ隱ｿ縺ｮ濶ｲ縲・
    float silhouetteEdgeStrength = 0.0f; // 隕也ｷ壹↓蟇ｾ縺励※螟門捉縺ｫ隕九∴繧矩擇繧貞ｼｷ隱ｿ縺吶ｋ蠑ｷ縺輔・
    float silhouetteEdgePower = 3.0f;    // 繧ｷ繝ｫ繧ｨ繝・ヨ蠑ｷ隱ｿ縺ｮ邏ｰ縺輔ょ､ｧ縺阪＞縺ｻ縺ｩ邏ｰ縺・・
    float intersectionEdgeStrength = 0.0f;// 豺ｱ蠎ｦ蟾ｮ縺ｧ莠､蟾ｮ驛ｨ蛻・ｒ蠑ｷ隱ｿ縺吶ｋ蠑ｷ縺輔・
    float intersectionEdgeWidth = 0.002f;// 莠､蟾ｮ蛻､螳壹・蟷・・
    float edgePatternStrength = 0.0f;    // Edge縺ｫTexture讓｡讒倥ｒ豺ｷ縺懊ｋ蜑ｲ蜷医・
    float fresnelPower = 2.0f;           // Fresnel縺ｮ驪ｭ縺輔・
    float fresnelStrength = 1.0f;        // Fresnel縺ｮ蠑ｷ縺輔・
    float dissolveThreshold = 0.0f;      // Mask/Alpha縺後％縺ｮ蛟､莉･荳九↑繧臥ｴ譽・☆繧九・
    float gradientFixedUv = 0.5f;         // Gradient Texture縺ｮ蝗ｺ螳啅V
    float gradientStartUv = 0.0f;         // Gradient縺ｮ髢句ｧ偽V
    float gradientEndUv = 1.0f;           // Gradient縺ｮ邨ゆｺ・V
    float gradientPadding;         // 16byte繧｢繝ｩ繧､繝ｳ隱ｿ謨ｴ
    DirectX::XMFLOAT3 gradientRegisterPadding{}; // HLSL cbuffer蠅・阜蜷医ｏ縺・

    DirectX::XMFLOAT4 dissolveEdgeColor; // Dissolve蠅・阜逋ｺ蜈芽牡
    DirectX::XMFLOAT4 backFaceColor;     // 閭碁擇Boost濶ｲ
    float dissolveEdgeWidth = 0.05f;
    float dissolveEdgeStrength = 3.0f;
    float fresnelNoiseStrength = 0.0f;
    float backFaceEmissiveStrength = 0.0f;
    std::string alphaCurveName;
    std::string scaleCurveName;
    std::string dissolveCurveName;
    std::string emissiveCurveName;
    std::string uvScrollCurveName;
    std::string uvRotationCurveName;

    bool useMask = false;                // Mask Texture縺ｧ陦ｨ遉ｺ遽・峇繧貞宛蠕｡縺吶ｋ縺九・
    bool useNoise = false;               // Noise Texture縺ｧUV繧呈ｭｪ縺ｾ縺帙ｋ縺九・
    bool useVertexNoise = false;         // Vertex Shader縺ｧMesh鬆らせ繧呈昭繧峨☆縺九・
    bool useMaskDistortion = false;      // Mask縺ｧ髫繧後ｋ鬆伜沺縺ｫ閭梧勹豁ｪ縺ｿ繧貞・縺吶°縲・
    bool useEdgeHighlight = false;       // Mesh陦ｨ髱｢蜀・・繧ｷ繝ｫ繧ｨ繝・ヨ/莠､蟾ｮ蜻ｨ霎ｺ繧貞ｼｷ隱ｿ縺吶ｋ縺九・
    bool useNormal = false;              // Normal Texture縺ｧ豕慕ｷ壹ｒ陬懈ｭ｣縺吶ｋ縺九・
    bool useFresnel = false;             // 隕也ｷ夊ｧ偵↓繧医ｋFresnel逋ｺ蜈峨ｒ菴ｿ縺・°縲・
    bool useGradientTexture = false;
    bool useDissolveEdge = false;
    bool useFresnelNoise = false;
    bool useBackFaceBoost = false;
    MESH_VFX_EDGE_PATTERN_MODE edgePatternMode = MESH_VFX_EDGE_PATTERN_MODE::NONE; // Edge讓｡讒倥・蜿ら・蜈・・
    MESH_VFX_GRADIENT_SAMPLE_MODE gradientSampleMode = MESH_VFX_GRADIENT_SAMPLE_MODE::HORIZONTAL;
    MESH_VFX_GRADIENT_DIRECTION gradientDirection = MESH_VFX_GRADIENT_DIRECTION::FORWARD;
    MeshVFXBlendMode blendMode = MeshVFXBlendMode::ALPHA; // 謠冗判譎ゅ・蜷域・譁ｹ豕輔・
};

// 隱ｭ縺ｿ霎ｼ繧薙□GPU繝ｪ繧ｽ繝ｼ繧ｹ縲・eshVFXPart縺ｯ菫晏ｭ倥ョ繝ｼ繧ｿ縲√％縺ｮ讒矩菴薙・螳溯｡梧凾繧ｭ繝｣繝・す繝･縲・
struct MeshVFXRuntimePart
{
    std::shared_ptr<ModelResource> modelResource; // 謠冗判蟇ｾ雎｡繝｢繝・Ν縲・
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> baseTexture; // Base Texture縺ｮSRV縲・
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalTexture; // Normal Texture縺ｮSRV縲・
    std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> maskTextures; // 蜷域・蜑庚ask Texture鄒､縲・
    std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> noiseTextures; // 蜷域・蜑康oise Texture鄒､縲・
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> distortionNoiseTexture; // 閭梧勹豁ｪ縺ｿ逕ｨNoise縲・
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> vertexNoiseTexture; // 鬆らせ謠ｺ繧峨＠逕ｨNoise縲・
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> edgePatternTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> emissiveTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gradientTexture;
};

// Trail縺ｪ縺ｩ縺悟ｮ溯｡梧凾縺ｫ菴懊▲縺櫪esh繧樽eshVFX縺ｨ縺励※謠上￥縺溘ａ縺ｮ繝舌ャ繝輔ぃ蜿ら・縲・
// 繝舌ャ繝輔ぃ縺ｮ謇譛画ｨｩ縺ｯ豢ｾ逕櫃omponent蛛ｴ縺ｫ縺ゅｊ縲ヽenderSystem縺ｯ蛟溘ｊ縺ｦ謠冗判縺吶ｋ縺縺代・
struct MeshVFXDynamicMesh
{
    ID3D11Buffer* vertexBuffer = nullptr; // 蜍慕噪Mesh縺ｮ鬆らせ繝舌ャ繝輔ぃ縲・
    ID3D11Buffer* indexBuffer = nullptr;  // 蜍慕噪Mesh縺ｮIndex繝舌ャ繝輔ぃ縲・
    UINT indexCount = 0;                  // 謠冗判縺吶ｋIndex謨ｰ縲・
    UINT vertexStride = 0;                // 鬆らせ1蛟句・縺ｮ繝舌う繝医し繧､繧ｺ縲・
};

// Afterimage縺ｪ縺ｩ縲√Δ繝・Ν蟋ｿ蜍｢繧定､・｣ｽ縺励※謠上￥縺溘ａ縺ｮPose繝・・繧ｿ縲・
struct MeshVFXModelPose
{
    std::shared_ptr<GltfModelCommponennt> model; // 蟋ｿ蜍｢繧呈戟縺､繝｢繝・Ν縲・
    std::vector<std::vector<DirectX::XMFLOAT4X4>> meshTransforms; // Mesh縺斐→縺ｮ繝懊・繝ｳ陦悟・縲・
    float time = 0.0f;  // Pose縺斐→縺ｮ邨碁℃譎る俣縲４hader縺ｮ譎る俣貍泌・縺ｫ貂｡縺吶・
    float alpha = 1.0f; // Pose縺斐→縺ｮ騾乗・蠎ｦ縲・
};

// MeshVFXModelPose繧偵さ繝斐・縺帙★縺ｫ蜿ら・縺吶ｋ謠冗判逕ｨ繝薙Η繝ｼ縲・
struct MeshVFXModelPoseView
{
    std::shared_ptr<GltfModelCommponennt> model; // 謠冗判蜈・Δ繝・Ν縲・
    const std::vector<std::vector<DirectX::XMFLOAT4X4>>* meshTransforms = nullptr; // 螟夜Κ謇譛峨・繝懊・繝ｳ陦悟・縲・
    float time = 0.0f;  // 繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｹ譎る俣縲・
    float alpha = 1.0f; // 繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｹ騾乗・蠎ｦ縲・
};

// MeshVFX Asset蜈ｨ菴薙り､・焚Part繧・縺､縺ｮVFX縺ｨ縺励※謇ｱ縺・・
struct MeshVFXAsset
{
    std::vector<MeshVFXPart> parts; // 蜀咲函繝ｻ謠冗判縺吶ｋPart荳隕ｧ縲・
};
