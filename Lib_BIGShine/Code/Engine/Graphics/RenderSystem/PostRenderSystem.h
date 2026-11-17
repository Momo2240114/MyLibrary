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
#include "FogSystem.h"
#include "FxaaSystem.h"
#include "PostDistortionSystem.h"

class PostRenderSystem {
public:
    // 画面解像度に合わせた作業バッファと各PostEffectを初期化する。
    bool Initialize(ID3D11Device* device, int width, int height);
    void Shutdown();

    // sceneSRVへBloom/ColorFilterを通した後、有効なPostEffectだけをPing-PongでつないでfinalRTVへ出力する。
    void Execute(const RenderContext& rc, ID3D11ShaderResourceView* sceneSRV, ID3D11RenderTargetView* finalRTV);

    BloomSystem& GetBloomSystem() { return bloom; }
    XeGtaoSystem& GetXeGtaoSystem() { return xeGtao; }
    MotionBlurSystem& GetMotionBlurSystem() { return motionBlur; }
    VignetteSystem& GetVignetteSystem() { return vignette; }
    PostDistortionSystem& GetPostDistortionSystem() { return postDistortion; }
    bool IsFogAvailable() const { return fogAvailable; }
    bool IsFxaaAvailable() const { return fxaaAvailable; }
    void SetDebugCaptureEnabled(bool enabled_) { debugCaptureEnabled = enabled_; }
    ID3D11ShaderResourceView* GetBloomCompositeDebugSRV() const { return debugBloom.valid ? debugBloom.srv.Get() : nullptr; }
    ID3D11ShaderResourceView* GetFogDebugSRV() const { return debugFog.valid ? debugFog.srv.Get() : nullptr; }
    ID3D11ShaderResourceView* GetVignetteDebugSRV() const { return debugVignette.valid ? debugVignette.srv.Get() : nullptr; }
    ID3D11ShaderResourceView* GetFxaaDebugSRV() const { return debugFxaa.valid ? debugFxaa.srv.Get() : nullptr; }
    ID3D11ShaderResourceView* GetWorkBufferSRV(int index_) const
    {
        // Debug表示などで内部Ping-Pongバッファを参照できるよう、範囲外はnullptrで返す。
        if (index_ < 0 || index_ >= 2) return nullptr;
        return workBuffers[index_].srv.Get();
    }
private:
    // PostEffect間で使い回す中間RenderTarget。RTV/SRVを両方持ち、描画先と次Effect入力を切り替える。
    struct PostBuffer {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    };

    struct DebugBuffer {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        bool valid = false;
    };

    bool CreatePostBuffer(ID3D11Device* device, int width, int height, PostBuffer& outBuffer);
    void CaptureDebugOutput(const RenderContext& rc_, ID3D11RenderTargetView* outputRTV_, DebugBuffer& buffer_);

    // 個別Effectの設定値は各Systemが保持し、このクラスは実行順と中間バッファ管理を担当する。
    BloomSystem bloom;
    RadialBlurSystem radialBlur;
    XeGtaoSystem xeGtao;
    MotionBlurSystem motionBlur;
    VignetteSystem vignette;
    PostDistortionSystem postDistortion;
    FogSystem fog;
    FxaaSystem fxaa;
    bool fogAvailable = false;
    bool fxaaAvailable = false;
    bool initialized = false;
    // 2枚の作業バッファをPing-Pongして、複数PostEffectを余計なコピーなしで連結する。
    PostBuffer workBuffers[2];
    DebugBuffer debugBloom;
    DebugBuffer debugFog;
    DebugBuffer debugVignette;
    DebugBuffer debugFxaa;
    bool debugCaptureEnabled = false;
};
