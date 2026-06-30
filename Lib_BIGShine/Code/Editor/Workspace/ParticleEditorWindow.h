#pragma once

#include "EditorWorkspaceWindow.h"
#include "EditorRenderTarget.h"
#include "Editor/Window/AssetBrowserWindow.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Combat/AttackEffectHitComponent.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include "Engine/Graphics/RenderSystem/ParticleRenderSystem.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"
#include "Engine/Graphics/VFX/MeshVFXRenderSystem.h"

#include "Editor/EditorCameraData.h"
#include "Editor/Utility/EditorPrimitiveRenderer.h"

#include <memory>
#include <string>
#include <vector>

// ParticleとMeshVFXを同じ時間軸で確認・調整するための専用Editor。
// Preview用GameObjectを別に持ち、Sceneを止めた状態で安全にVFXを編集する。
class ParticleEditorWindow : public EditorWorkspaceWindow
{
public:
    explicit ParticleEditorWindow(GameObjectManager* manager_);
    void Draw() override;
    bool ShouldPauseScene() const override { return true; }

private:
    // Timeline上で扱う再生対象の種類。
    enum class TimelineTrackType
    {
        PARTICLE_EMITTER, // ParticleComponent内のEmitter名を対象にする。
        MESH_VFX,         // MeshVFXComponentのPart/Asset再生を対象にする。
    };

    // Timelineの1トラック。開始・終了フレームから秒時間へ変換して再生を制御する。
    struct TimelineTrack
    {
        TimelineTrackType type = TimelineTrackType::PARTICLE_EMITTER; // ParticleかMeshVFXか。
        std::string targetName; // ParticleならEmitter名、MeshVFXなら対象名。
        float startTime = 0.0f; // 秒単位の開始時間。
        float duration = 1.0f;  // 秒単位の再生長。
        int frameStart = 0;     // Timeline表示用の開始フレーム。
        int frameEnd = 60;      // Timeline表示用の終了フレーム。
        bool active = false;    // 現在のsequenceTimeで再生範囲内か。
    };

    void EnsurePreviewObjects();
    void DrawToolbar();
    void DrawPreviewPanel();
    void DrawTimelinePanel();
    void DrawAssetBrowserPanel();
    void DrawInspectorPanel();
    void RenderPreview(const ImVec2& previewSize_);
    void RefreshParticleAssets();
    void RefreshSequenceAssets();
    void UpdateSequence(float deltaTime_);
    void ResetSequence();
    bool SaveSequence(const std::string& path_);
    bool LoadSequence(const std::string& path_);


private:
    EditorPrimitiveRenderer editorPrimitiveRenderer; // Preview Grid / Axis描画用。
    EditorRenderTarget previewTarget;       // Preview描画先のRenderTarget。
    ModelRenderSystem modelRenderSystem;    // Previewモデル描画用。
    ParticleRenderSystem particleRenderSystem; // Preview Particle描画用。
    MeshVFXRenderSystem meshVFXRenderSystem;   // Preview MeshVFX描画用。
    AssetBrowserWindow assetBrowser;        // Asset選択用ブラウザー。
    CameraComponent previewCamera;          // Preview専用カメラ。
    EditorCameraData editorCamera;         // Preview共通FreeCamera制御。
    std::shared_ptr<GameObject> previewObject; // Preview上のVFX親Object。
    std::shared_ptr<GltfModelCommponennt> previewModel; // 参照モデル。
    std::shared_ptr<ParticleComponent> previewParticle; // 編集対象Particle。
    std::shared_ptr<MeshVFXComponent> previewMeshVFX;   // 編集対象MeshVFX。
    std::shared_ptr<AttackEffectHitComponent> previewAttackHit; // VFX同期攻撃判定。
    std::vector<std::string> particleAssetPaths;        // Particle Asset候補。
    std::vector<std::string> sequenceAssetPaths;        // Sequence Asset候補。
    std::vector<TimelineTrack> timelineTracks;          // Timeline上の再生トラック一覧。
    int selectedAssetIndex = -1;       // 選択中Particle Asset。
    int selectedSequenceIndex = -1;    // 選択中Sequence Asset。
    int selectedTimelineTrack = -1;    // 選択中Timeline Track。
    int firstTimelineFrame = 0;        // Timeline表示の左端フレーム。
    char sequenceNameBuffer[128] = "NewVFXSequence"; // Sequence保存名入力。
    float sequenceTime = 0.0f;         // 現在のSequence再生時間。
    float sequenceDuration = 5.0f;     // Sequence全体の長さ。
    float inspectorWidth = 360.0f;     // Inspectorペイン幅。
    float timelineHeight = 190.0f;     // Timelineペイン高さ。
    float assetBrowserHeight = 280.0f; // AssetBrowserペイン高さ。
    bool isInitialized = false;        // Preview Object初期化済みか。
    bool sequencePlaying = false;      // Sequenceを再生中か。
    bool sequenceLoop = true;          // Sequence末尾でループするか。
    bool timelineExpanded = true;      // Timeline表示を展開しているか。
    bool showGrid = true;              // PreviewにGridを表示するか。


};
