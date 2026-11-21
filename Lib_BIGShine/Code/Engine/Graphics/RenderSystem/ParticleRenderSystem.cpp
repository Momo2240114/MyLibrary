#include "PrecompiledHeader.h"

#include "ParticleRenderSystem.h"

#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Core/RenderContext.h"
#include <DirectXMath.h>

#include "Engine/Debug/TracyProfiler.h"

void ParticleRenderSystem::Initialize(ID3D11Device* device)
{
    // Emitter間で共有するShaderを先に初期化し、Componentごとの重複作成を避ける。
    ParticleEmitter::InitializeSharedShaders(device);

    // Camera行列など、全Particle Trackで共通に読むConstantBufferを作る。
    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = sizeof(CbScene);
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    device->CreateBuffer(&cbDesc, nullptr, sceneConstantBuffer.GetAddressOf());
}

void ParticleRenderSystem::PushSubmit(ParticleComponent* component)
{
    // 無効ComponentはQueueに積まず、このFrameの描画対象から外す。
    if (!component || !component->IsEnabled())
    {
        return;
    }
    drawInfos.push_back({ component });
}

void ParticleRenderSystem::UpdateAll(ID3D11DeviceContext*, float, const DirectX::XMMATRIX&, const std::vector<std::weak_ptr<ParticleComponent>>& components)
{
    // 毎FrameQueueを作り直し、破棄済み/無効化済みComponentが残らないようにする。
    drawInfos.clear();
    for (auto& wp : components)
    {
        auto comp = wp.lock();
        if (!comp || !comp->IsEnabled())
        {
            continue;
        }
        drawInfos.push_back({ comp.get() });
    }
}

void ParticleRenderSystem::ApplySceneConstants(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.deviceContext;
    if (!dc || !rc.camera || !sceneConstantBuffer)
    {
        return;
    }

    // Camera行列と逆行列を作り、Particle ShaderでBillboardやDepth復元に使えるようにする。
    CbScene cbScene{};
    DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&rc.camera->GetView());
    DirectX::XMMATRIX projection = DirectX::XMLoadFloat4x4(&rc.camera->GetProjection());
    DirectX::XMMATRIX viewProjection = view * projection;
    DirectX::XMMATRIX invViewProjection = DirectX::XMMatrixInverse(nullptr, viewProjection);
    DirectX::XMStoreFloat4x4(&cbScene.viewProjection, viewProjection);
    DirectX::XMStoreFloat4x4(&cbScene.invViewProjection, invViewProjection);

    const DirectX::XMFLOAT3& eye = rc.camera->GetEye();
    cbScene.cameraPosition = { eye.x, eye.y, eye.z, 0.0f };
    dc->UpdateSubresource(sceneConstantBuffer.Get(), 0, nullptr, &cbScene, 0, 0);

    // VS/GS/PSすべてから参照できるよう、同じScene CBをSlot7へBindする。
    ID3D11Buffer* sceneCB = sceneConstantBuffer.Get();
    dc->VSSetConstantBuffers(7, 1, &sceneCB);
    dc->GSSetConstantBuffers(7, 1, &sceneCB);
    dc->PSSetConstantBuffers(7, 1, &sceneCB);
}

void ParticleRenderSystem::RenderAll(const RenderContext& rc, bool isEditorWindow_ )
{
    BS_PROFILE_SCOPE("Render::Particle");
    ID3D11DeviceContext* dc = rc.deviceContext;
    if (!dc || !rc.camera || drawInfos.empty())
    {
        // 描画不能なフレームでも、1フレーム用のSubmit Queueは持ち越さない。
        drawInfos.clear();
        return;
    }
    auto tracyGpuContext_ = GraphicsDeviceManager::Instance().GetTracyGpuContext();
    BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Particle Draw Pass");

    {
        BS_PROFILE_SCOPE("ParticleRender::ApplySceneConstants");
        ApplySceneConstants(rc);
    }


    {
        BS_PROFILE_SCOPE("ParticleRender::SetupPipeline");

        // Particleは半透明描画が中心なので、DepthはTestだけ行い書き込みはしない。
        if (rc.layerDepthTarget)
        {
            auto* colorTarget_ = rc.layerColorTarget;
            dc->OMSetRenderTargets(colorTarget_ ? 1 : 0, colorTarget_ ? &colorTarget_ : nullptr, rc.layerDepthTarget);
        }
        dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(
            rc.layerDepthTarget && rc.layerCaptureMode != LayerCaptureMode::BLOOM_COLOR
            ? DepthState::TestAndWrite : DepthState::TestOnly), 0);
        dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullNone));

        // ParticleEmitterはPointListからGeometry Shader等でQuadへ展開する。
        dc->IASetInputLayout(nullptr);
        dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        ParticleEmitter::BindSharedRenderShaders(dc);

        ID3D11SamplerState* sampler_ = rc.renderState->GetSamplerState(SamplerState::LinearWrap);
        ID3D11SamplerState* samplerStates_[3] =
        {
            sampler_,
            sampler_,
            sampler_
        };
        dc->PSSetSamplers(0, _countof(samplerStates_), samplerStates_);
    }

    float blendFactor[4] = { 0, 0, 0, 0 };
    {
        BS_PROFILE_SCOPE("ParticleRender::Tracks");

        for (auto& info : drawInfos)
        {
            if (!info.component)
            {
                continue;
            }

            BS_PROFILE_SCOPE("ParticleRender::Component");

            for (auto& track : info.component->GetTracks())
            {
                if (!track.isStarted || !track.emitter)
                {
                    continue;
                }

                BS_PROFILE_SCOPE("ParticleRender::Track");

                const BlendState blendState_ = track.runtimeBlendMode == ParticleBlendMode::ADDITIVE
                    ? BlendState::AdditiveKeepAlpha
                    : (isEditorWindow_ ? BlendState::TransparencyKeepAlpha : BlendState::Transparency);

                {
                    BS_PROFILE_SCOPE("ParticleRender::SetBlendState");

                    // Trackごとに加算/通常透明を切り替えて描画する。
                    dc->OMSetBlendState(
                        rc.renderState->GetBlendState(blendState_), blendFactor, 0xffffffff);
                }

                {
                    BS_PROFILE_SCOPE("ParticleEmitter::Render");
                    track.emitter->Render(dc);
                }
            }
        }
    }

    {
        BS_PROFILE_SCOPE("ParticleRender::CleanupPipeline");

        // 後続描画へParticle用SRV/CB/Shader/BlendStateが漏れないよう、使ったSlotを明示的に戻す。
        ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr };
        dc->GSSetShaderResources(9, 2, nullSRVs);

        ID3D11ShaderResourceView* nullPSViews[] = { nullptr, nullptr, nullptr, nullptr };
        dc->PSSetShaderResources(0, _countof(nullPSViews), nullPSViews);
        ID3D11SamplerState* nullSamplerStates_[3] = {};
        dc->PSSetSamplers(0, _countof(nullSamplerStates_), nullSamplerStates_);

        ID3D11Buffer* nullCB = nullptr;
        dc->VSSetConstantBuffers(7, 1, &nullCB);
        dc->GSSetConstantBuffers(7, 1, &nullCB);
        dc->PSSetConstantBuffers(7, 1, &nullCB);
        dc->VSSetConstantBuffers(9, 1, &nullCB);
        dc->GSSetConstantBuffers(9, 1, &nullCB);
        dc->PSSetConstantBuffers(9, 1, &nullCB);

        dc->VSSetShader(nullptr, nullptr, 0);
        dc->GSSetShader(nullptr, nullptr, 0);
        dc->PSSetShader(nullptr, nullptr, 0);

        dc->OMSetBlendState(
            rc.renderState->GetBlendState(BlendState::Opaque), blendFactor, 0xffffffff);

        dc->OMSetDepthStencilState(
            rc.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);

        dc->RSSetState(
            rc.renderState->GetRasterizerState(RasterizerState::SolidCullBack));
    }

    {
        BS_PROFILE_SCOPE("ParticleRender::ClearDrawInfos");
        // Submit Queueは1Frame限りなので、描画後に破棄する。
        drawInfos.clear();
    }
}
