// PostRenderSystem.h
#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include "Engine/Graphics/Core/RenderContext.h" // 既存のものを使用
#include "BloomSystem.h" 
#include "RadialBlurSystem.h" 


class PostRenderSystem {
public:
    bool Initialize(ID3D11Device* device, int width, int height);
    void Shutdown();

    // 順序立てて実行する
    void Execute(const RenderContext& rc, ID3D11ShaderResourceView* sceneSRV, ID3D11RenderTargetView* finalRTV);

    BloomSystem& GetBloomSystem() {return bloom;}
private:
    struct PostBuffer {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    };

    bool CreatePostBuffer(ID3D11Device* device, int width, int height, PostBuffer& outBuffer);

    // 各術式を内包する
    BloomSystem bloom;
    RadialBlurSystem radialBlur;
    PostBuffer workBuffers[2];

};

