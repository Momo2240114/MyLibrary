#include "PrecompiledHeader.h"
#include "Engine/Core/GameRuntimeSettings.h"

#include "PostRenderSystem.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"

#include <algorithm>
#include <filesystem>
#include <utility>

namespace
{
    struct LayerEffectConstants
    {
        DirectX::XMFLOAT4 color;
        uint32_t type;
        int sampleCount;
        float strength;
        float maxVelocity;
        DirectX::XMFLOAT4X4 inverseViewProjection;
        DirectX::XMFLOAT4X4 previousViewProjection;
        uint32_t useCameraVelocity;
        float padding[3];
    };
    static_assert(sizeof(LayerEffectConstants) == 176);
}

bool PostRenderSystem::Initialize(ID3D11Device* device, int width, int height) {
    initialized = false;
    if (!device || width <= 0 || height <= 0)
    {
        return false;
    }
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
    constexpr const char* LAYER_EFFECT_SHADER_PATH = "Data/Shader/LayerPostEffectPS.cso";
    constexpr const char* LAYER_MODEL_MASK_SHADER_PATH = "Data/Shader/LayerModelMaskPS.cso";
    std::error_code shaderPathError_;
    if (std::filesystem::exists(LAYER_EFFECT_SHADER_PATH, shaderPathError_) &&
        std::filesystem::exists(LAYER_MODEL_MASK_SHADER_PATH, shaderPathError_))
    {
        CPUResourceLoader::LoadVertexShader(device, "Data/Shader/FullScreenVS.cso",
            nullptr, 0, nullptr, layerEffectVertexShader.ReleaseAndGetAddressOf());
        CPUResourceLoader::LoadPixelShader(device, LAYER_EFFECT_SHADER_PATH,
            layerEffectPixelShader.ReleaseAndGetAddressOf());
        CPUResourceLoader::LoadPixelShader(device, LAYER_MODEL_MASK_SHADER_PATH,
            layerModelMaskShader.ReleaseAndGetAddressOf());
        CPUResourceLoader::CreateConstantBuffer(device, sizeof(LayerEffectConstants),
            layerEffectConstants.ReleaseAndGetAddressOf());
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
    layerEffectVertexShader.Reset();
    layerEffectPixelShader.Reset();
    layerModelMaskShader.Reset();
    layerEffectConstants.Reset();
    layerDepthDSV.Reset();
    layerDepthSRV.Reset();
    layerDepthWidth = 0;
    layerDepthHeight = 0;
    fogAvailable = false;
    fxaaAvailable = false;
    initialized = false;
    debugBloom = {};
    debugFog = {};
    debugVignette = {};
    debugFxaa = {};
    layerMotionSource = {};
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
    if (!initialized)
    {
        return;
    }
    const bool captureDebug_ = debugCaptureEnabled;
    debugCaptureEnabled = false;
    debugBloom.valid = false;
    debugFog.valid = false;
    debugVignette.valid = false;
    debugFxaa.valid = false;
    // GraphicsDeviceManagerに集約された現在のポストエフェクト設定を読み取る。
    auto& graphics_ = GraphicsDeviceManager::Instance();
    auto tracyGpuContext_ = graphics_.GetTracyGpuContext();
    auto bloomParams_ = graphics_.GetBloomParams();
    bloomParams_.exposure *= GameRuntimeSettings::GetUserPreferences().brightness;
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
            if (captureDebug_)
            {
                CopyRenderOutput(rc, finalRTV, debugBloom);
            }
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
        if (captureDebug_)
        {
            CopyRenderOutput(rc, workBuffers[0].rtv.Get(), debugBloom);
        }
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
        {
            CopyRenderOutput(rc, hasNextPass_ ? workBuffers[1].rtv.Get() : finalRTV, debugFog);
        }
        if (!hasNextPass_)
        {
            return;
        }
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
            {
                CopyRenderOutput(rc, hasNextPass_ ? workBuffers[nextBufferIndex_].rtv.Get() : finalRTV, debugVignette);
            }
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
        if (!fxaaEnabled_)
        {
            return;
        }
        currentSRV_ = workBuffers[nextBufferIndex_].srv.Get();
    }

    if (fxaaEnabled_)
    {
        BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::FXAA");
        fxaa.Execute(rc, currentSRV_, finalRTV, fxaaParams_);
        if (captureDebug_)
        {
            CopyRenderOutput(rc, finalRTV, debugFxaa);
        }
    }
}

bool PostRenderSystem::EnsureLayerDepth(const RenderContext& rc_)
{
    Microsoft::WRL::ComPtr<ID3D11Resource> sceneDepthResource_;
    rc_.depthSRV->GetResource(sceneDepthResource_.GetAddressOf());
    Microsoft::WRL::ComPtr<ID3D11Texture2D> sceneDepthTexture_;
    if (FAILED(sceneDepthResource_.As(&sceneDepthTexture_)))
    {
        return false;
    }

    D3D11_TEXTURE2D_DESC desc_{};
    sceneDepthTexture_->GetDesc(&desc_);
    if (desc_.SampleDesc.Count != 1)
    {
        return false;
    }
    if (layerDepthDSV && layerDepthWidth == desc_.Width && layerDepthHeight == desc_.Height)
    {
        return true;
    }

    desc_.MipLevels = 1;
    desc_.ArraySize = 1;
    desc_.Format = DXGI_FORMAT_R24G8_TYPELESS;
    desc_.Usage = D3D11_USAGE_DEFAULT;
    desc_.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    desc_.CPUAccessFlags = 0;
    desc_.MiscFlags = 0;

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc_{};
    dsvDesc_.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc_.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc_{};
    srvDesc_.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc_.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc_.Texture2D.MipLevels = 1;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_;
    if (FAILED(rc_.device->CreateTexture2D(&desc_, nullptr, depth_.GetAddressOf())) ||
        FAILED(rc_.device->CreateDepthStencilView(depth_.Get(), &dsvDesc_, dsv_.GetAddressOf())) ||
        FAILED(rc_.device->CreateShaderResourceView(depth_.Get(), &srvDesc_, srv_.GetAddressOf())))
    {
        return false;
    }

    layerDepthDSV = std::move(dsv_);
    layerDepthSRV = std::move(srv_);
    layerDepthWidth = desc_.Width;
    layerDepthHeight = desc_.Height;
    return true;
}

void PostRenderSystem::ExecuteLayerEffects(const RenderContext& rc_, ID3D11RenderTargetView* finalRTV_,
    const std::vector<LayerPostEffectSettings>& effects_,
    const DirectX::XMFLOAT4X4* previousViewProjection_,
    const std::function<bool(const LayerPostEffectSettings&, const RenderContext&, DirectX::XMFLOAT4&)>& drawLayer_)
{
    const auto isActive_ = [previousViewProjection_](const LayerPostEffectSettings& effect_)
    {
        if (!effect_.enabled)
        {
            return false;
        }
        switch (effect_.type)
        {
        case LayerPostEffectType::BLOOM:
            return effect_.bloom.intensity > 0.0f && effect_.bloom.blurPasses > 0;
        case LayerPostEffectType::MOTION_BLUR:
            return effect_.motionBlur.strength > 0.0f &&
                (!effect_.motionBlur.useCameraVelocity || previousViewProjection_ != nullptr);
        default:
            return effect_.color.w > 0.0f;
        }
    };
    if (!initialized || !IsLayerPostEffectAvailable() || !finalRTV_ || !rc_.device ||
        !rc_.deviceContext || !rc_.renderState || !rc_.camera || !rc_.depthSRV ||
        std::none_of(effects_.begin(), effects_.end(), isActive_) || !EnsureLayerDepth(rc_))
    {
        return;
    }

    BS_PROFILE_SCOPE("PostEffect::LayerEffects");
    auto tracyGpuContext_ = GraphicsDeviceManager::Instance().GetTracyGpuContext();
    BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Layer Post Effects");

    auto* dc_ = rc_.deviceContext;
    D3D11_VIEWPORT outputViewport_{};
    UINT viewportCount_ = 1;
    dc_->RSGetViewports(&viewportCount_, &outputViewport_);
    if (viewportCount_ == 0)
    {
        return;
    }

    const D3D11_VIEWPORT maskViewport_{
        0.0f, 0.0f, static_cast<float>(layerDepthWidth), static_cast<float>(layerDepthHeight), 0.0f, 1.0f };
    RenderContext maskContext_ = rc_;
    maskContext_.layerModelMaskShader = layerModelMaskShader.Get();
    maskContext_.layerPixelSize = { 1.0f / maskViewport_.Width, 1.0f / maskViewport_.Height };

    const auto drawEffect_ = [&](const LayerPostEffectSettings& effect_)
    {
        if (!isActive_(effect_))
        {
            return;
        }

        const bool silhouette_ = effect_.type == LayerPostEffectType::OCCLUDED_SILHOUETTE;
        const bool bloom_ = effect_.type == LayerPostEffectType::BLOOM;
        const bool motion_ = effect_.type == LayerPostEffectType::MOTION_BLUR;
        const bool modelMotion_ = motion_ && !effect_.motionBlur.useCameraVelocity;
        maskContext_.layerDepthTarget = bloom_
            ? GraphicsDeviceManager::Instance().GetDepthStencilView() : layerDepthDSV.Get();
        maskContext_.layerColorTarget = bloom_ || modelMotion_ ? workBuffers[0].rtv.Get() : nullptr;
        maskContext_.layerCaptureMode = bloom_ ? LayerCaptureMode::BLOOM_COLOR :
            (modelMotion_ ? LayerCaptureMode::MODEL_VELOCITY : LayerCaptureMode::DEPTH);

        // Bloomの抽出元は対象レイヤーを再描画し、背景色が抽出へ混ざらないようにする。
        dc_->RSSetViewports(1, &maskViewport_);
        if (!bloom_)
        {
            dc_->ClearDepthStencilView(layerDepthDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
        }
        if (maskContext_.layerColorTarget)
        {
            const float clearColor_[4]{};
            dc_->ClearRenderTargetView(maskContext_.layerColorTarget, clearColor_);
        }
        DirectX::XMFLOAT4 outlineColor_{};
        if (!drawLayer_(effect_, maskContext_, outlineColor_))
        {
            return;
        }

        // 全画面パスで読むマスク・カラーを先に出力先から外す。
        dc_->OMSetRenderTargets(0, nullptr, nullptr);
        ID3D11ShaderResourceView* layerInput_ = nullptr;
        ID3D11ShaderResourceView* motionSource_ = nullptr;
        LayerEffectConstants constants_{};
        constants_.type = static_cast<uint32_t>(effect_.type);
        constants_.color = effect_.color;
        if (bloom_)
        {
            layerInput_ = bloom.ExtractGlow(rc_, workBuffers[0].srv.Get(), effect_.bloom);
            if (!layerInput_)
            {
                return;
            }
            constants_.strength = effect_.bloom.intensity;
        }
        else if (motion_)
        {
            layerMotionSource.valid = false;
            CopyRenderOutput(rc_, finalRTV_, layerMotionSource);
            if (!layerMotionSource.valid)
            {
                return;
            }
            motionSource_ = layerMotionSource.srv.Get();
            layerInput_ = modelMotion_ ? workBuffers[0].srv.Get() : nullptr;
            constants_.strength = effect_.motionBlur.strength;
            constants_.maxVelocity = effect_.motionBlur.maxVelocity;
            constants_.sampleCount = effect_.motionBlur.sampleCount;
            if (!modelMotion_)
            {
                const auto currentViewProjection_ = DirectX::XMLoadFloat4x4(&rc_.camera->GetView()) *
                    DirectX::XMLoadFloat4x4(&rc_.camera->GetProjection());
                DirectX::XMStoreFloat4x4(&constants_.inverseViewProjection,
                    DirectX::XMMatrixInverse(nullptr, currentViewProjection_));
                constants_.previousViewProjection = *previousViewProjection_;
                constants_.useCameraVelocity = 1u;
            }
        }
        else if (silhouette_)
        {
            if (effect_.useOutlineColor)
            {
                constants_.color = { outlineColor_.x, outlineColor_.y, outlineColor_.z, effect_.color.w };
            }
            // シーンの最前面に見えている装備だけを記録し、敵や地形の後ろにある装備では除外しない。
            RenderContext exclusionContext_ = maskContext_;
            exclusionContext_.layerDepthTarget = GraphicsDeviceManager::Instance().GetDepthStencilView();
            exclusionContext_.layerColorTarget = workBuffers[0].rtv.Get();
            exclusionContext_.layerCaptureMode = LayerCaptureMode::SILHOUETTE_EXCLUSION;
            const float clearExclusion_[4]{};
            dc_->ClearRenderTargetView(exclusionContext_.layerColorTarget, clearExclusion_);
            DirectX::XMFLOAT4 unusedOutlineColor_{};
            drawLayer_(effect_, exclusionContext_, unusedOutlineColor_);
            layerInput_ = workBuffers[0].srv.Get();
        }
        dc_->UpdateSubresource(layerEffectConstants.Get(), 0, nullptr, &constants_, 0, 0);

        dc_->OMSetRenderTargets(1, &finalRTV_, nullptr);
        dc_->RSSetViewports(1, &outputViewport_);
        dc_->RSSetState(rc_.renderState->GetRasterizerState(RasterizerState::SolidCullNone));
        dc_->OMSetDepthStencilState(rc_.renderState->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
        dc_->OMSetBlendState(rc_.renderState->GetBlendState(
            bloom_ ? BlendState::AdditiveKeepAlpha : BlendState::TransparencyKeepAlpha), nullptr, 0xFFFFFFFF);
        ID3D11SamplerState* samplers_[]{
            rc_.renderState->GetSamplerState(SamplerState::LinearClamp),
            rc_.renderState->GetSamplerState(SamplerState::PointClamp) };
        dc_->PSSetSamplers(0, _countof(samplers_), samplers_);
        dc_->IASetInputLayout(nullptr);
        dc_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        dc_->VSSetShader(layerEffectVertexShader.Get(), nullptr, 0);
        dc_->GSSetShader(nullptr, nullptr, 0);
        dc_->PSSetShader(layerEffectPixelShader.Get(), nullptr, 0);
        dc_->PSSetConstantBuffers(0, 1, layerEffectConstants.GetAddressOf());
        ID3D11ShaderResourceView* srvs_[]{ bloom_ ? nullptr : layerDepthSRV.Get(),
            bloom_ ? nullptr : rc_.depthSRV, layerInput_, motionSource_ };
        dc_->PSSetShaderResources(0, _countof(srvs_), srvs_);
        dc_->Draw(3, 0);
        ID3D11ShaderResourceView* nullSrvs_[4]{};
        dc_->PSSetShaderResources(0, _countof(nullSrvs_), nullSrvs_);
    };
    // 遮蔽シルエットは、発光やブラーを重ねた後も指定色で読めるよう最後に描く。
    for (const bool silhouettePass_ : { false, true })
    {
        for (const auto& effect_ : effects_)
        {
            if ((effect_.type == LayerPostEffectType::OCCLUDED_SILHOUETTE) == silhouettePass_)
            {
                drawEffect_(effect_);
            }
        }
    }
    dc_->OMSetRenderTargets(1, &finalRTV_, nullptr);
    dc_->RSSetViewports(1, &outputViewport_);
}

void PostRenderSystem::CopyRenderOutput(const RenderContext& rc_, ID3D11RenderTargetView* outputRTV_, OutputCopy& buffer_)
{
    if (!rc_.device || !rc_.deviceContext || !outputRTV_)
    {
        return;
    }

    Microsoft::WRL::ComPtr<ID3D11Resource> sourceResource_;
    outputRTV_->GetResource(sourceResource_.GetAddressOf());
    Microsoft::WRL::ComPtr<ID3D11Texture2D> sourceTexture_;
    if (!sourceResource_ || FAILED(sourceResource_.As(&sourceTexture_)))
    {
        return;
    }

    D3D11_TEXTURE2D_DESC sourceDesc_{};
    sourceTexture_->GetDesc(&sourceDesc_);
    if (sourceDesc_.SampleDesc.Count != 1 || sourceDesc_.MipLevels != 1 || sourceDesc_.ArraySize != 1)
    {
        return;
    }

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
        if (FAILED(rc_.device->CreateTexture2D(&copyDesc_, nullptr, buffer_.texture.GetAddressOf())))
        {
            return;
        }
        if (FAILED(rc_.device->CreateShaderResourceView(buffer_.texture.Get(), nullptr, buffer_.srv.GetAddressOf())))
        {
            buffer_.texture.Reset();
            return;
        }
    }

    // 後続パスで出力先を書き換えながら読み戻せるよう、別のテクスチャへコピーする。
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
    if (FAILED(hr))
    {
        return false;
    }

    hr = device->CreateRenderTargetView(outBuffer.texture.Get(), nullptr, outBuffer.rtv.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        return false;
    }

    hr = device->CreateShaderResourceView(outBuffer.texture.Get(), nullptr, outBuffer.srv.ReleaseAndGetAddressOf());
    return SUCCEEDED(hr);
}
