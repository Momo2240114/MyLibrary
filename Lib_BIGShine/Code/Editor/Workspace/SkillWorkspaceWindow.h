#pragma once

#include "EditorWorkspaceWindow.h"
#include "Editor/EditorCameraData.h"
#include "Editor/Utility/EditorPrimitiveRenderer.h"
#include "EditorRenderTarget.h"
#include "Engine/GamePlay/Combat/SkillDefinition.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include "Engine/Graphics/RenderSystem/ParticleRenderSystem.h"
#include "Engine/Graphics/VFX/MeshVFXRenderSystem.h"

class GameObject;

// プレイヤーと敵の共通Skill Definition、Phase、既存演出をまとめて編集するWorkspace。
class SkillWorkspaceWindow : public EditorWorkspaceWindow
{
public:
    // Sceneと同じWorkspace Selectorへ登録し、初期状態では表示しない。
    explicit SkillWorkspaceWindow(GameObjectManager* manager_);
    // 共通定義と既存演出エディタ、Actorプレビューを表示する。
    void Draw() override;
    // Scene側の選択対象を編集パネルへ同期する。
    void Update(float deltaTime_) override;
    bool ShouldPauseScene() const override { return !simulateScene; }
    bool ShouldRenderSceneWhenPaused() const override { return false; }
    // Workspaceを閉じたら編集時のScene停止へ戻す。
    void OnVisibilityChanged(bool visible_) override;

private:
    struct PhaseTimelineAdapter;
    // ImSequencerへ渡す表示区間と編集開始前の尺。
    struct PhaseFrame
    {
        int start = 0;
        int end = 1;
        int originalStart = 0;
        int originalEnd = 1;
    };

    // 共通JSONの作成・読み込み・保存と、現在の編集内容を同期する。
    void LoadLibrary();
    bool SaveLibrary();
    void CommitDraft();
    void SelectSkill(int index_);
    void AddSkill(bool duplicate_);
    // Definition、Phase、入力表、AI接続をそれぞれのパネルへ表示する。
    void DrawToolbar();
    void DrawLibraryPanel();
    void DrawInspector();
    void DrawPhaseInspector();
    void DrawInputBindings();
    void DrawTimeline();
    void DrawExecutionPanel();
    // 保存済みDefinitionを選択Actorの既存Player/AI経路へ反映する。
    bool ApplyBinding();
    void PlaySelected();
    void StopSelected();
    // 対象モデルの尺を使ってPhaseを時間軸へ表示する。
    float GetPhaseDisplayDuration(const SkillPhase& phase_) const;
    void SyncPhaseFrames();
    // 既存Attack Workspaceと同じ描画APIでSceneを専用RTへ表示する。
    void EnsurePreviewResources();
    void DrawPreviewPanel();
    void RenderPreview(const ImVec2& previewSize_);

    std::weak_ptr<GameObject> targetObject;
    nlohmann::json document = { { "Inputs", nlohmann::json::array() }, { "Skills", nlohmann::json::array() } };
    SkillDefinition draft;
    std::string libraryPath = "Data/Skills/PlayerSkills.json";
    std::string statusText;
    std::string aiActionName = "AI.Skill";
    int selectedSkill = -1;
    int selectedPhase = -1;
    int selectedDragonAction = 0;
    int selectedPlayerSlot = 0;
    int timelineFrame = 0;
    int timelineFirstFrame = 0;
    bool dirty = false;
    bool timelineExpanded = true;
    bool timelineEditing = false;
    bool simulateScene = false;
    std::vector<PhaseFrame> phaseFrames;
    EditorPrimitiveRenderer editorPrimitiveRenderer;
    EditorRenderTarget previewTarget;
    ModelRenderSystem previewModelRenderSystem;
    ParticleRenderSystem previewParticleRenderSystem;
    MeshVFXRenderSystem previewMeshVFXRenderSystem;
    CameraComponent previewCamera;
    EditorCameraData editorCamera;
    bool isPreviewResourceInitialized = false;
    bool showGrid = true;
};
