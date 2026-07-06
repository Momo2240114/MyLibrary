#pragma once

#include <d3d11.h>
#include <wrl/client.h>

class EditorRenderTarget
{
public:
    bool Initialize(ID3D11Device* device_, int width_, int height_);
    bool Resize(ID3D11Device* device_, int width_, int height_);
    void Begin(ID3D11DeviceContext* context_, const float clearColor_[4]);
    void End(ID3D11DeviceContext* context_);

    ID3D11ShaderResourceView* GetShaderResourceView() const { return shaderResourceView.Get(); }
    ID3D11DepthStencilView* GetDepthStencilView() const { return depthStencilView.Get(); }
    ID3D11ShaderResourceView* GetDepthShaderResourceView() const { return depthShaderResourceView.Get(); }

    int GetWidth() const { return width; }
    int GetHeight() const { return height; }
    bool IsValid() const 
    {
        return renderTargetView && normalRenderTargetView && shaderResourceView && depthStencilView;
    }

    ID3D11RenderTargetView* GetRenderTargetView() const { return renderTargetView.Get(); }
    ID3D11RenderTargetView* GetNormalRenderTargetView() const { return normalRenderTargetView.Get(); }
    ID3D11ShaderResourceView* GetNormalShaderResourceView() const { return normalShaderResourceView.Get(); }
    void ResetRenderTarget(ID3D11DeviceContext* context_) const
    {
        if (!context_ || !IsValid()) return;
        // パーティクル用は1枚でOK
        ID3D11RenderTargetView* rtv_ = renderTargetView.Get();
        context_->OMSetRenderTargets(1, &rtv_, depthStencilView.Get());
    }
private:
    bool CreateNormalTarget(ID3D11Device* device_);


    void Reset();
    bool CreateColorTarget(ID3D11Device* device_);
    bool CreateDepthTarget(ID3D11Device* device_);

private:
    int width = 0;
    int height = 0;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> colorTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depthShaderResourceView;

    Microsoft::WRL::ComPtr<ID3D11Texture2D>           normalTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>    normalRenderTargetView;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  normalShaderResourceView;
};
