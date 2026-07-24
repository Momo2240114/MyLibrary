#include "PrecompiledHeader.h"

#include "ComponentUpdateSequencerWindow.h"

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"

#include <algorithm>

ComponentUpdateSequencerWindow::ComponentUpdateSequencerWindow(GameObjectManager* manager_)
    : EditorWindow("Component Update Sequencer", manager_)
{
    isVisible = false;
    defaultWindowSize = ImVec2(960.0f, 620.0f);
    style.backgroundColor = ImVec4(0.08f, 0.09f, 0.10f, 1.0f);
    style.titleBarColor = ImVec4(0.20f, 0.25f, 0.32f, 1.0f);
}

void ComponentUpdateSequencerWindow::Draw()
{
    if (!BeginEditorWindow(nullptr, ImGuiWindowFlags_NoCollapse))
    {
        EndEditorWindow();
        return;
    }

    DrawStylePopup();

    if (!gameObjectManager)
    {
        ImGui::TextDisabled("GameObjectManager is unavailable.");
        EndEditorWindow();
        return;
    }

    const std::shared_ptr<GameObject> selectedObject_ = gameObjectManager->GetSelectedObject();
    if (!selectedObject_)
    {
        ImGui::TextDisabled("Select a GameObject to edit component update timing.");
        EndEditorWindow();
        return;
    }

    ImGui::Text("Target: %s", selectedObject_->GetName().c_str());
    ImGui::Separator();

    DrawPhaseTabs();
    ImGui::SameLine();
    ImGui::Checkbox("Colliders", &showColliders);
    ImGui::SameLine();
    ImGui::Checkbox("Disabled", &showDisabledComponents);

    ImGui::Spacing();
    DrawComponentTable(selectedObject_);

    EndEditorWindow();
}

void ComponentUpdateSequencerWindow::DrawPhaseTabs()
{
    if (ImGui::RadioButton("Update", selectedPhase == COMPONENT_UPDATE_PHASE::UPDATE))
    {
        selectedPhase = COMPONENT_UPDATE_PHASE::UPDATE;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("LateUpdate", selectedPhase == COMPONENT_UPDATE_PHASE::LATE_UPDATE))
    {
        selectedPhase = COMPONENT_UPDATE_PHASE::LATE_UPDATE;
    }
}

void ComponentUpdateSequencerWindow::DrawComponentTable(const std::shared_ptr<GameObject>& object_)
{
    std::vector<ComponentRow> rows_ = BuildRows(object_);

    std::stable_sort(rows_.begin(), rows_.end(),
        [this](const ComponentRow& left_, const ComponentRow& right_)
        {
            const int leftPriority_ = selectedPhase == COMPONENT_UPDATE_PHASE::UPDATE
                ? left_.component->GetUpdatePriority()
                : left_.component->GetLateUpdatePriority();
            const int rightPriority_ = selectedPhase == COMPONENT_UPDATE_PHASE::UPDATE
                ? right_.component->GetUpdatePriority()
                : right_.component->GetLateUpdatePriority();

            if (leftPriority_ != rightPriority_) return leftPriority_ < rightPriority_;
            if (left_.isCollider != right_.isCollider) return !left_.isCollider;
            return left_.order < right_.order;
        });

    if (ImGui::Button("Reset Visible Timing"))
    {
        for (ComponentRow& row_ : rows_)
        {
            if (row_.component) row_.component->ResetUpdateTiming();
        }
    }

    ImGui::SameLine();
    ImGui::TextDisabled("Lower priority runs earlier.");

    const ImGuiTableFlags tableFlags_ =
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ScrollY;

    if (!ImGui::BeginTable("ComponentUpdateSequencerTable", 6, tableFlags_, ImVec2(0.0f, 0.0f)))
    {
        return;
    }

    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 36.0f);
    ImGui::TableSetupColumn("Component", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("Use", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Shift", ImGuiTableColumnFlags_WidthFixed, 130.0f);
    ImGui::TableHeadersRow();

    for (ComponentRow& row_ : rows_)
    {
        DrawComponentRow(row_);
    }

    ImGui::EndTable();
}

void ComponentUpdateSequencerWindow::DrawComponentRow(ComponentRow& row_)
{
    if (!row_.component) return;

    Component& component_ = *row_.component;

    ImGui::PushID(&component_);

    if (!component_.IsEnabled()) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.45f);

    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%d", row_.order);

    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted(component_.GetName().c_str());

    ImGui::TableSetColumnIndex(2);
    ImGui::TextUnformatted(row_.isCollider ? "Collider" : "Component");

    ImGui::TableSetColumnIndex(3);
    bool usePhase_ = selectedPhase == COMPONENT_UPDATE_PHASE::UPDATE
        ? component_.ShouldUpdate()
        : component_.ShouldLateUpdate();
    if (ImGui::Checkbox("##UsePhase", &usePhase_))
    {
        if (selectedPhase == COMPONENT_UPDATE_PHASE::UPDATE)
        {
            component_.SetUpdateEnabled(usePhase_);
        }
        else
        {
            component_.SetLateUpdateEnabled(usePhase_);
        }
    }

    ImGui::TableSetColumnIndex(4);
    int priority_ = selectedPhase == COMPONENT_UPDATE_PHASE::UPDATE
        ? component_.GetUpdatePriority()
        : component_.GetLateUpdatePriority();
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::DragInt("##Priority", &priority_, 1.0f, -10000, 10000))
    {
        if (selectedPhase == COMPONENT_UPDATE_PHASE::UPDATE)
        {
            component_.SetUpdatePriority(priority_);
        }
        else
        {
            component_.SetLateUpdatePriority(priority_);
        }
    }

    ImGui::TableSetColumnIndex(5);
    if (ImGui::SmallButton("-10")) MoveComponentPriority(component_, -10);
    ImGui::SameLine();
    if (ImGui::SmallButton("+10")) MoveComponentPriority(component_, 10);

    if (!component_.IsEnabled()) ImGui::PopStyleVar();

    ImGui::PopID();
}

std::vector<ComponentUpdateSequencerWindow::ComponentRow> ComponentUpdateSequencerWindow::BuildRows(
    const std::shared_ptr<GameObject>& object_) const
{
    std::vector<ComponentRow> rows_;
    if (!object_) return rows_;

    for (int order_ = 0; order_ < static_cast<int>(object_->components.size()); ++order_)
    {
        const std::shared_ptr<Component>& component_ = object_->components[order_];
        if (!component_) continue;
        if (!showDisabledComponents && !component_->IsEnabled()) continue;

        rows_.push_back({ component_, false, order_ });
    }

    if (showColliders)
    {
        for (int order_ = 0; order_ < static_cast<int>(object_->colliderComponents.size()); ++order_)
        {
            const std::shared_ptr<Component>& component_ = object_->colliderComponents[order_];
            if (!component_) continue;
            if (!showDisabledComponents && !component_->IsEnabled()) continue;

            rows_.push_back({ component_, true, order_ });
        }
    }

    return rows_;
}

void ComponentUpdateSequencerWindow::MoveComponentPriority(Component& component_, int delta_)
{
    if (selectedPhase == COMPONENT_UPDATE_PHASE::UPDATE)
    {
        component_.SetUpdatePriority(component_.GetUpdatePriority() + delta_);
        return;
    }

    component_.SetLateUpdatePriority(component_.GetLateUpdatePriority() + delta_);
}
