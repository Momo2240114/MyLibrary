#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include "Engine/Graphics/Core/RenderContext.h"

class XeGtaoSystem
{
public:
    struct Parameters
    {
        bool enabled = false;
        bool showOnly = false;
        float radius = 1.0f;
        float intensity = 1.0f;
        float power = 1.5f;
        float sampleRadius = 16.0f;
        int sampleCount = 4;
        int updateInterval = 3;
    };

    bool Initialize(ID3D11Device* device_, int width_, int height_);

    void Execute(
        const RenderContext& rc_,
        ID3D11ShaderResourceView* sceneSRV_,
        ID3D11ShaderResourceView* depthSRV_,
        ID3D11ShaderResourceView* normalSRV_,
        ID3D11RenderTargetView* outputRTV_,
        const Parameters& parameters_);

    ID3D11ShaderResourceView* GetAoSRV() const { return aoSrv.Get(); }

private:
    struct CbXeGtao
    {
        DirectX::XMINT2 viewportSize;
        DirectX::XMFLOAT2 viewportPixelSize;
        DirectX::XMFLOAT4X4 invViewProjection;
        float radius;
        float intensity;
        float power;
        float sampleRadius;
        int sampleCount;
        int frameIndex;
        int enabled;
        int showOnly;
    };

    bool CreateAoTarget(ID3D11Device* device_, int width_, int height_);
    void UpdateConstantBuffer(const RenderContext& rc_, const Parameters& parameters_);
    void DrawFullScreenQuad(ID3D11DeviceContext* dc_);

private:
    int width = 0;
    int height = 0;
    int aoWidth = 0;
    int aoHeight = 0;
    int frameIndex = 0;
    bool aoReady = false;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> aoTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> aoSrv;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> aoUav;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> xeGtaoCs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> compositePs;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> fullScreenVs;
};
