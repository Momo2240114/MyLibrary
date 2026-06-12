#include "ActionSequenceEditorWindow.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/EventSystem/ActionSequence.h"

ActionSequenceEditorWindow::ActionSequenceEditorWindow(GameObjectManager* manager_)
    : EditorWindow("Action Sequence Editor", manager_)
{
    isVisible = false;
    defaultWindowSize = ImVec2(1100.0f, 720.0f);
    style.backgroundColor = ImVec4(0.08f, 0.09f, 0.11f, 1.0f);
    style.titleBarColor = ImVec4(0.18f, 0.28f, 0.42f, 1.0f);
}

void ActionSequenceEditorWindow::Draw()
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

    auto selectedObject_ = gameObjectManager->GetSelectedObject();
    if (!selectedObject_)
    {
        ImGui::TextDisabled("Select a GameObject with an ActionSequence component.");
        EndEditorWindow();
        return;
    }

    auto actionSequence_ = selectedObject_->GetComponent<ActionSequence>();
    if (!actionSequence_)
    {
        ImGui::TextDisabled("Selected GameObject does not have an ActionSequence component.");
        EndEditorWindow();
        return;
    }

    ImGui::Text("Target: %s", selectedObject_->GetName().c_str());
    ImGui::Separator();
    actionSequence_->DrawEditorGui();

    EndEditorWindow();
}
