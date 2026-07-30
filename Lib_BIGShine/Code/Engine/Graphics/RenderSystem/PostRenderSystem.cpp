#include "PrecompiledHeader.h"

#include "PostRenderSystem.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"

bool PostRenderSystem::Initialize(ID3D11Device* device, int width, int height) {
    if (!bloom.Initialize(device, width, height)) return false;
    if (!radialBlur.Initialize(device, width, height)) return false;
    if (!xeGtao.Initialize(device, width, height)) return false;
    if (!motionBlur.Initialize(device, width, height)) return false;
    if (!vignette.Initialize(device, width, height)) return false;
    if (!postDistortion.Initialize(device, width, height)) return false;
    if (!CreatePostBuffer(device, width, height, workBuffers[0])) return false;
    if (!CreatePostBuffer(device, width, height, workBuffers[1])) return false;
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

    for (PostBuffer& buffer_ : workBuffers)
    {
        buffer_.srv.Reset();
        buffer_.rtv.Reset();
        buffer_.texture.Reset();
    }
}

void PostRenderSystem::Execute(const RenderContext& rc, ID3D11ShaderResourceView* sceneSRV, ID3D11RenderTargetView* finalRTV)
{
    auto& graphics_ = GraphicsDeviceManager::Instance();
    auto tracyGpuContext_ = graphics_.GetTracyGpuContext();
    auto& bloomParams_ = graphics_.GetBloomParams();
    auto& filterParams_ = graphics_.GetFilterParams();
    auto& radialBlurParams_ = graphics_.GetRadialBlurParams();
    auto& xeGtaoParams_ = graphics_.GetXeGtaoParams();
    auto& motionBlurParams_ = graphics_.GetMotionBlurParams();
    auto& vignetteParams_ = graphics_.GetVignetteParams();

    const bool radialBlurEnabled_ = radialBlurParams_.blurRadius > 0.0001f && radialBlurParams_.blurSamplingCount > 0;
    const bool xeGtaoEnabled_ = xeGtaoParams_.enabled && xeGtaoParams_.intensity > 0.0f;
    const bool motionBlurEnabled_ = motionBlurParams_.enabled && motionBlurParams_.strength > 0.0f;
    const bool vignetteEnabled_ = vignetteParams_.enabled && vignetteParams_.opacity > 0.0f;
    const bool distortionEnabled_ = graphics_.IsSceneDistortionUsed() && graphics_.GetSceneDistortionSRV() != nullptr;

    if (!motionBlurEnabled_)
    {
        motionBlur.SynchronizeCameraHistory(rc);
    }

    if (!radialBlurEnabled_ && !xeGtaoEnabled_ && !motionBlurEnabled_ && !vignetteEnabled_ && !distortionEnabled_)
    {
        {
            BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Final Composite");
            bloom.Execute(
                rc,
                sceneSRV,
                finalRTV,
                bloomParams_,
                filterParams_);
        }
        return;
    }

    {
        BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Bloom");
        bloom.Execute(
            rc,
            sceneSRV,
            workBuffers[0].rtv.Get(),
            bloomParams_,
            filterParams_);
    }

    ID3D11ShaderResourceView* currentSRV_ = workBuffers[0].srv.Get();
    int currentBufferIndex_ = 0;

    if (radialBlurEnabled_)
    {
        const bool hasNextPass_ = xeGtaoEnabled_ || motionBlurEnabled_ || vignetteEnabled_ || distortionEnabled_;
        {
            BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::RadialBlur");
            radialBlur.Execute(
                rc,
                currentSRV_,
                hasNextPass_ ? workBuffers[1].rtv.Get() : finalRTV,
                radialBlurParams_);
        }

        if (!hasNextPass_)
        {
            return;
        }

        currentSRV_ = workBuffers[1].srv.Get();
        currentBufferIndex_ = 1;
    }

    if (xeGtaoEnabled_)
    {
        const bool hasNextPass_ = motionBlurEnabled_ || vignetteEnabled_ || distortionEnabled_;
        const int nextBufferIndex_ = currentBufferIndex_ == 0 ? 1 : 0;
        {
            BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::XeGTAO");
            xeGtao.Execute(
                rc,
                currentSRV_,
                graphics_.GetMainDepthSRV(),
                graphics_.GetSceneNormalSRV(),
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
        const bool hasNextPass_ = vignetteEnabled_ || distortionEnabled_;
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
        const bool hasNextPass_ = distortionEnabled_;
        const int nextBufferIndex_ = currentBufferIndex_ == 0 ? 1 : 0;
        {
            BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Vignette");
            vignette.Execute(
                rc,
                currentSRV_,
                hasNextPass_ ? workBuffers[nextBufferIndex_].rtv.Get() : finalRTV,
                vignetteParams_);
        }

        if (!hasNextPass_)
        {
            return;
        }

        currentSRV_ = workBuffers[nextBufferIndex_].srv.Get();
    }

    if (distortionEnabled_)
    {
        BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Final Composite");
        postDistortion.Execute(
            rc,
            currentSRV_,
            graphics_.GetSceneDistortionSRV(),
            finalRTV);
    }
}

bool PostRenderSystem::CreatePostBuffer(ID3D11Device* device, int width, int height, PostBuffer& outBuffer)
{
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
