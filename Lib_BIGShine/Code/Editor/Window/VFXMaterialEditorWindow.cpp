#include "PrecompiledHeader.h"

#include "VFXMaterialEditorWindow.h"

#include "Editor/AssetSlotGui.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/VFXMaterial/VFXMaterialResourceManager.h"
#include "Engine/Platform/WindowsDialogHelper.h"

#include <algorithm>
#include <imgui.h>
#include <imgui_stdlib.h>

VFXMaterialEditorWindow::VFXMaterialEditorWindow(GameObjectManager* manager_)
    : EditorWindow("VFX Material Editor", manager_)
{
    defaultWindowSize = ImVec2(520.0f, 720.0f);
}

void VFXMaterialEditorWindow::Draw()
{
    BS_PROFILE_SCOPE("VFXMaterialEditorWindow::Draw");
    if (!BeginEditorWindow()) { EndEditorWindow(); return; }
    DrawMaterialToolbar();
    ImGui::Separator();
    DrawMaterialEditor();
    EndEditorWindow();
}

void VFXMaterialEditorWindow::DrawMaterialToolbar()
{
    std::string selectedPath_ = materialPath;
    if (AssetSlotGui::Draw(
        "Material",
        AssetSlotType::MATERIAL,
        selectedPath_,
        nullptr,
        "Data/VFXMaterials"))
    {
        LoadMaterial(selectedPath_);
    }

    if (ImGui::Button("New"))
    {
        VFXMaterialData materialData_{};
        materialData_.materialName = "NewVFXMaterial";
        materialResource = VFXMaterialResourceManager::GetInstance().CreateTemporary(materialData_);
        materialPath.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload") && !materialPath.empty())
    {
        VFXMaterialResourceManager::GetInstance().Reload(materialPath);
        materialResource = VFXMaterialResourceManager::GetInstance().Load(materialPath);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save"))
    {
        if (materialPath.empty())
        {
            const std::string selectedPath_ = WindowsDialogHelper::SaveVFXMaterialDialog(
                L"Save VFX Material",
                "Data/VFXMaterials/NewVFXMaterial.vfxmat",
                GraphicsDeviceManager::Instance().GetWindowHandle());
            if (!selectedPath_.empty()) SaveMaterial(selectedPath_);
        }
        else
        {
            SaveMaterial(materialPath);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save As"))
    {
        const std::string selectedPath_ = WindowsDialogHelper::SaveVFXMaterialDialog(
            L"Save VFX Material As",
            materialPath.empty() ? "Data/VFXMaterials/NewVFXMaterial.vfxmat" : materialPath,
            GraphicsDeviceManager::Instance().GetWindowHandle());
        if (!selectedPath_.empty()) SaveMaterial(selectedPath_);
    }
}

void VFXMaterialEditorWindow::DrawMaterialEditor()
{
    EnsureEditableMaterial();
    if (!materialResource) return;

    ImGui::TextDisabled("This material is a shared Resource. Changes affect every VFX using the same material.");
    VFXMaterialData& materialData_ = materialResource->GetEditableMaterialData();
    ImGui::InputText("Material Name", &materialData_.materialName);

    if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool textureChanged_ = false;
        textureChanged_ |= AssetSlotGui::Draw("Base Texture", AssetSlotType::TEXTURE, materialData_.baseTexturePath, materialResource->GetBaseTexture(), "Data/Texture");
        textureChanged_ |= AssetSlotGui::Draw("Mask Texture", AssetSlotType::TEXTURE, materialData_.maskTexturePath, materialResource->GetMaskTexture(), "Data/Texture");
        textureChanged_ |= AssetSlotGui::Draw("Noise Texture", AssetSlotType::TEXTURE, materialData_.noiseTexturePath, materialResource->GetNoiseTexture(), "Data/Texture");
        textureChanged_ |= AssetSlotGui::Draw("Normal Texture", AssetSlotType::TEXTURE, materialData_.normalTexturePath, materialResource->GetNormalTexture(), "Data/Texture");
        textureChanged_ |= AssetSlotGui::Draw("Distortion Texture", AssetSlotType::TEXTURE, materialData_.distortionTexturePath, materialResource->GetDistortionTexture(), "Data/Texture");
        const bool reloadTextures_ = ImGui::Button("Reload Textures");
        if (textureChanged_ || reloadTextures_)
        {
            materialResource->LoadTextures(GraphicsDeviceManager::Instance().GetDevice());
        }
    }

    if (ImGui::CollapsingHeader("Color", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::ColorEdit4("Base Color", &materialData_.baseColor.x);
        ImGui::ColorEdit3("Emissive Color", &materialData_.emissiveColor.x);
        ImGui::DragFloat("Emissive Intensity", &materialData_.emissiveIntensity, 0.05f, 0.0f, 100.0f);
        ImGui::DragFloat("Alpha", &materialData_.alpha, 0.01f, 0.0f, 1.0f);
    }

    if (ImGui::CollapsingHeader("UV", ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawUvEditor("Main UV", materialData_.mainUV);
        DrawUvEditor("Noise UV", materialData_.noiseUV);
    }

    if (ImGui::CollapsingHeader("Dissolve"))
    {
        ImGui::Checkbox("Dissolve Enabled", &materialData_.dissolve.enabled);
        ImGui::DragFloat("Threshold", &materialData_.dissolve.threshold, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Edge Width", &materialData_.dissolve.edgeWidth, 0.01f, 0.0f, 1.0f);
        ImGui::ColorEdit4("Edge Color", &materialData_.dissolve.edgeColor.x);
    }

    if (ImGui::CollapsingHeader("Distortion"))
    {
        ImGui::Checkbox("Distortion Enabled", &materialData_.distortion.enabled);
        ImGui::DragFloat("Strength", &materialData_.distortion.strength, 0.01f, 0.0f, 100.0f);
    }

    if (ImGui::CollapsingHeader("Render State"))
    {
        const char* blendNames_[] = { "Alpha", "Additive" };
        int blendMode_ = static_cast<int>(materialData_.renderState.blendMode);
        if (ImGui::Combo("Blend Mode", &blendMode_, blendNames_, IM_ARRAYSIZE(blendNames_)))
        {
            materialData_.renderState.blendMode =
                static_cast<VFX_MATERIAL_BLEND_MODE>(std::clamp(blendMode_, 0, 1));
        }
        const char* cullNames_[] = { "None", "Back", "Front" };
        int cullMode_ = static_cast<int>(materialData_.renderState.cullMode);
        if (ImGui::Combo("Cull Mode", &cullMode_, cullNames_, IM_ARRAYSIZE(cullNames_)))
        {
            materialData_.renderState.cullMode =
                static_cast<VFX_MATERIAL_CULL_MODE>(std::clamp(cullMode_, 0, 2));
        }
        ImGui::Checkbox("Depth Test", &materialData_.renderState.depthTest);
        ImGui::Checkbox("Depth Write", &materialData_.renderState.depthWrite);
    }
}

void VFXMaterialEditorWindow::DrawUvEditor(
    const char* label_,
    VFXMaterialUVData& uv_)
{
    if (!ImGui::TreeNode(label_)) return;
    ImGui::DragFloat2("Scale", &uv_.scale.x, 0.01f);
    ImGui::DragFloat2("Offset", &uv_.offset.x, 0.01f);
    ImGui::DragFloat2("Scroll Speed", &uv_.scrollSpeed.x, 0.01f);
    ImGui::DragFloat("Rotation Speed", &uv_.rotationSpeed, 0.01f);
    ImGui::DragFloat2("Pivot", &uv_.pivot.x, 0.01f);
    ImGui::TreePop();
}

void VFXMaterialEditorWindow::LoadMaterial(const std::string& path_)
{
    BS_PROFILE_SCOPE("VFXMaterialEditorWindow::LoadMaterial");
    materialPath = path_;
    materialResource = VFXMaterialResourceManager::GetInstance().Load(materialPath);
}

void VFXMaterialEditorWindow::SaveMaterial(const std::string& path_)
{
    BS_PROFILE_SCOPE("VFXMaterialEditorWindow::SaveMaterial");
    EnsureEditableMaterial();
    if (!materialResource) return;
    VFXMaterialResourceManager::GetInstance().Save(path_, materialResource);
    materialPath = path_;
}

void VFXMaterialEditorWindow::EnsureEditableMaterial()
{
    if (materialResource) return;
    VFXMaterialData materialData_{};
    materialData_.materialName = "TemporaryVFXMaterial";
    materialResource = VFXMaterialResourceManager::GetInstance().CreateTemporary(materialData_);
}
