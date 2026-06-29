#include "ParticleRenderSystem.h"

#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Core/RenderContext.h"
#include <DirectXMath.h>

#include "Engine/Debug/TracyProfiler.h"

void ParticleRenderSystem::Initialize(ID3D11Device* device)
{
    ParticleEmitter::InitializeSharedShaders(device);

    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = sizeof(CbScene);
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    device->CreateBuffer(&cbDesc, nullptr, sceneConstantBuffer.GetAddressOf());
}

void ParticleRenderSystem::PushSubmit(ParticleComponent* component)
{
    if (!component || !component->IsEnabled()) return;
    drawInfos.push_back({ component });
}

void ParticleRenderSystem::UpdateAll(ID3D11DeviceContext*, float, const DirectX::XMMATRIX&, const std::vector<std::weak_ptr<ParticleComponent>>& components)
{
    drawInfos.clear();
    for (auto& wp : components)
    {
        auto comp = wp.lock();
        if (!comp || !comp->IsEnabled()) continue;
        drawInfos.push_back({ comp.get() });
    }
}

void ParticleRenderSystem::ApplySceneConstants(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.deviceContext;
    if (!dc || !rc.camera || !sceneConstantBuffer) return;

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

    ID3D11Buffer* sceneCB = sceneConstantBuffer.Get();
    dc->VSSetConstantBuffers(7, 1, &sceneCB);
    dc->GSSetConstantBuffers(7, 1, &sceneCB);
    dc->PSSetConstantBuffers(7, 1, &sceneCB);
}

void ParticleRenderSystem::RenderAll(const RenderContext& rc, bool isEditorWindow_ )
{
    ID3D11DeviceContext* dc = rc.deviceContext;
    if (!dc || !rc.camera || drawInfos.empty()) return;

    {
        BS_PROFILE_SCOPE("ParticleRender::ApplySceneConstants");
        ApplySceneConstants(rc);
    }


    {
        BS_PROFILE_SCOPE("ParticleRender::SetupPipeline");

        dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestOnly), 0);
        dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullNone));

        dc->IASetInputLayout(nullptr);
        dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
        ParticleEmitter::BindSharedRenderShaders(dc);

        ID3D11SamplerState* sampler = rc.renderState->GetSamplerState(SamplerState::LinearWrap);
        dc->PSSetSamplers(0, 1, &sampler);
    }

    float blendFactor[4] = { 0, 0, 0, 0 };
    {
        BS_PROFILE_SCOPE("ParticleRender::Tracks");

        for (auto& info : drawInfos)
        {
            if (!info.component) continue;

            BS_PROFILE_SCOPE("ParticleRender::Component");

            for (auto& track : info.component->GetTracks())
            {
                if (!track.isStarted || !track.emitter) continue;

                BS_PROFILE_SCOPE("ParticleRender::Track");

                const BlendState blendState_ = track.blendMode == ParticleBlendMode::ADDITIVE
                    ? BlendState::AdditiveKeepAlpha
                    : (isEditorWindow_ ? BlendState::TransparencyKeepAlpha : BlendState::Transparency);

                {
                    BS_PROFILE_SCOPE("ParticleRender::SetBlendState");

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

        ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr };
        dc->GSSetShaderResources(9, 2, nullSRVs);

        ID3D11ShaderResourceView* nullPSViews[] = { nullptr, nullptr, nullptr, nullptr };
        dc->PSSetShaderResources(0, _countof(nullPSViews), nullPSViews);

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
        drawInfos.clear();
    }
}
