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
            DrawAssetTable(true);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Mesh VFX"))
        {
            DrawAssetTable(false);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    EndEditorWindow();
}

void EffectAssetListWindow::DrawAssetTable(bool isParticle_)
{
    if (!gameObjectManager) return;

    const EffectManager& effectManager_ = gameObjectManager->GetEffectManager();
    const std::vector<EffectAssetEntry>& assets_ = isParticle_
        ? effectManager_.GetParticleAssets()
        : effectManager_.GetMeshVFXAssets();

    ImGui::Text("Count: %d", static_cast<int>(assets_.size()));
    if (ImGui::BeginTable(
        isParticle_ ? "ParticleEffectAssetTable" : "MeshVFXEffectAssetTable",
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

        for (const EffectAssetEntry& asset_ : assets_)
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
