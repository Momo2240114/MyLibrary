#pragma once

#include "EditorWindow.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include <vector>

class AnimationControllerComponent;
class GltfModelCommponennt;
class PoseLibraryResource;
struct CustomAnimation;

class AnimationEditorWindow : public EditorWindow
{
public:
    AnimationEditorWindow(GameObjectManager* manager)
        : EditorWindow("Animation Editor", manager)
    {
        isVisible = false;
    }

    void Draw() override;
    void OnVisibilityChanged(bool visible) override;

private:
    bool ResolveTarget(GltfModelCommponennt*& modelComp, AnimationControllerComponent*& animController) const;
    void DrawPoseAnimationCreator(GltfModelCommponennt* modelComp, AnimationControllerComponent* animController);
    void DrawPoseKeyframeSequencer(CustomAnimation& anim, PoseLibraryResource& poseLibrary, float currentTime);
    void DrawEventSequencer(GltfModelCommponennt* modelComp, AnimationControllerComponent* animController);
    void DrawPosePreview(GltfModelCommponennt* modelComp, AnimationControllerComponent* animController);
    void DrawPoseEditMode(GltfModelCommponennt* modelComp, AnimationControllerComponent* animController);
    void SampleEditPoseFromAnimation(GltfModelCommponennt* modelComp);
    void ApplyEditPoseToModel(GltfModelCommponennt* modelComp);
    void DrawSelectedNodeGizmo(GltfModelCommponennt* modelComp);

    int selectedPose = -1;
    int selectedCustom = -1;
    int selectedKeyframe = -1;
    int selectedEvent = -1;
    int firstPoseFrame = 0;
    int firstEventFrame = 0;
    bool poseSequencerExpanded = true;
    bool eventSequencerExpanded = true;
    std::string selectedEventStateName;
    std::string newPoseName = "Pose";
    std::string newCustomName = "CustomAnim";
    float newCustomDuration = 1.0f;
    bool poseEditEnabled = false;
    bool poseEditPoseDirty = false;
    int poseEditAnimIndex = 0;
    float poseEditTime = 0.0f;
    int selectedEditNode = -1;
    int firstEditFrame = 0;
    bool editScrubExpanded = true;
    bool showNodeHierarchy = true;
    int poseEditListMode = 0;
    int gizmoOperation = 2;
    std::string editPoseName = "EditedPose";
    std::vector<GltfModelCommponennt::NodePose> editPoseBuffer;
};

