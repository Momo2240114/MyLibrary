#include "MeshVFXRenderSystem.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Graphics/Core/RenderContext.h"
#include "Engine/Graphics/Resource/ModelResource.h"
#include "MeshVFXComponent.h"

#include <algorithm>
#include <cmath>

namespace
{
    struct PartAnimationState
    {
        DirectX::XMFLOAT3 rotation;
        DirectX::XMFLOAT3 scale;
        float time = 0.0f;
        float alpha = 1.0f;
        bool active = true;
    };

    float Lerp(float start_, float end_, float amount_)
    {
        return start_ + (end_ - start_) * amount_;
    }

    DirectX::XMFLOAT3 Lerp(
        const DirectX::XMFLOAT3& start_,
        const DirectX::XMFLOAT3& end_,
        float amount_)
    {
        return
        {
            Lerp(start_.x, end_.x, amount_),
            Lerp(start_.y, end_.y, amount_),
            Lerp(start_.z, end_.z, amount_),
        };
    }

    PartAnimationState CalculatePartAnimation(const MeshVFXPart& part_, float componentTime_)
    {
        PartAnimationState state_{};
        state_.rotation = part_.rotation;
        state_.scale = part_.scale;
        state_.alpha = part_.alpha;

        float normalizedTime_ = 0.0f;
        if (part_.duration > 0.0f)
        {
            if (!part_.loop && componentTime_ >= part_.duration)
            {
                state_.active = false;
                return state_;
            }
            state_.time = part_.loop
                ? std::fmod((std::max)(componentTime_, 0.0f), part_.duration)
                : std::clamp(componentTime_, 0.0f, part_.duration);
            normalizedTime_ = state_.time / part_.duration;
        }
        else
        {
            state_.time = (std::max)(componentTime_, 0.0f);
        }

        state_.rotation.x += part_.meshRotationSpeed.x * state_.time;
        state_.rotation.y += part_.meshRotationSpeed.y * state_.time;
        state_.rotation.z += part_.meshRotationSpeed.z * state_.time;
        if (part_.animateScale) state_.scale = Lerp(part_.scaleStart, part_.scaleEnd, normalizedTime_);
        if (part_.animateAlpha) state_.alpha *= Lerp(part_.alphaStart, part_.alphaEnd, normalizedTime_);
        return state_;
    }

    DirectX::XMMATRIX CreatePartTransform(
        const MeshVFXPart& part_,
        const PartAnimationState& animation_)
    {
        const DirectX::XMVECTOR rotation_ = DirectX::XMQuaternionRotationRollPitchYaw(
            DirectX::XMConvertToRadians(animation_.rotation.x),
            DirectX::XMConvertToRadians(animation_.rotation.y),
            DirectX::XMConvertToRadians(animation_.rotation.z));
        return DirectX::XMMatrixScaling(animation_.scale.x, animation_.scale.y, animation_.scale.z) *
            DirectX::XMMatrixRotationQuaternion(rotation_) *
            DirectX::XMMatrixTranslation(part_.position.x, part_.position.y, part_.position.z);
    }

    DirectX::XMMATRIX CreateNodeTransform(const GltfModelCommponennt::Node* node_)
    {
        DirectX::XMMATRIX transform_ = DirectX::XMMatrixIdentity();
        const GltfModelCommponennt::Node* currentNode_ = node_;
        while (currentNode_)
        {
            const DirectX::XMMATRIX local_ =
                DirectX::XMMatrixScaling(currentNode_->scale.x, currentNode_->scale.y, currentNode_->scale.z) *
                DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&currentNode_->rotation)) *
                DirectX::XMMatrixTranslation(
                    currentNode_->position.x, currentNode_->position.y, currentNode_->position.z);
            transform_ *= local_;
            currentNode_ = currentNode_->parent;
        }
        return transform_;
    }
}

bool MeshVFXRenderSystem::Initialize(ID3D11Device* device_)
{
    shader = std::make_unique<MeshVFXShader>();
    return shader->Initialize(device_);
}

void MeshVFXRenderSystem::PushSubmit(MeshVFXComponent* component_)
{
    if (!component_ || !component_->IsEnabled()) return;
    components.push_back(component_);
}

void MeshVFXRenderSystem::RenderAll(const RenderContext& renderContext_)
{
    if (!shader || !renderContext_.deviceContext || !renderContext_.camera ||
        !renderContext_.renderState || components.empty())
    {
        components.clear();
        return;
    }

    ID3D11DeviceContext* deviceContext_ = renderContext_.deviceContext;
    deviceContext_->OMSetDepthStencilState(
        renderContext_.renderState->GetDepthStencilState(DepthState::TestOnly), 0);
    deviceContext_->RSSetState(
        renderContext_.renderState->GetRasterizerState(RasterizerState::SolidCullNone));
    ID3D11SamplerState* sampler_ =
        renderContext_.renderState->GetSamplerState(SamplerState::LinearWrap);
    deviceContext_->PSSetSamplers(0, 1, &sampler_);

    shader->Begin(renderContext_);
    shader->UpdateScene(renderContext_);

    for (MeshVFXComponent* component_ : components)
    {
        const std::shared_ptr<GameObject> owner_ = component_->GetGameObject();
        if (!owner_ || !owner_->transform) continue;

        const MeshVFXAsset& asset_ = component_->GetAsset();
        const std::vector<MeshVFXRuntimePart>& runtimeParts_ = component_->GetRuntimeParts();
        const size_t partCount_ = (std::min)(asset_.parts.size(), runtimeParts_.size());

        for (size_t partIndex_ = 0; partIndex_ < partCount_; ++partIndex_)
        {
            const MeshVFXPart& part_ = asset_.parts[partIndex_];
            const MeshVFXRuntimePart& runtimePart_ = runtimeParts_[partIndex_];
            if (!part_.renderEnabled || !runtimePart_.modelResource) continue;

            const PartAnimationState animation_ =
                CalculatePartAnimation(part_, component_->GetTime());
            if (!animation_.active) continue;

            const BlendState blendState_ = part_.blendMode == MeshVFXBlendMode::ADDITIVE
                ? BlendState::Additive
                : BlendState::Transparency;
            deviceContext_->OMSetBlendState(
                renderContext_.renderState->GetBlendState(blendState_), nullptr, 0xFFFFFFFF);

            const auto& meshes_ = runtimePart_.modelResource->meshes;
            for (size_t meshIndex_ = 0; meshIndex_ < meshes_.size(); ++meshIndex_)
            {
                if (part_.meshIndex >= 0 && part_.meshIndex != static_cast<int>(meshIndex_)) continue;

                const GltfModelCommponennt::Mesh& mesh_ = meshes_[meshIndex_];
                if (!mesh_.vertexBuffer || !mesh_.indexBuffer) continue;

                const DirectX::XMMATRIX world_ =
                    CreateNodeTransform(mesh_.node) *
                    CreatePartTransform(part_, animation_) *
                    owner_->transform->GetWorldMatrix();

                ID3D11ShaderResourceView* baseTexture_ = runtimePart_.baseTexture.Get();
                if (part_.useMaterialBaseTexture && part_.baseTexturePath.empty() &&
                    mesh_.material && mesh_.material->baseMap)
                {
                    baseTexture_ = mesh_.material->baseMap.Get();
                }
                ID3D11ShaderResourceView* normalTexture_ = runtimePart_.normalTexture.Get();
                if (part_.useMaterialNormalTexture && part_.normalTexturePath.empty() &&
                    mesh_.material && mesh_.material->normalMap)
                {
                    normalTexture_ = mesh_.material->normalMap.Get();
                }
                shader->UpdatePart(
                    renderContext_, part_, runtimePart_, baseTexture_, normalTexture_,
                    world_, animation_.time, animation_.alpha);

                UINT stride_ = sizeof(GltfModelCommponennt::Vertex);
                UINT offset_ = 0;
                ID3D11Buffer* vertexBuffer_ = mesh_.vertexBuffer.Get();
                deviceContext_->IASetVertexBuffers(0, 1, &vertexBuffer_, &stride_, &offset_);
                deviceContext_->IASetIndexBuffer(mesh_.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
                deviceContext_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                deviceContext_->DrawIndexed(static_cast<UINT>(mesh_.indices.size()), 0, 0);
            }
        }
    }

    shader->End(renderContext_);
    deviceContext_->OMSetBlendState(
        renderContext_.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    deviceContext_->OMSetDepthStencilState(
        renderContext_.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
    deviceContext_->RSSetState(
        renderContext_.renderState->GetRasterizerState(RasterizerState::SolidCullBack));
    components.clear();
}
