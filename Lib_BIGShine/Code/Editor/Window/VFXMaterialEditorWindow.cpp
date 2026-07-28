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
    BS_PROFILE_SCOPE("VFXMaterialEditorWindow::DrawMaterialToolbar");

    SyncSelectedMaterialSlot();
    if (ImGui::Button("Add Slot"))
    {
        VFXMaterialData materialData_{};
        materialData_.materialName = "NewVFXMaterial";
        MaterialEditorSlot slot_{};
        slot_.materialResource = VFXMaterialResourceManager::GetInstance().CreateTemporary(materialData_);
        materialSlots.push_back(slot_);
        selectedMaterialIndex = static_cast<int>(materialSlots.size()) - 1;
        SyncSelectedMaterialSlot();
    }
    ImGui::SameLine();
    if (materialSlots.size() > 1u && ImGui::Button("Remove Slot"))
    {
        materialSlots.erase(materialSlots.begin() + selectedMaterialIndex);
        selectedMaterialIndex = std::clamp(selectedMaterialIndex, 0, static_cast<int>(materialSlots.size()) - 1);
        SyncSelectedMaterialSlot();
    }

    if (materialSlots.empty())
    {
        MaterialEditorSlot slot_{};
        slot_.materialResource = materialResource;
        slot_.materialPath = materialPath;
        materialSlots.push_back(slot_);
    }

    if (ImGui::BeginTabBar("VFXMaterialEditorSlots"))
    {
        for (int slotIndex_ = 0; slotIndex_ < static_cast<int>(materialSlots.size()); ++slotIndex_)
        {
            const std::string label_ = "Material " + std::to_string(slotIndex_ + 1);
            if (ImGui::BeginTabItem(label_.c_str()))
            {
                if (selectedMaterialIndex != slotIndex_)
                {
                    StoreSelectedMaterialSlot();
                    selectedMaterialIndex = slotIndex_;
                    SyncSelectedMaterialSlot();
                }
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

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
        StoreSelectedMaterialSlot();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload") && !materialPath.empty())
    {
        VFXMaterialResourceManager::GetInstance().Reload(materialPath);
        materialResource = VFXMaterialResourceManager::GetInstance().Load(materialPath);
        StoreSelectedMaterialSlot();
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
    BS_PROFILE_SCOPE("VFXMaterialEditorWindow::DrawMaterialEditor");

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

    if (ImGui::CollapsingHeader("Color Ramp"))
    {
        DrawColorRampEditor(materialData_.colorRamp);
    }

    if (ImGui::CollapsingHeader("Random Color"))
    {
        DrawRandomColorEditor(materialData_.randomColor);
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
    BS_PROFILE_SCOPE("VFXMaterialEditorWindow::DrawUvEditor");

    if (!ImGui::TreeNode(label_)) return;
    ImGui::DragFloat2("Scale", &uv_.scale.x, 0.01f);
    ImGui::DragFloat2("Offset", &uv_.offset.x, 0.01f);
    ImGui::DragFloat2("Scroll Speed", &uv_.scrollSpeed.x, 0.01f);
    ImGui::DragFloat("Rotation", &uv_.rotation, 1.0f, -360.0f, 360.0f);
    ImGui::DragFloat("Rotation Speed", &uv_.rotationSpeed, 0.01f);
    ImGui::DragFloat2("Pivot", &uv_.pivot.x, 0.01f);
    ImGui::TreePop();
}

void VFXMaterialEditorWindow::DrawColorRampEditor(VFXMaterialColorRampData& colorRamp_)
{
    BS_PROFILE_SCOPE("VFXMaterialEditorWindow::DrawColorRampEditor");

    ImGui::Checkbox("Ramp Enabled", &colorRamp_.enabled);

    const char* sampleSourceNames_[] = { "Life", "Time", "Fixed" };
    int sampleSource_ = static_cast<int>(colorRamp_.sampleSource);
    if (ImGui::Combo("Sample Source", &sampleSource_, sampleSourceNames_, IM_ARRAYSIZE(sampleSourceNames_)))
    {
        colorRamp_.sampleSource = static_cast<VFX_MATERIAL_COLOR_RAMP_SAMPLE_SOURCE>(
            std::clamp(sampleSource_, 0, IM_ARRAYSIZE(sampleSourceNames_) - 1));
    }

    const char* blendModeNames_[] = { "Multiply", "Override", "Add" };
    int blendMode_ = static_cast<int>(colorRamp_.blendMode);
    if (ImGui::Combo("Blend Mode", &blendMode_, blendModeNames_, IM_ARRAYSIZE(blendModeNames_)))
    {
        colorRamp_.blendMode = static_cast<VFX_MATERIAL_COLOR_RAMP_BLEND_MODE>(
            std::clamp(blendMode_, 0, IM_ARRAYSIZE(blendModeNames_) - 1));
    }

    const char* interpolationNames_[] = { "Linear", "Constant", "Smooth" };
    int interpolation_ = static_cast<int>(colorRamp_.interpolation);
    if (ImGui::Combo("Interpolation", &interpolation_, interpolationNames_, IM_ARRAYSIZE(interpolationNames_)))
    {
        colorRamp_.interpolation = static_cast<VFX_MATERIAL_COLOR_RAMP_INTERPOLATION>(
            std::clamp(interpolation_, 0, IM_ARRAYSIZE(interpolationNames_) - 1));
    }

    const char* wrapModeNames_[] = { "Clamp", "Repeat", "Mirror" };
    int wrapMode_ = static_cast<int>(colorRamp_.wrapMode);
    if (ImGui::Combo("Wrap Mode", &wrapMode_, wrapModeNames_, IM_ARRAYSIZE(wrapModeNames_)))
    {
        colorRamp_.wrapMode = static_cast<VFX_MATERIAL_COLOR_RAMP_WRAP_MODE>(
            std::clamp(wrapMode_, 0, IM_ARRAYSIZE(wrapModeNames_) - 1));
    }

    ImGui::DragFloat("Fixed Position", &colorRamp_.fixedPosition, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Scroll Speed", &colorRamp_.scrollSpeed, 0.01f, -20.0f, 20.0f);
    ImGui::DragFloat("Ramp Influence", &colorRamp_.influence, 0.01f, 0.0f, 1.0f);
    ImGui::Checkbox("Ramp Affect Alpha", &colorRamp_.affectAlpha);

    if (colorRamp_.stops.empty())
    {
        colorRamp_.stops.push_back({ 0.0f, { 1.0f, 1.0f, 1.0f, 1.0f } });
        colorRamp_.stops.push_back({ 1.0f, { 1.0f, 1.0f, 1.0f, 1.0f } });
    }

    if (ImGui::Button("Add Ramp Stop"))
    {
        colorRamp_.stops.push_back({ 1.0f, { 1.0f, 1.0f, 1.0f, 1.0f } });
    }
    ImGui::SameLine();
    if (ImGui::Button("Sort Ramp Stops"))
    {
        std::sort(colorRamp_.stops.begin(), colorRamp_.stops.end(),
            [](const VFXMaterialColorRampStopData& left_, const VFXMaterialColorRampStopData& right_)
            {
                return left_.position < right_.position;
            });
    }

    int removeIndex_ = -1;
    for (int stopIndex_ = 0; stopIndex_ < static_cast<int>(colorRamp_.stops.size()); ++stopIndex_)
    {
        ImGui::PushID(stopIndex_);
        VFXMaterialColorRampStopData& stop_ = colorRamp_.stops[stopIndex_];
        ImGui::Separator();
        ImGui::Text("Stop %d", stopIndex_);
        ImGui::DragFloat("Position", &stop_.position, 0.01f, 0.0f, 1.0f);
        stop_.position = std::clamp(stop_.position, 0.0f, 1.0f);
        ImGui::ColorEdit4("Color", &stop_.color.x);
        if (colorRamp_.stops.size() > 1u && ImGui::Button("Remove Stop"))
        {
            removeIndex_ = stopIndex_;
        }
        ImGui::PopID();
    }

    if (removeIndex_ >= 0)
    {
        colorRamp_.stops.erase(colorRamp_.stops.begin() + removeIndex_);
    }
}

void VFXMaterialEditorWindow::DrawRandomColorEditor(VFXMaterialRandomColorData& randomColor_)
{
    BS_PROFILE_SCOPE("VFXMaterialEditorWindow::DrawRandomColorEditor");

    ImGui::Checkbox("Random Enabled", &randomColor_.enabled);
    const char* modeNames_[] = { "Multiply", "Override", "Add" };
    int mode_ = static_cast<int>(randomColor_.mode);
    if (ImGui::Combo("Random Blend Mode", &mode_, modeNames_, IM_ARRAYSIZE(modeNames_)))
    {
        randomColor_.mode = static_cast<VFX_MATERIAL_RANDOM_COLOR_MODE>(
            std::clamp(mode_, 0, IM_ARRAYSIZE(modeNames_) - 1));
    }

    ImGui::ColorEdit4("Random Min Color", &randomColor_.minColor.x);
    ImGui::ColorEdit4("Random Max Color", &randomColor_.maxColor.x);
    ImGui::DragFloat("Random Influence", &randomColor_.influence, 0.01f, 0.0f, 1.0f);
    ImGui::Checkbox("Random Affect Alpha", &randomColor_.affectAlpha);

    int seed_ = static_cast<int>(randomColor_.seed);
    if (ImGui::DragInt("Random Seed", &seed_, 1.0f, 0, 2147483647))
    {
        randomColor_.seed = static_cast<uint32_t>(seed_);
    }
}

void VFXMaterialEditorWindow::LoadMaterial(const std::string& path_)
{
    BS_PROFILE_SCOPE("VFXMaterialEditorWindow::LoadMaterial");
    materialPath = path_;
    materialResource = VFXMaterialResourceManager::GetInstance().Load(materialPath);
    StoreSelectedMaterialSlot();
}

void VFXMaterialEditorWindow::SaveMaterial(const std::string& path_)
{
    BS_PROFILE_SCOPE("VFXMaterialEditorWindow::SaveMaterial");
    EnsureEditableMaterial();
    if (!materialResource) return;
    VFXMaterialResourceManager::GetInstance().Save(path_, materialResource);
    materialPath = path_;
    StoreSelectedMaterialSlot();
}

void VFXMaterialEditorWindow::EnsureEditableMaterial()
{
    BS_PROFILE_SCOPE("VFXMaterialEditorWindow::EnsureEditableMaterial");

    if (materialResource) return;
    VFXMaterialData materialData_{};
    materialData_.materialName = "TemporaryVFXMaterial";
    materialResource = VFXMaterialResourceManager::GetInstance().CreateTemporary(materialData_);
    StoreSelectedMaterialSlot();
}

void VFXMaterialEditorWindow::SyncSelectedMaterialSlot()
{
    BS_PROFILE_SCOPE("VFXMaterialEditorWindow::SyncSelectedMaterialSlot");
    if (materialSlots.empty())
    {
        MaterialEditorSlot slot_{};
        slot_.materialResource = materialResource;
        slot_.materialPath = materialPath;
        materialSlots.push_back(slot_);
    }
    selectedMaterialIndex = std::clamp(
        selectedMaterialIndex,
        0,
        static_cast<int>(materialSlots.size()) - 1);
    materialResource = materialSlots[selectedMaterialIndex].materialResource;
    materialPath = materialSlots[selectedMaterialIndex].materialPath;
}

void VFXMaterialEditorWindow::StoreSelectedMaterialSlot()
{
    BS_PROFILE_SCOPE("VFXMaterialEditorWindow::StoreSelectedMaterialSlot");
    if (materialSlots.empty())
    {
        materialSlots.emplace_back();
        selectedMaterialIndex = 0;
    }
    selectedMaterialIndex = std::clamp(
        selectedMaterialIndex,
        0,
        static_cast<int>(materialSlots.size()) - 1);
    materialSlots[selectedMaterialIndex].materialResource = materialResource;
    materialSlots[selectedMaterialIndex].materialPath = materialPath;
}
