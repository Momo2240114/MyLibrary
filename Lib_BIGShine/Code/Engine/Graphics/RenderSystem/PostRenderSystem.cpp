#include "PrecompiledHeader.h"

#include "PostRenderSystem.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"

bool PostRenderSystem::Initialize(ID3D11Device* device, int width, int height) {
    initialized = false;
    if (!device || width <= 0 || height <= 0) return false;
    // 各ポストエフェクトを同じ解像度で初期化し、必要な中間バッファも2枚確保する。
    if (!bloom.Initialize(device, width, height))
    {
        OutputDebugStringA("BloomSystem initialization failed.\n");
        return false;
    }
    if (!radialBlur.Initialize(device, width, height))
    {
        OutputDebugStringA("RadialBlurSystem initialization failed.\n");
        return false;
    }
    if (!xeGtao.Initialize(device, width, height))
    {
        OutputDebugStringA("XeGtaoSystem initialization failed.\n");
        return false;
    }
    if (!motionBlur.Initialize(device, width, height))
    {
        OutputDebugStringA("MotionBlurSystem initialization failed.\n");
        return false;
    }
    if (!vignette.Initialize(device, width, height))
    {
        OutputDebugStringA("VignetteSystem initialization failed.\n");
        return false;
    }
    if (!postDistortion.Initialize(device, width, height))
    {
        OutputDebugStringA("PostDistortionSystem initialization failed.\n");
        return false;
    }
    if (!CreatePostBuffer(device, width, height, workBuffers[0]) ||
        !CreatePostBuffer(device, width, height, workBuffers[1]))
    {
        OutputDebugStringA("PostRenderSystem work buffer creation failed.\n");
        return false;
    }
    fogAvailable = fog.Initialize(device, width, height);
    if (!fogAvailable)
    {
        fog.Shutdown();
        OutputDebugStringA("FogSystem initialization failed; fog is skipped.\n");
    }
    fxaaAvailable = fxaa.Initialize(device, width, height);
    if (!fxaaAvailable)
    {
        fxaa.Shutdown();
        OutputDebugStringA("FxaaSystem initialization failed; FXAA is skipped.\n");
    }
    initialized = true;
    return true;
}

void PostRenderSystem::Shutdown() {
    BS_PROFILE_SCOPE("PostRenderSystem::Shutdown");

    bloom.Shutdown();
    radialBlur.Shutdown();
    xeGtao.Shutdown();
    motionBlur.Shutdown();
    vignette.Shutdown();
    postDistortion.Shutdown();
    fog.Shutdown();
    fxaa.Shutdown();
    fogAvailable = false;
    fxaaAvailable = false;
    initialized = false;
    debugBloom = {};
    debugFog = {};
    debugVignette = {};
    debugFxaa = {};
    debugCaptureEnabled = false;

    for (PostBuffer& buffer_ : workBuffers)
    {
        buffer_.srv.Reset();
        buffer_.rtv.Reset();
        buffer_.texture.Reset();
    }
}

void PostRenderSystem::Execute(const RenderContext& rc, ID3D11ShaderResourceView* sceneSRV, ID3D11RenderTargetView* finalRTV)
{
    if (!initialized) return;
    const bool captureDebug_ = debugCaptureEnabled;
    debugCaptureEnabled = false;
    debugBloom.valid = false;
    debugFog.valid = false;
    debugVignette.valid = false;
    debugFxaa.valid = false;
    // GraphicsDeviceManagerに集約された現在のポストエフェクト設定を読み取る。
    auto& graphics_ = GraphicsDeviceManager::Instance();
    auto tracyGpuContext_ = graphics_.GetTracyGpuContext();
    auto& bloomParams_ = graphics_.GetBloomParams();
    auto& filterParams_ = graphics_.GetFilterParams();
    auto& radialBlurParams_ = graphics_.GetRadialBlurParams();
    auto& xeGtaoParams_ = graphics_.GetXeGtaoParams();
    auto& motionBlurParams_ = graphics_.GetMotionBlurParams();
    auto vignetteParams_ = graphics_.GetVignetteParams();
    vignetteParams_.damageOpacity = graphics_.GetDamageVignetteOpacity();
    const auto& fogParams_ = graphics_.GetFogParams();
    const auto& fxaaParams_ = graphics_.GetFxaaParams();

    const bool radialBlurEnabled_ = radialBlurParams_.blurRadius > 0.0001f && radialBlurParams_.blurSamplingCount > 0;
    const bool xeGtaoEnabled_ = xeGtaoParams_.enabled && xeGtaoParams_.intensity > 0.0f;
    const bool motionBlurEnabled_ = motionBlurParams_.enabled && motionBlurParams_.strength > 0.0f;
    const bool vignetteEnabled_ = (vignetteParams_.enabled && vignetteParams_.opacity > 0.0f) ||
        vignetteParams_.damageOpacity > 0.0f;
    const bool fogEnabled_ = fogAvailable && fogParams_.enabled && fogParams_.density > 0.0f &&
        graphics_.GetMainDepthSRV() != nullptr && rc.camera != nullptr;
    const bool fxaaEnabled_ = fxaaAvailable && fxaaParams_.enabled;
    const bool distortionEnabled_ = graphics_.IsSceneDistortionUsed() && graphics_.GetSceneDistortionSRV() != nullptr;

    if (!motionBlurEnabled_)
    {
        // MotionBlurを使っていない時も、次に有効化された瞬間の履歴飛びを防ぐため同期する。
        motionBlur.SynchronizeCameraHistory(rc);
    }

    if (!fogEnabled_ && !radialBlurEnabled_ && !xeGtaoEnabled_ && !motionBlurEnabled_ &&
        !vignetteEnabled_ && !distortionEnabled_ && !fxaaEnabled_)
    {
        {
            // BloomはColorFilterの最終合成も兼ねるので、他の効果が無くても最後に通す。
            BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Final Composite");
            bloom.Execute(
                rc,
                sceneSRV,
                finalRTV,
                bloomParams_,
                filterParams_);
            if (captureDebug_) CaptureDebugOutput(rc, finalRTV, debugBloom);
        }
        return;
    }

    {
        // 追加効果がある場合は、まずBloom/ColorFilter結果を作業バッファへ出す。
        BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Bloom");
        bloom.Execute(
            rc,
            sceneSRV,
            workBuffers[0].rtv.Get(),
            bloomParams_,
            filterParams_);
        if (captureDebug_) CaptureDebugOutput(rc, workBuffers[0].rtv.Get(), debugBloom);
    }

    ID3D11ShaderResourceView* currentSRV_ = workBuffers[0].srv.Get();
    int currentBufferIndex_ = 0;

    if (fogEnabled_)
    {
        const bool hasNextPass_ = radialBlurEnabled_ || xeGtaoEnabled_ || motionBlurEnabled_ ||
            vignetteEnabled_ || distortionEnabled_ || fxaaEnabled_;
        BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Fog");
        fog.Execute(rc, currentSRV_, graphics_.GetMainDepthSRV(),
            hasNextPass_ ? workBuffers[1].rtv.Get() : finalRTV, fogParams_);
        if (captureDebug_)
            CaptureDebugOutput(rc, hasNextPass_ ? workBuffers[1].rtv.Get() : finalRTV, debugFog);
        if (!hasNextPass_) return;
        currentSRV_ = workBuffers[1].srv.Get();
        currentBufferIndex_ = 1;
    }

    if (radialBlurEnabled_)
    {
        // 後続パスがある場合だけPing-Pong先へ出し、最後なら直接finalRTVへ書く。
        const bool hasNextPass_ = xeGtaoEnabled_ || motionBlurEnabled_ || vignetteEnabled_ || distortionEnabled_ || fxaaEnabled_;
        const int nextBufferIndex_ = currentBufferIndex_ == 0 ? 1 : 0;
        {
            BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::RadialBlur");
            radialBlur.Execute(
                rc,
                currentSRV_,
                hasNextPass_ ? workBuffers[nextBufferIndex_].rtv.Get() : finalRTV,
                radialBlurParams_);
        }

        if (!hasNextPass_)
        {
            return;
        }

        currentSRV_ = workBuffers[nextBufferIndex_].srv.Get();
        currentBufferIndex_ = nextBufferIndex_;
    }

    if (xeGtaoEnabled_)
    {
        // GTAOはDepth/Normalを参照して現在カラーへAOを合成する。
        const bool hasNextPass_ = motionBlurEnabled_ || vignetteEnabled_ || distortionEnabled_ || fxaaEnabled_;
        const int nextBufferIndex_ = currentBufferIndex_ == 0 ? 1 : 0;
        {
            BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::XeGTAO");
            xeGtao.Execute(
                rc,
                currentSRV_,
                graphics_.GetMainDepthSRV(),
                graphics_.GetGBufferNormalSRV(),
                hasNextPass_ ? workBuffers[nextBufferIndex_].rtv.Get() : finalRTV,
                xeGtaoParams_);
        }

        if (!hasNextPass_)
        {
            return;
        }

        currentSRV_ = workBuffers[nextBufferIndex_].srv.Get();
        currentBufferIndex_ = nextBufferIndex_;
    }

    if (motionBlurEnabled_)
    {
        // MotionBlurはDepthから前フレーム位置を復元するため、MainDepthを渡す。
        const bool hasNextPass_ = vignetteEnabled_ || distortionEnabled_ || fxaaEnabled_;
        const int nextBufferIndex_ = currentBufferIndex_ == 0 ? 1 : 0;
        {
            BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::MotionBlur");
            motionBlur.Execute(
                rc,
                currentSRV_,
                graphics_.GetMainDepthSRV(),
                hasNextPass_ ? workBuffers[nextBufferIndex_].rtv.Get() : finalRTV,
                motionBlurParams_);
        }

        if (!hasNextPass_)
        {
            return;
        }

        currentSRV_ = workBuffers[nextBufferIndex_].srv.Get();
        currentBufferIndex_ = nextBufferIndex_;
    }

    if (vignetteEnabled_)
    {
        // Vignette後にDistortionが残っていなければ、ここで最終RTへ確定する。
        const bool hasNextPass_ = distortionEnabled_ || fxaaEnabled_;
        const int nextBufferIndex_ = currentBufferIndex_ == 0 ? 1 : 0;
        {
            BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Vignette");
            vignette.Execute(
                rc,
                currentSRV_,
                hasNextPass_ ? workBuffers[nextBufferIndex_].rtv.Get() : finalRTV,
                vignetteParams_);
            if (captureDebug_)
                CaptureDebugOutput(rc, hasNextPass_ ? workBuffers[nextBufferIndex_].rtv.Get() : finalRTV, debugVignette);
        }

        if (!hasNextPass_)
        {
            return;
        }

        currentSRV_ = workBuffers[nextBufferIndex_].srv.Get();
        currentBufferIndex_ = nextBufferIndex_;
    }

    if (distortionEnabled_)
    {
        // Scene中の歪みマップを最後に適用し、最終カラーとして出力する。
        BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Final Composite");
        const int nextBufferIndex_ = currentBufferIndex_ == 0 ? 1 : 0;
        postDistortion.Execute(
            rc,
            currentSRV_,
            graphics_.GetSceneDistortionSRV(),
            fxaaEnabled_ ? workBuffers[nextBufferIndex_].rtv.Get() : finalRTV);
        if (!fxaaEnabled_) return;
        currentSRV_ = workBuffers[nextBufferIndex_].srv.Get();
    }

    if (fxaaEnabled_)
    {
        BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::FXAA");
        fxaa.Execute(rc, currentSRV_, finalRTV, fxaaParams_);
        if (captureDebug_) CaptureDebugOutput(rc, finalRTV, debugFxaa);
    }
}

void PostRenderSystem::CaptureDebugOutput(const RenderContext& rc_, ID3D11RenderTargetView* outputRTV_, DebugBuffer& buffer_)
{
    if (!rc_.device || !rc_.deviceContext || !outputRTV_) return;

    Microsoft::WRL::ComPtr<ID3D11Resource> sourceResource_;
    outputRTV_->GetResource(sourceResource_.GetAddressOf());
    Microsoft::WRL::ComPtr<ID3D11Texture2D> sourceTexture_;
    if (!sourceResource_ || FAILED(sourceResource_.As(&sourceTexture_))) return;

    D3D11_TEXTURE2D_DESC sourceDesc_{};
    sourceTexture_->GetDesc(&sourceDesc_);
    if (sourceDesc_.SampleDesc.Count != 1 || sourceDesc_.MipLevels != 1 || sourceDesc_.ArraySize != 1) return;

    bool recreate_ = !buffer_.texture || !buffer_.srv;
    if (!recreate_)
    {
        D3D11_TEXTURE2D_DESC currentDesc_{};
        buffer_.texture->GetDesc(&currentDesc_);
        recreate_ = currentDesc_.Width != sourceDesc_.Width || currentDesc_.Height != sourceDesc_.Height ||
            currentDesc_.Format != sourceDesc_.Format;
    }
    if (recreate_)
    {
        buffer_.texture.Reset();
        buffer_.srv.Reset();
        D3D11_TEXTURE2D_DESC copyDesc_ = sourceDesc_;
        copyDesc_.Usage = D3D11_USAGE_DEFAULT;
        copyDesc_.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        copyDesc_.CPUAccessFlags = 0;
        copyDesc_.MiscFlags = 0;
        if (FAILED(rc_.device->CreateTexture2D(&copyDesc_, nullptr, buffer_.texture.GetAddressOf()))) return;
        if (FAILED(rc_.device->CreateShaderResourceView(buffer_.texture.Get(), nullptr, buffer_.srv.GetAddressOf())))
        {
            buffer_.texture.Reset();
            return;
        }
    }

    // Ping-Pong先は後続パスで上書きされるため、表示用に描画直後の内容を保持する。
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> boundRTV_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> boundDSV_;
    rc_.deviceContext->OMGetRenderTargets(1, boundRTV_.GetAddressOf(), boundDSV_.GetAddressOf());
    rc_.deviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    rc_.deviceContext->CopyResource(buffer_.texture.Get(), sourceTexture_.Get());
    ID3D11RenderTargetView* previousRTV_ = boundRTV_.Get();
    rc_.deviceContext->OMSetRenderTargets(1, &previousRTV_, boundDSV_.Get());
    buffer_.valid = true;
}

bool PostRenderSystem::CreatePostBuffer(ID3D11Device* device, int width, int height, PostBuffer& outBuffer)
{
    // HDRカラーを維持するため、ポスト処理の作業バッファは16bit float RGBAで作る。
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = device->CreateTexture2D(&desc, nullptr, outBuffer.texture.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    hr = device->CreateRenderTargetView(outBuffer.texture.Get(), nullptr, outBuffer.rtv.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    hr = device->CreateShaderResourceView(outBuffer.texture.Get(), nullptr, outBuffer.srv.ReleaseAndGetAddressOf());
    return SUCCEEDED(hr);
}
