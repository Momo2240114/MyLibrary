#include "AnimNodeEditorWindow.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"

bool AnimNodeEditorWindow::ResolveTarget(GltfModelCommponennt*& modelComp, AnimationControllerComponent*& animController) const
{
    modelComp = nullptr;
    animController = nullptr;

    if (!gameObjectManager) return false;

    auto _SelectedObject = gameObjectManager->GetSelectedObject();
    if (!_SelectedObject) return false;

    auto model = _SelectedObject->GetComponent<GltfModelCommponennt>();
    auto animation = _SelectedObject->GetComponent<AnimationControllerComponent>();
    if (!model || !animation) return false;

    modelComp = model.get();
    animController = animation.get();
    return true;
}

void AnimNodeEditorWindow::Draw()
{
    GltfModelCommponennt* modelComp = nullptr;
    AnimationControllerComponent* animController = nullptr;

    if (!ResolveTarget(modelComp, animController))
    {
        ImGui::SetNextWindowSize(ImVec2(420, 140), ImGuiCond_FirstUseEver);
        ImGui::Begin(windowName.c_str(), &isVisible);
        DrawStylePopup();
        ImGui::TextWrapped("Select a GameObject that has both GltfModel and AnimationControllerComponent.");
        ImGui::End();
        return;
    }

    animController->OnGuiNodeEditor();
}

