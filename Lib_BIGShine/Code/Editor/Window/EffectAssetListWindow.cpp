#include "PrecompiledHeader.h"

#include "EffectAssetListWindow.h"

#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/System/EffectManager.h"

#include <imgui.h>

#include <vector>

EffectAssetListWindow::EffectAssetListWindow(GameObjectManager* manager_)
    : EditorWindow("Effect Asset Keys", manager_)
{
    defaultWindowSize = ImVec2(760.0f, 520.0f);
    SetVisible(false);
}

void EffectAssetListWindow::Draw()
{
    if (!BeginEditorWindow(nullptr, ImGuiWindowFlags_NoCollapse))
    {
        EndEditorWindow();
        return;
    }

    if (!gameObjectManager)
    {
        ImGui::TextDisabled("GameObjectManager is not available.");
        EndEditorWindow();
        return;
    }

    EffectManager& effectManager_ = gameObjectManager->GetEffectManager();
    if (ImGui::Button("Refresh"))
    {
        effectManager_.RefreshAssetCache();
        effectManager_.BuildAssetNameMap();
        effectManager_.PreloadAllEffects();
    }

    if (ImGui::BeginTabBar("EffectAssetTabs"))
    {
        if (ImGui::BeginTabItem("Particle"))
        {
            DrawAssetTable(EFFECT_ASSET_LIST_TYPE::PARTICLE);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Mesh VFX"))
        {
            DrawAssetTable(EFFECT_ASSET_LIST_TYPE::MESH_VFX);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("VFX Sequence"))
        {
            DrawAssetTable(EFFECT_ASSET_LIST_TYPE::VFX_SEQUENCE);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Impact Effect"))
        {
            DrawAssetTable(EFFECT_ASSET_LIST_TYPE::IMPACT_EFFECT);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    EndEditorWindow();
}

void EffectAssetListWindow::DrawAssetTable(EFFECT_ASSET_LIST_TYPE type_)
{
    if (!gameObjectManager) return;

    const EffectManager& effectManager_ = gameObjectManager->GetEffectManager();
    const std::vector<EffectAssetEntry>* assets_ = nullptr;
    const char* tableId_ = nullptr;
    switch (type_)
    {
    case EFFECT_ASSET_LIST_TYPE::PARTICLE:
        assets_ = &effectManager_.GetParticleAssets();
        tableId_ = "ParticleEffectAssetTable";
        break;
    case EFFECT_ASSET_LIST_TYPE::MESH_VFX:
        assets_ = &effectManager_.GetMeshVFXAssets();
        tableId_ = "MeshVFXEffectAssetTable";
        break;
    case EFFECT_ASSET_LIST_TYPE::VFX_SEQUENCE:
        assets_ = &effectManager_.GetVFXSequenceAssets();
        tableId_ = "VFXSequenceEffectAssetTable";
        break;
    case EFFECT_ASSET_LIST_TYPE::IMPACT_EFFECT:
        assets_ = &effectManager_.GetImpactEffectAssets();
        tableId_ = "ImpactEffectAssetTable";
        break;
    }
    if (!assets_ || !tableId_) return;

    ImGui::Text("Count: %d", static_cast<int>(assets_->size()));
    if (ImGui::BeginTable(
        tableId_,
        3,
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ScrollY))
    {
        ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableSetupColumn("Full Path", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (const EffectAssetEntry& asset_ : *assets_)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(asset_.key.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(asset_.fileName.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(asset_.fullPath.c_str());
        }

        ImGui::EndTable();
    }
}
