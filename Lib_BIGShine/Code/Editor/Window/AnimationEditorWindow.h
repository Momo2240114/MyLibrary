#pragma once

#include "Editor/Workspace/EditorWorkspaceWindow.h"
#include "Editor/Workspace/EditorRenderTarget.h"
#include "Editor/Window/AssetBrowserWindow.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include "Engine/Graphics/RenderSystem/ParticleRenderSystem.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Graphics/VFX/MeshVFXRenderSystem.h"
#include <vector>

class AnimationControllerComponent;
class GltfModelCommponennt;
class PoseLibraryResource;
struct CustomAnimation;
struct AnimEvent;

/// アニメーション制作に必要なプレビュー、ポーズ編集、イベント編集を統合したワークスペース。
/// 編集対象とは独立したプレビューシーンを所有し、ゲーム本編の状態を変更せずに結果を確認できる。
class AnimationEditorWindow : public EditorWorkspaceWindow
{
public:
    explicit AnimationEditorWindow(GameObjectManager* manager_);

    void Draw() override;
    void OnVisibilityChanged(bool visible) override;
    void Update(float deltaTime_) override;
    bool ShouldPauseScene() const override { return true; }
private:
    // 編集対象のモデルとアニメーション制御コンポーネントを安全に解決する。
    bool ResolveTarget(GltfModelCommponennt*& modelComp, AnimationControllerComponent*& animController) const;

    // 独立したプレビューシーンの生成・表示を担当する。
    void EnsurePreviewScene();
    void DrawPreviewPanel();
    void DrawPreviewInspectorPanel();
    void DrawPreviewHierarchyPanel();
    void DrawEditorTimelinePanel();
    void RenderPreview(const ImVec2& previewSize_);
    GameObjectManager* GetPreviewManager() const;
    bool CopySceneObjectToPreview(const std::shared_ptr<GameObject>& sourceObject_);

    // ポーズアニメーションとイベントトラックの編集UIを構築する。
    void DrawPoseAnimationCreator(GltfModelCommponennt* modelComp, AnimationControllerComponent* animController);
    void DrawPoseKeyframeSequencer(CustomAnimation& anim, PoseLibraryResource& poseLibrary, GltfModelCommponennt* modelComp);
    void DrawEventSequencer(GltfModelCommponennt* modelComp, AnimationControllerComponent* animController);
    void DrawCurveEditor(AnimationControllerComponent* animController_);
    void PlaySelectedEffectPreview(const AnimEvent& event_);
    void StopSelectedEffectPreview();
    void PlayAttackTemplateEffectPreview(const std::string& templateId_);
    void StopAttackTemplateEffectPreview();

    // モデルのノード姿勢を直接編集し、ポーズデータへ反映する。
    void DrawPoseEditMode(GltfModelCommponennt* modelComp, AnimationControllerComponent* animController);
    void SampleEditPoseFromAnimation(GltfModelCommponennt* modelComp);
    void ApplyEditPoseToModel(GltfModelCommponennt* modelComp);
    void DrawSelectedNodeGizmo(GltfModelCommponennt* modelComp);

    // タイムライン上で現在選択されている編集要素。
    int selectedPose = -1;
    int selectedCustom = -1;
    int selectedKeyframe = -1;
    int selectedEvent = -1;

    // シーケンサーの表示範囲と展開状態。
    int firstPoseFrame = 0;
    int firstEventFrame = 0;
    bool poseSequencerExpanded = true;
    bool eventSequencerExpanded = true;

    // 新規アセット作成およびベイク処理の入力値。
    std::string selectedEventStateName;
    std::string selectedCurveStateName;
    std::string eventStateSearchText;
    std::string curveStateSearchText;
    std::string registeredStateSearchText;
    std::string newPoseName = "Pose";
    std::string newCustomName = "CustomAnim";
    float newCustomDuration = 1.0f;
    std::string bakedAnimationName = "BakedOriginal";
    int bakeSourceAnimationIndex = 0;
    float bakeSampleInterval = 0.2f;
    std::string bakeStatusMessage;

    // エディタ内プレビュー再生の状態。ゲーム本編の再生状態とは分離して管理する。
    bool customPreviewEnabled = false;
    bool editorPlaybackEnabled = false;
    bool editorPlaybackLoop = true;
    float editorPlaybackTime = 0.0f;
    float editorPlaybackSpeed = 1.0f;

    // ポーズ直接編集モードの作業状態。
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

    // 編集専用シーンと描画リソース。実ゲームのGameObjectManagerには登録しない。
    std::unique_ptr<GameObjectManager> previewGameObjectManager;
    GameObjectManager* sceneGameObjectManager = nullptr;
    EditorRenderTarget previewTarget;
    ModelRenderSystem previewModelRenderSystem;
    ParticleRenderSystem previewParticleRenderSystem;
    MeshVFXRenderSystem previewMeshVFXRenderSystem;
    AssetBrowserWindow assetBrowser;
    CameraComponent previewCamera;
    float timelineHeight = 300.0f;
    float assetBrowserWidth = 360.0f;
    bool layoutSizeInitialized = false;
    bool isPreviewSceneInitialized = false;
    bool autoUpdatePreview = true;
    bool showPreviewGrid = true;
    std::weak_ptr<GameObject> selectedEffectPreviewObject;
    std::vector<std::weak_ptr<GameObject>> attackTemplateEffectPreviewObjects;
    bool loopSelectedEffectPreview = false;
    bool loopAttackTemplateEffectPreview = false;
    float selectedEffectPreviewTimer = 0.0f;
    float attackTemplateEffectPreviewTimer = 0.0f;
    float attackTemplateEffectPreviewDuration = 1.0f;
    bool keepSceneTransformOnCopy = false;
    int selectedSceneObjectIndex = -1;
    std::string sceneObjectCopyStatus;
};

