#pragma once

#include "MeshVFXShader.h"

#include <d3d11.h>
#include <memory>
#include <vector>
#include <wrl.h>

class MeshVFXComponent;
struct RenderContext;

// 複数Textureを一時RenderTarget上で合成し、MeshVFXのMask/Noiseとして渡す補助クラス。
class MeshVFXTextureMixer
{
public:
    bool Initialize(ID3D11Device* device_);
    ID3D11ShaderResourceView* Mix(
        const RenderContext& renderContext_,
        const std::vector<ID3D11ShaderResourceView*>& textures_,
        const std::vector<MeshVFXMaskBlendMode>* blendModes_,
        const std::vector<bool>* invertLayers_,
        bool average_);

    // Afterimage Instancing用にVSへ渡す1インスタンス分の時間・Alpha。
    struct AfterimageInstanceData
    {
        float time = 0.0f;             // 残像ごとの経過時間。
        float alpha = 1.0f;            // 残像ごとの透明度。
        DirectX::XMFLOAT2 padding{};   // 16byteアライン用。
    };
    static_assert(sizeof(AfterimageInstanceData) == 16);
private:

    // TextureMixPSへ渡す1Layer分の合成設定。
    struct MixConstants
    {
        uint32_t blendMode = 0;    // MeshVFXMaskBlendMode。
        uint32_t invertLayer = 0;  // 反転してから合成するか。
        float blendWeight = 1.0f;  // 合成重み。現在は基本1。
        float padding = 0.0f;      // 16byteアライン用。
    };
    // Ping-Pong合成に使う一時RenderTarget。
    struct Target
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture; // 合成結果Texture。
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView; // 書き込み用RTV。
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView; // 読み取り用SRV。
    };

    bool CreateTarget(ID3D11Device* device_, Target& target_);

    Microsoft::WRL::ComPtr<ID3D11VertexShader> fullScreenVertexShader; // 全画面三角形用VS。
    Microsoft::WRL::ComPtr<ID3D11PixelShader> mixPixelShader;          // Texture合成用PS。
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;               // MixConstants用CB。
    Target targets[2];                                                 // Ping-Pong合成先。
};

// Scene内のMeshVFXComponentを集め、まとめて描画するRenderSystem。
class MeshVFXRenderSystem
{
public:
    bool Initialize(ID3D11Device* device_);
    void PushSubmit(MeshVFXComponent* component_);
    void RenderAll(
        const RenderContext& renderContext_,
        bool isEditorWindow_ = false,
        ID3D11RenderTargetView* activeRenderTarget_ = nullptr,
        ID3D11DepthStencilView* activeDepthStencil_ = nullptr);

private:
    bool PrepareSceneSampling(
        const RenderContext& renderContext_,
        bool isEditorWindow_,
        ID3D11RenderTargetView* activeRenderTarget_,
        ID3D11DepthStencilView* activeDepthStencil_);
    bool EnsureCopyTexture(
        ID3D11Device* device_,
        ID3D11ShaderResourceView* source_,
        Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture_,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& shaderResourceView_);
    bool EnsureAfterimageBuffers(ID3D11Device* device_, UINT instanceCapacity_);
    UINT UpdateAfterimageBuffers(
        const RenderContext& renderContext_,
        const std::vector<const MeshVFXModelPoseView*>& poses_,
        size_t meshIndex_);

    std::unique_ptr<MeshVFXShader> shader; // MeshVFX描画用Shader管理。
    std::vector<MeshVFXComponent*> components; // 今フレーム描画するComponent一覧。
    Microsoft::WRL::ComPtr<ID3D11Texture2D> sceneColorCopy; // 背景歪み用のScene Colorコピー。
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneColorCopySRV; // Scene ColorコピーのSRV。
    Microsoft::WRL::ComPtr<ID3D11Texture2D> sceneDepthCopy; // Edge/交差判定用のDepthコピー。
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneDepthCopySRV; // DepthコピーのSRV。
    MeshVFXTextureMixer maskTextureMixer; // 複数Maskを1枚にまとめるMixer。
    MeshVFXTextureMixer noiseTextureMixer; // 複数Noiseを1枚にまとめるMixer。
    Microsoft::WRL::ComPtr<ID3D11Buffer> afterimageBoneBuffer; // Afterimage用ボーン行列StructuredBuffer。
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> afterimageBoneSRV; // Afterimageボーン行列SRV。
    Microsoft::WRL::ComPtr<ID3D11Buffer> afterimageInstanceBuffer; // Afterimageインスタンス情報Buffer。
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> afterimageInstanceSRV; // Afterimageインスタンス情報SRV。
    UINT afterimageInstanceCapacity = 0; // 現在確保済みのAfterimageインスタンス最大数。
    bool isInitialized = false; // Initialize済みか。
};
