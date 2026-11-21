#include "PrecompiledHeader.h"

#include "VFXMaterialPreviewWindow.h"

#include "Editor/AssetSlotGui.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Core/RenderContext.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"

#include <algorithm>
#include <imgui.h>

namespace
{
    constexpr const char* PREVIEW_SPHERE_MODEL_PATH = "Data/Model/VFX/Sphere.glb";
    constexpr const char* DEFAULT_PREVIEW_MATERIAL_PATH = "Data/VFXMaterials/NewVFXMaterial.vfxmat";
}

VFXMaterialPreviewWindow::VFXMaterialPreviewWindow(GameObjectManager* manager_)
    : EditorWindow("VFX Material Preview", manager_)
{
    // 必要な時だけWindowメニューから開く補助ビューなので、起動直後は非表示にしておく。
    isVisible = false;
    defaultWindowSize = ImVec2(420.0f, 520.0f);
    materialPath = DEFAULT_PREVIEW_MATERIAL_PATH;
}

void VFXMaterialPreviewWindow::Draw()
{
    BS_PROFILE_SCOPE("VFXMaterialPreviewWindow::Draw");
    if (!BeginEditorWindow())
    {
        EndEditorWindow();
        return;
    }

    // Materialを選んで球体MeshVFXへ適用し、オフスクリーンで即プレビューする。
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

    DrawStylePopup();
    EndEditorWindow();
}

void VFXMaterialPreviewWindow::EnsurePreviewObject()
{
    BS_PROFILE_SCOPE("VFXMaterialPreviewWindow::EnsurePreviewObject");
    if (isInitialized)
    {
        return;
    }

    // Scene内の実オブジェクトを使わず、プレビュー専用GameObjectとMeshVFXComponentを作る。
    ID3D11Device* device_ = GraphicsDeviceManager::Instance().GetDevice();
    if (!device_)
    {
        return;
    }

    meshVFXRenderSystem.Initialize(device_);

    previewObject = std::make_shared<GameObject>("VFXMaterialPreviewSphere");
    previewObject->transform->gameObject = previewObject;
    previewObject->transform->localScale = { 1.5f, 1.5f, 1.5f };
    previewObject->transform->MarkDirty();

    previewMeshVFX = previewObject->AddComponent<MeshVFXComponent>();
    previewMeshVFX->SetAsset(CreatePreviewAsset());
    previewMeshVFX->Play();

    isInitialized = true;
}

MeshVFXAsset VFXMaterialPreviewWindow::CreatePreviewAsset() const
{
    BS_PROFILE_SCOPE("VFXMaterialPreviewWindow::CreatePreviewAsset");

    MeshVFXAsset asset_{};
    MeshVFXPart part_{};
    // 選択中Materialを球へ貼るための、Window専用の最小MeshVFXアセットを組み立てる。
    part_.renderEnabled = true;
    part_.modelPath = PREVIEW_SPHERE_MODEL_PATH;
    part_.meshIndex = -1;
    part_.blendMode = MeshVFXBlendMode::ALPHA;
    part_.baseSamplerMode = MESH_VFX_SAMPLER_MODE::WRAP;
    part_.normalSamplerMode = MESH_VFX_SAMPLER_MODE::WRAP;
    part_.maskSamplerMode = MESH_VFX_SAMPLER_MODE::WRAP;
    part_.noiseSamplerMode = MESH_VFX_SAMPLER_MODE::WRAP;
    part_.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    part_.alpha = 1.0f;

    const std::string previewMaterialPath_ = materialPath.empty()
        ? DEFAULT_PREVIEW_MATERIAL_PATH
        : materialPath;
    part_.materialPath = previewMaterialPath_;
    part_.materialPaths.push_back(previewMaterialPath_);

    asset_.parts.push_back(part_);
    return asset_;
}

void VFXMaterialPreviewWindow::DrawToolbar()
{
    BS_PROFILE_SCOPE("VFXMaterialPreviewWindow::DrawToolbar");

    // AssetSlotでVFXMaterialを選び、変更されたらプレビュー用MeshVFXへ即反映する。
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

    ImGui::SameLine();
    if (ImGui::Button("Default"))
    {
        materialPath = DEFAULT_PREVIEW_MATERIAL_PATH;
        ApplyPreviewMaterial();
    }
}

void VFXMaterialPreviewWindow::DrawPreview(const ImVec2& previewSize_)
{
    BS_PROFILE_SCOPE("VFXMaterialPreviewWindow::DrawPreview");
    // RenderPreviewで描いたRTをImGui画像として表示する。
    RenderPreview(previewSize_);
    if (previewTarget.GetShaderResourceView())
    {
        const ImVec2 imageMin_ = ImGui::GetCursorScreenPos();
        const ImVec2 imageMax_ = ImVec2(imageMin_.x + previewSize_.x, imageMin_.y + previewSize_.y);
        // RTのAlphaやMaterial設定に関係なく、プレビュー領域そのものがSceneに溶けないよう下地を描く。
        ImGui::GetWindowDrawList()->AddRectFilled(
            imageMin_,
            imageMax_,
            ImGui::GetColorU32(ImVec4(0.025f, 0.028f, 0.032f, 1.0f)));
        ImGui::Image(
            reinterpret_cast<void*>(previewTarget.GetShaderResourceView()),
            previewSize_);
        if (previewMeshVFX && !previewMeshVFX->HasRuntimeResources())
        {
            ImGui::SetCursorScreenPos(ImVec2(imageMin_.x + 12.0f, imageMin_.y + 12.0f));
            ImGui::TextDisabled("Preview mesh resource is not ready.");
            ImGui::SetCursorScreenPos(ImVec2(imageMin_.x, imageMax_.y));
        }
    }
    else
    {
        ImGui::TextDisabled("Preview render target is not ready.");
    }
}

void VFXMaterialPreviewWindow::RenderPreview(const ImVec2& previewSize_)
{
    BS_PROFILE_SCOPE("VFXMaterialPreviewWindow::RenderPreview");

    // ウィンドウ内サイズに合わせてRTを作り直し、MeshVFXRenderSystemをEditorWindow用出力先で実行する。
    auto& graphics_ = GraphicsDeviceManager::Instance();
    ID3D11Device* device_ = graphics_.GetDevice();
    ID3D11DeviceContext* context_ = graphics_.GetContext();
    if (!device_ || !context_ || !previewMeshVFX || !previewObject)
    {
        return;
    }

    const int width_ = (std::max)(1, static_cast<int>(previewSize_.x));
    const int height_ = (std::max)(1, static_cast<int>(previewSize_.y));
    if (!previewTarget.Resize(device_, width_, height_))
    {
        return;
    }

    const float clearColor_[4] = { 0.025f, 0.028f, 0.032f, 1.0f };
    previewTarget.Begin(context_, clearColor_);

    previewRotation += ImGui::GetIO().DeltaTime * 18.0f;
    // 回転させながら表示し、MaterialのUVスクロールやノーマルの見え方を確認しやすくする。
    previewObject->transform->localRotation = { 0.0f, previewRotation, 0.0f };
    previewObject->transform->UpdateWorldMatrix();
    previewMeshVFX->Update(ImGui::GetIO().DeltaTime);

    SetupPreviewCamera(width_, height_);

    DecalManager decalManager_{};
    RenderContext renderContext_{};
    // 通常描画と同じRenderContext形式に詰めて、MeshVFX側の描画経路をそのまま使う。
    renderContext_.device = device_;
    renderContext_.deviceContext = context_;
    renderContext_.renderState = graphics_.GetRenderState();
    renderContext_.camera = &previewCamera;
    renderContext_.depthSRV = previewTarget.GetDepthShaderResourceView();
    renderContext_.baseColorSRV = nullptr;
    renderContext_.decal = &decalManager_;
    renderContext_.screenWidth = static_cast<float>(width_);
    renderContext_.screenHeight = static_cast<float>(height_);

    meshVFXRenderSystem.PushSubmit(previewMeshVFX.get());
    meshVFXRenderSystem.RenderAll(
        renderContext_,
        true,
        previewTarget.GetRenderTargetView(),
        previewTarget.GetDepthStencilView());
    previewTarget.ResetRenderTarget(context_);

    previewTarget.End(context_);
    graphics_.SetBackBufferRenderTarget();
}

void VFXMaterialPreviewWindow::ApplyPreviewMaterial()
{
    BS_PROFILE_SCOPE("VFXMaterialPreviewWindow::ApplyPreviewMaterial");
    if (!previewMeshVFX)
    {
        return;
    }

    // Material変更時はプレビュー専用Assetを作り直し、Runtimeリソースへ即反映する。
    previewMeshVFX->SetAsset(CreatePreviewAsset());
    previewMeshVFX->Play();
}

void VFXMaterialPreviewWindow::SetupPreviewCamera(int width_, int height_)
{
    BS_PROFILE_SCOPE("VFXMaterialPreviewWindow::SetupPreviewCamera");

    const float aspect_ = static_cast<float>((std::max)(width_, 1)) /
        static_cast<float>((std::max)(height_, 1));
    previewCamera.SetPerspectiveFov(45.0f, aspect_, 0.01f, 100.0f);

    // Window専用の固定カメラで球を斜め前から見る。
    const DirectX::XMMATRIX view_ = DirectX::XMMatrixLookAtLH(
        DirectX::XMVectorSet(0.0f, 0.35f, -4.0f, 1.0f),
        DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
        DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    DirectX::XMFLOAT4X4 viewMatrix_{};
    DirectX::XMStoreFloat4x4(&viewMatrix_, view_);
    previewCamera.SetViewMatrix(viewMatrix_);
}
