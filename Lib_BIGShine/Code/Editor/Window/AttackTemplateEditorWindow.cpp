#include "PrecompiledHeader.h"

#include "AttackTemplateEditorWindow.h"

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Combat/AttackTemplateManager.h"

#include <algorithm>
#include <imgui.h>
#include <imgui_stdlib.h>

AttackTemplateEditorWindow::AttackTemplateEditorWindow(GameObjectManager* manager_)
    : EditorWindow("Attack Template Editor", manager_)
{
    defaultWindowSize = ImVec2(720.0f, 760.0f);
    AttackTemplateManager::Instance().LoadAll();
}

void AttackTemplateEditorWindow::SelectFallbackTemplate()
{
    AttackTemplateManager& manager_ = AttackTemplateManager::Instance();
    if (manager_.GetTemplates().empty())
    {
        selectedTemplateId.clear();
        return;
    }

    const auto iterator_ = std::find_if(
        manager_.GetTemplates().begin(),
        manager_.GetTemplates().end(),
        [this](const AttackTemplate& attackTemplate_)
        {
            return attackTemplate_.id == selectedTemplateId;
        });
    if (iterator_ == manager_.GetTemplates().end())
    {
        selectedTemplateId = manager_.GetTemplates().front().id;
    }
}

void AttackTemplateEditorWindow::Draw()
{
    BS_PROFILE_SCOPE("AttackTemplateEditorWindow::Draw");

    if (!BeginEditorWindow())
    {
        EndEditorWindow();
        return;
    }

    DrawStylePopup();

    AttackTemplateManager& manager_ = AttackTemplateManager::Instance();
    SelectFallbackTemplate();

    if (ImGui::Button("Reload"))
    {
        manager_.LoadAll();
        SelectFallbackTemplate();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputText("New ID", &newTemplateIdBuffer);

    ImGui::SameLine();
    const bool canCreate_ =
        !newTemplateIdBuffer.empty() &&
        manager_.FindTemplate(newTemplateIdBuffer) == nullptr;
    ImGui::BeginDisabled(!canCreate_);
    if (ImGui::Button("Create"))
    {
        AttackTemplate newTemplate_{};
        newTemplate_.id = newTemplateIdBuffer;
        newTemplate_.displayName = newTemplateIdBuffer;
        if (manager_.SaveTemplate(newTemplate_))
        {
            manager_.LoadAll();
            selectedTemplateId = newTemplateIdBuffer;
            duplicateTemplateIdBuffer = selectedTemplateId + "_Copy";
        }
    }
    ImGui::EndDisabled();

    const char* preview_ = selectedTemplateId.empty()
        ? "(No Attack Template)"
        : selectedTemplateId.c_str();
    ImGui::SetNextItemWidth(360.0f);
    if (ImGui::BeginCombo("Template", preview_))
    {
        for (const AttackTemplate& attackTemplate_ : manager_.GetTemplates())
        {
            const bool selected_ = attackTemplate_.id == selectedTemplateId;
            const std::string label_ = attackTemplate_.displayName.empty()
                ? attackTemplate_.id
                : attackTemplate_.displayName + "##" + attackTemplate_.id;
            if (ImGui::Selectable(label_.c_str(), selected_))
            {
                selectedTemplateId = attackTemplate_.id;
                duplicateTemplateIdBuffer = selectedTemplateId + "_Copy";
            }
            if (selected_)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputText("Duplicate ID", &duplicateTemplateIdBuffer);
    ImGui::SameLine();
    const bool canDuplicate_ =
        !selectedTemplateId.empty() &&
        !duplicateTemplateIdBuffer.empty() &&
        manager_.FindTemplate(selectedTemplateId) != nullptr &&
        manager_.FindTemplate(duplicateTemplateIdBuffer) == nullptr;
    ImGui::BeginDisabled(!canDuplicate_);
    if (ImGui::Button("Duplicate"))
    {
        if (const AttackTemplate* source_ = manager_.FindTemplate(selectedTemplateId))
        {
            if (manager_.DuplicateTemplate(*source_, duplicateTemplateIdBuffer))
            {
                selectedTemplateId = duplicateTemplateIdBuffer;
                duplicateTemplateIdBuffer = selectedTemplateId + "_Copy";
                manager_.LoadAll();
            }
        }
    }
    ImGui::EndDisabled();

    ImGui::Separator();

    if (!selectedTemplateId.empty())
    {
        manager_.DrawEditorContent(selectedTemplateId, gameObjectManager);
    }
    else
    {
        ImGui::TextDisabled("Data/AttackTemplate に攻撃テンプレートがありません。");
    }

    EndEditorWindow();
}
