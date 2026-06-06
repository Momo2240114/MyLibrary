#pragma once

#include "Editor/Workspace/EditorWorkspaceWindow.h"
#include "Editor/Workspace/EditorRenderTarget.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include "Engine/Graphics/RenderSystem/ParticleRenderSystem.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include <vector>

class AnimationControllerComponent;
class GltfModelCommponennt;
class PoseLibraryResource;
struct CustomAnimation;

class AnimationEditorWindow : public EditorWorkspaceWindow
{
public:
    explicit AnimationEditorWindow(GameObjectManager* manager_);

    void Draw() override;
    void OnVisibilityChanged(bool visible) override;
    void Update(float deltaTime_) override;

private:
    bool ResolveTarget(GltfModelCommponennt*& modelComp, AnimationControllerComponent*& animController) const;
    void EnsurePreviewScene();
    void DrawPreviewPanel();
    void DrawPreviewInspectorPanel();
    void DrawPreviewHierarchyPanel();
    void DrawEditorTimelinePanel();
    void RenderPreview(const ImVec2& previewSize_);
    GameObjectManager* GetPreviewManager() const;
    void DrawPoseAnimationCreator(GltfModelCommponennt* modelComp, AnimationControllerComponent* animController);
    void DrawPoseKeyframeSequencer(CustomAnimation& anim, PoseLibraryResource& poseLibrary, GltfModelCommponennt* modelComp);
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
    std::string bakedAnimationName = "BakedOriginal";
    int bakeSourceAnimationIndex = 0;
    float bakeSampleInterval = 0.2f;
    std::string bakeStatusMessage;
    bool customPreviewEnabled = false;
    bool editorPlaybackEnabled = false;
    bool editorPlaybackLoop = true;
    float editorPlaybackTime = 0.0f;
    float editorPlaybackSpeed = 1.0f;
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
    std::unique_ptr<GameObjectManager> previewGameObjectManager;
    EditorRenderTarget previewTarget;
    ModelRenderSystem previewModelRenderSystem;
    ParticleRenderSystem previewParticleRenderSystem;
    CameraComponent previewCamera;
    bool isPreviewSceneInitialized = false;
    bool autoUpdatePreview = true;
    bool showPreviewGrid = true;
};

