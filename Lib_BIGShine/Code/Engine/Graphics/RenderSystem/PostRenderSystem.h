// PostRenderSystem.h
#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include "Engine/Graphics/Core/RenderContext.h"
#include "BloomSystem.h"
#include "RadialBlurSystem.h"
#include "XeGtaoSystem.h"
#include "MotionBlurSystem.h"
#include "VignetteSystem.h"
#include "PostDistortionSystem.h"

class PostRenderSystem {
public:
    bool Initialize(ID3D11Device* device, int width, int height);
    void Shutdown();

    void Execute(const RenderContext& rc, ID3D11ShaderResourceView* sceneSRV, ID3D11RenderTargetView* finalRTV);

    BloomSystem& GetBloomSystem() { return bloom; }
    XeGtaoSystem& GetXeGtaoSystem() { return xeGtao; }
    MotionBlurSystem& GetMotionBlurSystem() { return motionBlur; }
    VignetteSystem& GetVignetteSystem() { return vignette; }
    PostDistortionSystem& GetPostDistortionSystem() { return postDistortion; }
private:
    struct PostBuffer {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    };

    bool CreatePostBuffer(ID3D11Device* device, int width, int height, PostBuffer& outBuffer);

    BloomSystem bloom;
    RadialBlurSystem radialBlur;
    XeGtaoSystem xeGtao;
    MotionBlurSystem motionBlur;
    VignetteSystem vignette;
    PostDistortionSystem postDistortion;
    PostBuffer workBuffers[2];
};