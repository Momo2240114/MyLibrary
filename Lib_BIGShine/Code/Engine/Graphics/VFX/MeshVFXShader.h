#pragma once

#include "MeshVFXResource.h"
#include "Engine/Graphics/Resource/GLTFModel.h"

#include <DirectXMath.h>
#include <cstdint>
#include <d3d11.h>
#include <wrl.h>

struct RenderContext;

// MeshVFX逕ｨShader荳蠑上→ConstantBuffer譖ｴ譁ｰ繧偵∪縺ｨ繧√ｋ繧ｯ繝ｩ繧ｹ縲・
// RenderSystem縺ｯ謠冗判縺吶ｋPart縺斐→縺ｫUpdatePart繧貞他縺ｳ縲√％縺薙′HLSL縺ｸ蛟､繧呈ｩ区ｸ｡縺励☆繧九・
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
    // Scene.hlsli蛛ｴ縺ｮ繧ｫ繝｡繝ｩ繝ｻ繝ｩ繧､繝域ュ蝣ｱ縺ｨ謠・∴繧鬼cene逕ｨConstantBuffer縲・
    struct CbScene
    {
        DirectX::XMFLOAT4X4 viewProjection;      // View * Projection陦悟・縲・
        DirectX::XMFLOAT4 lightDirection{};      // 蟷ｳ陦悟・縺ｮ譁ｹ蜷代・
        DirectX::XMFLOAT4 lightColor{};          // 蟷ｳ陦悟・縺ｮ濶ｲ縲・
        DirectX::XMFLOAT4 cameraPosition{};      // 繝ｯ繝ｼ繝ｫ繝臥ｩｺ髢薙・繧ｫ繝｡繝ｩ菴咲ｽｮ縲・
        DirectX::XMFLOAT4 zBufferParameters{};   // 豺ｱ蠎ｦ蠕ｩ蜈・畑繝代Λ繝｡繝ｼ繧ｿ縲・
        int numPointLights = 0;                  // 轤ｹ蜈画ｺ先焚縲・
        int numSpotLights = 0;                   // 繧ｹ繝昴ャ繝医Λ繧､繝域焚縲・
        DirectX::XMFLOAT2 scenePadding{};        // 16byte繧｢繝ｩ繧､繝ｳ逕ｨ縲・
        DirectX::XMFLOAT4X4 invViewProjection;   // Screen/Depth縺九ｉWorld蠕ｩ蜈・☆繧九◆繧√・騾・｡悟・縲・
    };

    // MeshVFX.hlsli縺ｮCbMeshVFX縺ｨ蜷後§鬆・ｺ上〒荳ｦ縺ｹ繧輝art逕ｨConstantBuffer縲・
    struct CbMeshVFX
    {
        DirectX::XMFLOAT4X4 world;       // Part縺ｮ繝ｯ繝ｼ繝ｫ繝芽｡悟・縲・
        DirectX::XMFLOAT4 color;         // BaseTexture縺ｫ謗帙￠繧玖牡縲・
        DirectX::XMFLOAT2 baseUVSpeed;   // Base UV繧ｹ繧ｯ繝ｭ繝ｼ繝ｫ騾溷ｺｦ縲・
        DirectX::XMFLOAT2 maskUVSpeed;   // Mask UV繧ｹ繧ｯ繝ｭ繝ｼ繝ｫ騾溷ｺｦ縲・
        DirectX::XMFLOAT2 noiseUVSpeed;  // Noise UV繧ｹ繧ｯ繝ｭ繝ｼ繝ｫ騾溷ｺｦ縲・
        DirectX::XMFLOAT2 baseUVScale;
        DirectX::XMFLOAT2 maskUVScale;
        DirectX::XMFLOAT2 noiseUVScale;
        DirectX::XMFLOAT2 normalUVScale;
        DirectX::XMFLOAT2 distortionNoiseUVScale;
        DirectX::XMFLOAT2 edgePatternUVScale;
        DirectX::XMFLOAT2 emissiveUVScale;
        DirectX::XMFLOAT2 uvRotationCenter; // UV蝗櫁ｻ｢繝ｻ謾ｾ蟆・僑邵ｮ縺ｮ荳ｭ蠢・・
        float time;                      // Part縺斐→縺ｮ蜀咲函譎る俣縲・
        float alpha;                     // Part縺斐→縺ｮ譛邨・lpha縲・
        float normalStrength;            // Normal Texture縺ｮ蠑ｷ蠎ｦ縲・
        float noiseStrength;             // UV Noise縺ｮ蠑ｷ蠎ｦ縲・
        float fresnelPower;              // Fresnel縺ｮ驪ｭ縺輔・
        float fresnelStrength;           // Fresnel縺ｮ蠑ｷ縺輔・
        float dissolveThreshold;         // Mask/Alpha遐ｴ譽・明蛟､縲・
        float baseUVRotation;            // Base UV蝗櫁ｻ｢縲ゅΛ繧ｸ繧｢繝ｳ縲・
        float maskUVRotation;            // Mask UV蝗櫁ｻ｢縲ゅΛ繧ｸ繧｢繝ｳ縲・
        float noiseUVRotation;           // Noise UV蝗櫁ｻ｢縲ゅΛ繧ｸ繧｢繝ｳ縲・
        float uvRadialScale;             // UV謾ｾ蟆・僑邵ｮ蛟咲紫縲・
        float uvRadialScaleSpeed;        // UV謾ｾ蟆・僑邵ｮ騾溷ｺｦ縲・
        float vertexNoiseHorizontalStrength; // 鬆らせ讓ｪ謠ｺ繧悟ｼｷ蠎ｦ縲・
        float vertexNoiseVerticalStrength;   // 鬆らせ邵ｦ謠ｺ繧悟ｼｷ蠎ｦ縲・
        float vertexNoiseHorizontalScale;    // 鬆らせ讓ｪNoise縺ｮ邏ｰ縺九＆縲・
        float vertexNoiseVerticalScale;      // 鬆らせ邵ｦNoise縺ｮ邏ｰ縺九＆縲・
        DirectX::XMFLOAT3 vertexNoiseSpeed;  // 鬆らせNoise繧ｹ繧ｯ繝ｭ繝ｼ繝ｫ騾溷ｺｦ縲・
        uint32_t useVertexNoise;        // 鬆らせNoise繧剃ｽｿ縺・°縲・
        uint32_t useMask;               // Mask繧剃ｽｿ縺・°縲・
        uint32_t useNoise;              // UV Noise繧剃ｽｿ縺・°縲・
        uint32_t useNormal;             // Normal Texture繧剃ｽｿ縺・°縲・
        uint32_t useFresnel;            // Fresnel繧剃ｽｿ縺・°縲・
        DirectX::XMFLOAT4 edgeColor;    // Edge Highlight濶ｲ縲・
        float maskDistortionStrength;   // 閭梧勹豁ｪ縺ｿ蠑ｷ蠎ｦ縲・
        float maskDistortionOpacity;    // 閭梧勹豁ｪ縺ｿAlpha縲・
        float silhouetteEdgeStrength;   // 繧ｷ繝ｫ繧ｨ繝・ヨ蠑ｷ隱ｿ蠑ｷ蠎ｦ縲・
        float silhouetteEdgePower;      // 繧ｷ繝ｫ繧ｨ繝・ヨ蠑ｷ隱ｿ縺ｮ邏ｰ縺輔・
        float intersectionEdgeStrength; // 豺ｱ蠎ｦ莠､蟾ｮ蠑ｷ隱ｿ蠑ｷ蠎ｦ縲・
        float intersectionEdgeWidth;    // 豺ｱ蠎ｦ莠､蟾ｮ蟷・・
        uint32_t useMaskDistortion;     // 閭梧勹豁ｪ縺ｿ繧剃ｽｿ縺・°縲・
        uint32_t useEdgeHighlight;      // Edge Highlight繧剃ｽｿ縺・°縲・
        uint32_t hasDistortionNoiseTexture; // 豁ｪ縺ｿNoise縺瑚ｪｭ縺ｿ霎ｼ繧√※縺・ｋ縺九・
        uint32_t hasVertexNoiseTexture; // 鬆らせNoise縺瑚ｪｭ縺ｿ霎ｼ繧√※縺・ｋ縺九・
        uint32_t edgePatternMode;       // Edge讓｡讒伜盾辣ｧ蜈・・
        float edgePatternStrength;      // Edge讓｡讒倥・蜿肴丐蜑ｲ蜷医・
        float emissiveStrength;
        float emissivePulseSpeed;
        float emissivePulseAmount;
        uint32_t hasEdgePatternTexture;
        uint32_t hasEmissiveTexture;
        float padding;
    };
    static_assert(sizeof(CbMeshVFX) % 16 == 0);

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader; // 騾壼ｸｸMeshVFX逕ｨVS縲・
    Microsoft::WRL::ComPtr<ID3D11VertexShader> materialOverlayVertexShader; // MaterialOverlay逕ｨVS縲・
    Microsoft::WRL::ComPtr<ID3D11VertexShader> afterimageInstancedVertexShader; // Afterimage Instancing逕ｨVS縲・
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> distortionPixelShader; // MeshVFX蜈ｱ騾啀S縲・
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout; // GltfModel鬆らせ蠖｢蠑上・InputLayout縲・
    Microsoft::WRL::ComPtr<ID3D11Buffer> sceneConstantBuffer; // Scene逕ｨCB縲・
    Microsoft::WRL::ComPtr<ID3D11Buffer> partConstantBuffer; // Part逕ｨCB縲・
    Microsoft::WRL::ComPtr<ID3D11Buffer> skeletonConstantBuffer; // Skinning逕ｨ繝懊・繝ｳ陦悟・CB縲・
};
