#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include "Engine/Graphics/Core/RenderContext.h"

class MotionBlurSystem
{
public:
    class ObjectVelocityRenderer
    {
    public:
        virtual ~ObjectVelocityRenderer() = default;
        virtual void DrawObjectVelocity(
            const RenderContext& rc_,
            ID3D11RenderTargetView* velocityRTV_,
            int width_,
            int height_,
            const DirectX::XMFLOAT4X4& currentViewProjection_,
            const DirectX::XMFLOAT4X4& previousViewProjection_) = 0;
        virtual void CommitHistory() {}
    };

    struct Parameters
    {
        bool enabled = false;
        bool useCameraVelocity = false;
        int sampleCount = 6;
        float strength = 0.6f;
        float maxVelocity = 0.035f;
    };

    bool Initialize(ID3D11Device* device_, int width_, int height_);
    void SynchronizeCameraHistory(const RenderContext& rc_);
    void ResetCameraHistory();
    void SetObjectVelocityRenderer(ObjectVelocityRenderer* objectVelocityRenderer_);
    void ClearVelocityBuffer(ID3D11DeviceContext* dc_);
    void ExecuteBlurFromCurrentVelocity(
        const RenderContext& rc_,
        ID3D11ShaderResourceView* sceneSRV_,
        ID3D11ShaderResourceView* depthSRV_,
        ID3D11RenderTargetView* outputRTV_,
        const Parameters& parameters_);

    ID3D11RenderTargetView* GetVelocityRTV() const { return velocityBuffer.rtv.Get(); }
    ID3D11ShaderResourceView* GetVelocitySRV() const { return velocityBuffer.srv.Get(); }
    int GetVelocityWidth() const { return width; }
    int GetVelocityHeight() const { return height; }

    void Execute(
        const RenderContext& rc_,
        ID3D11ShaderResourceView* sceneSRV_,
        ID3D11ShaderResourceView* depthSRV_,
        ID3D11RenderTargetView* outputRTV_,
        const Parameters& parameters_);

private:
    static constexpr int TILE_SIZE = 20;

    struct CbMotionBlur
    {
        DirectX::XMINT2 screenSize;
        DirectX::XMFLOAT2 screenPixelSize;
        DirectX::XMFLOAT4X4 invViewProjection;
        DirectX::XMFLOAT4X4 previousViewProjection;
        int enabled;
        int sampleCount;
        float strength;
        float maxVelocity;
    };

    struct RenderTarget
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    };

    bool CreateRenderTarget(
        ID3D11Device* device_,
        int width_,
        int height_,
        DXGI_FORMAT format_,
        RenderTarget& target_);
    void UpdateConstantBuffer(const RenderContext& rc_, const Parameters& parameters_);
    void UpdatePreviousViewProjection(const RenderContext& rc_);
    DirectX::XMFLOAT4X4 BuildViewProjection(const RenderContext& rc_) const;
    void DrawPass(
        ID3D11DeviceContext* dc_,
        ID3D11RenderTargetView* outputRTV_,
        ID3D11PixelShader* pixelShader_,
        ID3D11ShaderResourceView* const* shaderResources_,
        UINT shaderResourceCount_,
        int width_,
        int height_);
    void DrawFullScreenQuad(ID3D11DeviceContext* dc_);

    int width = 0;
    int height = 0;
    int tileWidth = 0;
    int tileHeight = 0;
    bool hasPreviousViewProjection = false;
    DirectX::XMFLOAT4X4 previousViewProjection{};
    ObjectVelocityRenderer* objectVelocityRenderer = nullptr;

    RenderTarget velocityBuffer;
    RenderTarget tileMaxBuffer;
    RenderTarget neighborMaxBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> velocityPixelShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> tileMaxPixelShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> neighborMaxPixelShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> fullScreenVs;
};
