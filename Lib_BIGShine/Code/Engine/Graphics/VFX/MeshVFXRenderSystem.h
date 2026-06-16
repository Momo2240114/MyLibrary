#pragma once

#include "MeshVFXShader.h"

#include <d3d11.h>
#include <memory>
#include <vector>
#include <wrl.h>

class MeshVFXComponent;
struct RenderContext;

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

private:
    struct MixConstants
    {
        uint32_t blendMode = 0;
        uint32_t invertLayer = 0;
        float blendWeight = 1.0f;
        float padding = 0.0f;
    };
    struct Target
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
    };

    bool CreateTarget(ID3D11Device* device_, Target& target_);

    Microsoft::WRL::ComPtr<ID3D11VertexShader> fullScreenVertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> mixPixelShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    Target targets[2];
};

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

    std::unique_ptr<MeshVFXShader> shader;
    std::vector<MeshVFXComponent*> components;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> sceneColorCopy;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneColorCopySRV;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> sceneDepthCopy;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneDepthCopySRV;
    MeshVFXTextureMixer maskTextureMixer;
    MeshVFXTextureMixer noiseTextureMixer;
    bool isInitialized = false;
};
