#include "MotionVelocityRenderSystem.h"

#include <algorithm>
#include <filesystem>

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Graphics/MotionVelocityComponent.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/GlTFModel.h"

bool MotionVelocityRenderSystem::Initialize(ID3D11Device* device_)
{
    if (device_ == nullptr) return false;
    if (!std::filesystem::exists("Shader/MotionObjectVelocityVS.cso") ||
        !std::filesystem::exists("Shader/MotionObjectVelocityPS.cso"))
    {
        initialized = false;
        return false;
    }

    if (FAILED(CPUResourceLoader::LoadVertexShader(
        device_,
        "Shader/MotionObjectVelocityVS.cso",
        GltfModelCommponennt::InputElementDescs.data(),
        static_cast<UINT>(GltfModelCommponennt::InputElementDescs.size()),
        inputLayout.GetAddressOf(),
        vertexShader.GetAddressOf())))
    {
        initialized = false;
        return false;
    }

    if (FAILED(CPUResourceLoader::LoadPixelShader(
        device_,
        "Shader/MotionObjectVelocityPS.cso",
        pixelShader.GetAddressOf())))
    {
        initialized = false;
        return false;
    }

    if (FAILED(CPUResourceLoader::CreateConstantBuffer(
        device_,
        sizeof(CbObjectMotionVelocity),
        objectConstantBuffer.GetAddressOf())))
    {
        initialized = false;
        return false;
    }

    if (FAILED(CPUResourceLoader::CreateConstantBuffer(
        device_,
        sizeof(CbSkeleton),
        skeletonConstantBuffer.GetAddressOf())))
    {
        initialized = false;
        return false;
    }

    initialized = true;
    return true;
}

void MotionVelocityRenderSystem::Register(const std::shared_ptr<MotionVelocityComponent>& component_)
{
    if (!component_) return;

    RemoveExpiredComponents();
    const auto exists_ = std::any_of(
        components.begin(),
        components.end(),
        [&component_](const std::weak_ptr<MotionVelocityComponent>& registered_)
        {
            return registered_.lock() == component_;
        });
    if (exists_) return;

    components.push_back(component_);
}

void MotionVelocityRenderSystem::Unregister(MotionVelocityComponent* component_)
{
    components.erase(
        std::remove_if(
            components.begin(),
            components.end(),
            [component_](const std::weak_ptr<MotionVelocityComponent>& registered_)
            {
                const std::shared_ptr<MotionVelocityComponent> lockedComponent_ = registered_.lock();
                return !lockedComponent_ || lockedComponent_.get() == component_;
            }),
        components.end());
}

void MotionVelocityRenderSystem::DrawObjectVelocity(
    const RenderContext& rc_,
    ID3D11RenderTargetView* velocityRTV_,
    int width_,
    int height_,
    const DirectX::XMFLOAT4X4& currentViewProjection_,
    const DirectX::XMFLOAT4X4& previousViewProjection_)
{
    if (!initialized || rc_.deviceContext == nullptr || rc_.renderState == nullptr || velocityRTV_ == nullptr)
    {
        return;
    }

    RemoveExpiredComponents();
    if (components.empty()) return;

    ID3D11DeviceContext* dc_ = rc_.deviceContext;
    D3D11_VIEWPORT viewport_{};
    viewport_.Width = static_cast<float>(width_);
    viewport_.Height = static_cast<float>(height_);
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;
    dc_->RSSetViewports(1, &viewport_);

    ID3D11DepthStencilView* depthStencilView_ = GraphicsDeviceManager::Instance().GetDepthStencilView();
    dc_->OMSetRenderTargets(1, &velocityRTV_, depthStencilView_);
    dc_->OMSetDepthStencilState(rc_.renderState->GetDepthStencilState(DepthState::TestOnly), 0);
    dc_->OMSetBlendState(rc_.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    dc_->RSSetState(rc_.renderState->GetRasterizerState(RasterizerState::SolidCullBack));

    dc_->IASetInputLayout(inputLayout.Get());
    dc_->VSSetShader(vertexShader.Get(), nullptr, 0);
    dc_->PSSetShader(pixelShader.Get(), nullptr, 0);
    dc_->VSSetConstantBuffers(0, 1, objectConstantBuffer.GetAddressOf());
    dc_->VSSetConstantBuffers(6, 1, skeletonConstantBuffer.GetAddressOf());
    dc_->PSSetConstantBuffers(0, 1, objectConstantBuffer.GetAddressOf());

    for (const std::weak_ptr<MotionVelocityComponent>& registered_ : components)
    {
        const std::shared_ptr<MotionVelocityComponent> component_ = registered_.lock();
        if (!component_ || !component_->IsEnabled() || !component_->IsVelocityEnabled())
        {
            continue;
        }
        if (!component_->ShouldAffectModel())
        {
            continue;
        }

        component_->CaptureCurrentWorld();
        if (!component_->HasHistory())
        {
            continue;
        }

        DrawModelVelocity(rc_, component_, currentViewProjection_, previousViewProjection_);
    }

    ID3D11Buffer* nullBuffer_ = nullptr;
    dc_->VSSetConstantBuffers(0, 1, &nullBuffer_);
    dc_->VSSetConstantBuffers(6, 1, &nullBuffer_);
    dc_->PSSetConstantBuffers(0, 1, &nullBuffer_);
}

void MotionVelocityRenderSystem::CommitHistory()
{
    RemoveExpiredComponents();
    for (const std::weak_ptr<MotionVelocityComponent>& registered_ : components)
    {
        const std::shared_ptr<MotionVelocityComponent> component_ = registered_.lock();
        if (!component_ || !component_->IsEnabled() || !component_->IsVelocityEnabled())
        {
            continue;
        }

        component_->CommitHistory();
    }
}

void MotionVelocityRenderSystem::RemoveExpiredComponents()
{
    components.erase(
        std::remove_if(
            components.begin(),
            components.end(),
            [](const std::weak_ptr<MotionVelocityComponent>& registered_)
            {
                return registered_.expired();
            }),
        components.end());
}

void MotionVelocityRenderSystem::DrawModelVelocity(
    const RenderContext& rc_,
    const std::shared_ptr<MotionVelocityComponent>& component_,
    const DirectX::XMFLOAT4X4& currentViewProjection_,
    const DirectX::XMFLOAT4X4& previousViewProjection_)
{
    const std::shared_ptr<GameObject> owner_ = component_ ? component_->GetGameObject() : nullptr;
    const std::shared_ptr<GltfModelCommponennt> model_ = owner_ ? owner_->GetComponent<GltfModelCommponennt>() : nullptr;
    if (!model_ || !model_->IsEnabled()) return;

    model_->UpdateTransformIfDirty();

    ID3D11DeviceContext* dc_ = rc_.deviceContext;
    CbObjectMotionVelocity objectConstants_{};
    objectConstants_.currentViewProjection = currentViewProjection_;
    objectConstants_.previousViewProjection = previousViewProjection_;
    objectConstants_.currentOwnerWorld = component_->GetCurrentWorld();
    objectConstants_.previousOwnerWorld = component_->GetPreviousWorld();
    objectConstants_.velocityScale =
        component_->GetVelocityScale() * component_->GetModelVelocityScale();

    const DirectX::XMMATRIX currentOwnerWorld_ =
        DirectX::XMLoadFloat4x4(&objectConstants_.currentOwnerWorld);
    DirectX::XMStoreFloat4x4(
        &objectConstants_.inverseCurrentOwnerWorld,
        DirectX::XMMatrixInverse(nullptr, currentOwnerWorld_));

    dc_->UpdateSubresource(objectConstantBuffer.Get(), 0, nullptr, &objectConstants_, 0, 0);

    const auto& meshes_ = model_->GetMeshes();
    for (const GltfModelCommponennt::Mesh& mesh_ : meshes_)
    {
        if (!mesh_.vertexBuffer || !mesh_.indexBuffer || mesh_.indices.empty())
        {
            continue;
        }
        if (mesh_.material && mesh_.material->alphaMode == GltfModelCommponennt::AlphaMode::Blend)
        {
            continue;
        }

        UINT stride_ = sizeof(GltfModelCommponennt::Vertex);
        UINT offset_ = 0;
        dc_->IASetVertexBuffers(0, 1, mesh_.vertexBuffer.GetAddressOf(), &stride_, &offset_);
        dc_->IASetIndexBuffer(mesh_.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        dc_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        CbSkeleton skeleton_{};
        if (!mesh_.bones.empty())
        {
            for (size_t boneIndex_ = 0; boneIndex_ < mesh_.bones.size() && boneIndex_ < 256; ++boneIndex_)
            {
                const GltfModelCommponennt::Bone& bone_ = mesh_.bones.at(boneIndex_);
                DirectX::XMMATRIX worldTransform_ = DirectX::XMLoadFloat4x4(&bone_.node->worldTransform);
                DirectX::XMMATRIX offsetTransform_ = DirectX::XMLoadFloat4x4(&bone_.offsetTransform);
                DirectX::XMStoreFloat4x4(&skeleton_.boneTransforms[boneIndex_], offsetTransform_ * worldTransform_);
            }
        }
        else if (mesh_.node != nullptr)
        {
            skeleton_.boneTransforms[0] = mesh_.node->worldTransform;
        }
        else
        {
            DirectX::XMStoreFloat4x4(&skeleton_.boneTransforms[0], DirectX::XMMatrixIdentity());
        }
        dc_->UpdateSubresource(skeletonConstantBuffer.Get(), 0, nullptr, &skeleton_, 0, 0);
        dc_->DrawIndexed(static_cast<UINT>(mesh_.indices.size()), 0, 0);
    }
}
