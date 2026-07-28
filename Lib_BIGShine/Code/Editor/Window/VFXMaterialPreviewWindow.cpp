#include "PrecompiledHeader.h"

#include "VFXMaterialPreviewWindow.h"

#include "Editor/AssetSlotGui.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Core/RenderContext.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"

#include <algorithm>
#include <imgui.h>

VFXMaterialPreviewWindow::VFXMaterialPreviewWindow(GameObjectManager* manager_)
    : EditorWindow("VFX Material Preview", manager_)
{
    defaultWindowSize = ImVec2(420.0f, 520.0f);
}

void VFXMaterialPreviewWindow::Draw()
{
    BS_PROFILE_SCOPE("VFXMaterialPreviewWindow::Draw");
    if (!BeginEditorWindow()) { EndEditorWindow(); return; }

    EnsurePreviewObject();
    DrawToolbar();
    ImGui::Separator();

    const ImVec2 availableSize_ = ImGui::GetContentRegionAvail();
    const ImVec2 previewSize_ =
    {
        (std::max)(availableSize_.x, 64.0f),
        (std::max)(availableSize_.y, 64.0f)
    };
    DrawPreview(previewSize_);

    EndEditorWindow();
}

void VFXMaterialPreviewWindow::EnsurePreviewObject()
{
    BS_PROFILE_SCOPE("VFXMaterialPreviewWindow::EnsurePreviewObject");
    if (isInitialized) return;

    ID3D11Device* device_ = GraphicsDeviceManager::Instance().GetDevice();
    if (!device_) return;

    meshVFXRenderSystem.Initialize(device_);

    previewObject = std::make_shared<GameObject>("VFXMaterialPreviewSphere");
    previewObject->transform->gameObject = previewObject;
    previewObject->transform->localScale = { 1.5f, 1.5f, 1.5f };
    previewObject->transform->MarkDirty();

    previewMeshVFX = previewObject->AddComponent<MeshVFXComponent>();
    MeshVFXAsset asset_{};
    MeshVFXPart part_{};
    part_.renderEnabled = true;
    part_.modelPath = "Data/Model/VFX/Sphere.glb";
    part_.meshIndex = -1;
    part_.blendMode = MeshVFXBlendMode::ALPHA;
    part_.baseSamplerMode = MESH_VFX_SAMPLER_MODE::WRAP;
    part_.normalSamplerMode = MESH_VFX_SAMPLER_MODE::WRAP;
    part_.maskSamplerMode = MESH_VFX_SAMPLER_MODE::WRAP;
    part_.noiseSamplerMode = MESH_VFX_SAMPLER_MODE::WRAP;
    asset_.parts.push_back(part_);
    previewMeshVFX->SetAsset(asset_);
    previewMeshVFX->Play();

    isInitialized = true;
}

void VFXMaterialPreviewWindow::DrawToolbar()
{
    BS_PROFILE_SCOPE("VFXMaterialPreviewWindow::DrawToolbar");

    if (AssetSlotGui::Draw(
        "VFX Material",
        AssetSlotType::MATERIAL,
        materialPath,
        nullptr,
        "Data/VFXMaterials"))
    {
        ApplyPreviewMaterial();
    }

    if (ImGui::Button("Reload Preview"))
    {
        ApplyPreviewMaterial();
    }
}

void VFXMaterialPreviewWindow::DrawPreview(const ImVec2& previewSize_)
{
    BS_PROFILE_SCOPE("VFXMaterialPreviewWindow::DrawPreview");
    RenderPreview(previewSize_);
    if (previewTarget.GetShaderResourceView())
    {
        ImGui::Image(
            reinterpret_cast<void*>(previewTarget.GetShaderResourceView()),
            previewSize_);
    }
}

void VFXMaterialPreviewWindow::RenderPreview(const ImVec2& previewSize_)
{
    BS_PROFILE_SCOPE("VFXMaterialPreviewWindow::RenderPreview");

    auto& graphics_ = GraphicsDeviceManager::Instance();
    ID3D11Device* device_ = graphics_.GetDevice();
    ID3D11DeviceContext* context_ = graphics_.GetContext();
    if (!device_ || !context_ || !previewMeshVFX || !previewObject) return;

    const int width_ = (std::max)(1, static_cast<int>(previewSize_.x));
    const int height_ = (std::max)(1, static_cast<int>(previewSize_.y));
    if (!previewTarget.Resize(device_, width_, height_)) return;

    const float clearColor_[4] = { 0.025f, 0.028f, 0.032f, 0.0f };
    previewTarget.Begin(context_, clearColor_);

    previewRotation += ImGui::GetIO().DeltaTime * 18.0f;
    previewObject->transform->localRotation = { 0.0f, previewRotation, 0.0f };
    previewObject->transform->UpdateWorldMatrix();
    previewMeshVFX->Update(ImGui::GetIO().DeltaTime);

    const float aspect_ = static_cast<float>(width_) / static_cast<float>(height_);
    previewCamera.SetPerspectiveFov(45.0f, aspect_, 0.01f, 100.0f);
    const DirectX::XMMATRIX view_ = DirectX::XMMatrixLookAtLH(
        DirectX::XMVectorSet(0.0f, 0.35f, -4.0f, 1.0f),
        DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
        DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    DirectX::XMFLOAT4X4 viewMatrix_{};
    DirectX::XMStoreFloat4x4(&viewMatrix_, view_);
    previewCamera.SetViewMatrix(viewMatrix_);

    DecalManager decalManager_{};
    RenderContext renderContext_{};
    renderContext_.device = device_;
    renderContext_.deviceContext = context_;
    renderContext_.renderState = graphics_.GetRenderState();
    renderContext_.camera = &previewCamera;
    renderContext_.depthSRV = previewTarget.GetDepthShaderResourceView();
    renderContext_.baseColorSRV = previewTarget.GetShaderResourceView();
    renderContext_.decal = &decalManager_;
    renderContext_.screenWidth = static_cast<float>(width_);
    renderContext_.screenHeight = static_cast<float>(height_);

    meshVFXRenderSystem.PushSubmit(previewMeshVFX.get());
    meshVFXRenderSystem.RenderAll(
        renderContext_,
        true,
        previewTarget.GetRenderTargetView(),
        previewTarget.GetDepthStencilView());

    previewTarget.End(context_);
}

void VFXMaterialPreviewWindow::ApplyPreviewMaterial()
{
    BS_PROFILE_SCOPE("VFXMaterialPreviewWindow::ApplyPreviewMaterial");
    if (!previewMeshVFX) return;

    MeshVFXAsset asset_ = previewMeshVFX->GetAsset();
    if (asset_.parts.empty()) asset_.parts.emplace_back();
    MeshVFXPart& part_ = asset_.parts.front();
    part_.renderEnabled = true;
    part_.modelPath = "Data/Model/VFX/Sphere.glb";
    part_.materialPath = materialPath;
    part_.materialPaths.clear();
    if (!materialPath.empty()) part_.materialPaths.push_back(materialPath);
    previewMeshVFX->SetAsset(asset_);
}
