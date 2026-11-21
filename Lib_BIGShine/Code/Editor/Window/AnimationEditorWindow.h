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
#include "Editor/EditorCameraData.h"
#include "Engine/Physics/PhysXSystem.h"
#include <vector>

class AnimationControllerComponent;
class GltfModelCommponennt;
class PoseLibraryResource;
struct CustomAnimation;
struct AnimEvent;

/// アニメーション制作に必要なプレビュー、ポーズ編集、イベント編集を統合するワークスペース。
/// ゲーム本編とは独立したプレビューシーンを所有し、実行中のシーンへ影響を与えずに編集結果を確認する。
class AnimationEditorWindow : public EditorWorkspaceWindow
{
public:
    /// アニメーションエディタを生成し、編集元となるシーンのManagerを保持する。
    explicit AnimationEditorWindow(GameObjectManager* manager_);
    ~AnimationEditorWindow() override;

    /// プレビュー、インスペクタ、階層、タイムラインを含むエディタ全体を描画する。
    void Draw() override;

    /// ウィンドウの表示状態変更時に、編集中のプレビューや一時的な再生状態を終了する。
    void OnVisibilityChanged(bool visible) override;

    /// プレビューシーンと編集中アニメーションの再生時間を更新する。
    void Update(float deltaTime_) override;

    /// アニメーション編集中はゲーム本編のScene更新を停止する。
    bool ShouldPauseScene() const override
    {
        return true;
    }

private:
    /// 選択中のプレビューObjectから、編集対象のモデルとアニメーション制御を取得する。
    bool ResolveTarget(
        GltfModelCommponennt*& modelComp,
        AnimationControllerComponent*& animController) const;

    /// 独立したプレビューシーンと描画リソースを初回のみ生成する。
    void EnsurePreviewScene();
    void CreatePreviewFloor();

    /// モデル、エフェクト、デバッグ形状を表示するプレビュー領域を描画する。
    void DrawPreviewPanel();

    /// プレビューシーンで選択中のObjectのInspectorを描画する。
    void DrawPreviewInspectorPanel();

    /// プレビューObjectの生成、複製、親子階層の編集UIを描画する。
    void DrawPreviewHierarchyPanel();

    /// ポーズ、イベント、カーブ編集を切り替えるタイムライン領域を描画する。
    void DrawEditorTimelinePanel();

    /// プレビューシーンを専用RenderTargetへ描画する。
    void RenderPreview(const ImVec2& previewSize_);

    /// 編集専用のGameObjectManagerを返す。
    GameObjectManager* GetPreviewManager() const;

    /// 実シーンのObject階層をプレビュー専用シーンへ複製する。
    bool CopySceneObjectToPreview(const std::shared_ptr<GameObject>& sourceObject_);

    /// ポーズの記録、ベイク、カスタムアニメーション作成UIを描画する。
    void DrawPoseAnimationCreator(
        GltfModelCommponennt* modelComp,
        AnimationControllerComponent* animController);

    /// カスタムアニメーションのポーズキーフレームシーケンサーを描画する。
    void DrawPoseKeyframeSequencer(
        CustomAnimation& anim,
        PoseLibraryResource& poseLibrary,
        GltfModelCommponennt* modelComp);

    /// アニメーションStateに登録されたイベントトラックと詳細設定を描画する。
    void DrawEventSequencer(
        GltfModelCommponennt* modelComp,
        AnimationControllerComponent* animController);

    /// アニメーションStateへ割り当てる速度・RootMotion用カーブを編集する。
    void DrawCurveEditor(AnimationControllerComponent* animController_);

    /// 選択中のMeshVFXイベントをプレビューシーン上で再生する。
    void PlaySelectedEffectPreview(const AnimEvent& event_);

    /// 選択中イベントから生成したMeshVFXプレビューを停止する。
    void StopSelectedEffectPreview();

    /// AttackTemplateに登録された命中エフェクトをプレビュー再生する。
    void PlayAttackTemplateEffectPreview(const std::string& templateId_);

    /// AttackTemplateから生成した全てのプレビューエフェクトを停止する。
    void StopAttackTemplateEffectPreview();

    /// モデルのノードを直接操作するポーズ編集UIを描画する。
    void DrawPoseEditMode(
        GltfModelCommponennt* modelComp,
        AnimationControllerComponent* animController);

    /// 指定アニメーションの現在時刻からノード姿勢をサンプリングする。
    void SampleEditPoseFromAnimation(GltfModelCommponennt* modelComp);

    /// 編集中のノード姿勢をモデルへ反映し、ワールド行列を更新する。
    void ApplyEditPoseToModel(GltfModelCommponennt* modelComp);

    /// 選択中ノードを操作するためのImGuizmoを描画する。
    void DrawSelectedNodeGizmo(GltfModelCommponennt* modelComp);

    // ポーズライブラリで現在選択中のポーズ番号。
    int selectedPose = -1;

    // 現在編集中のカスタムアニメーション番号。
    int selectedCustom = -1;

    // ポーズシーケンサーで現在選択中のキーフレーム番号。
    int selectedKeyframe = -1;

    // イベントシーケンサーで現在選択中のイベント番号。
    int selectedEvent = -1;

    // ポーズシーケンサー左端に表示する先頭フレーム。
    int firstPoseFrame = 0;

    // イベントシーケンサー左端に表示する先頭フレーム。
    int firstEventFrame = 0;

    // ポーズシーケンサーのトラック表示を展開するか。
    bool poseSequencerExpanded = true;

    // イベントシーケンサーのトラック表示を展開するか。
    bool eventSequencerExpanded = true;

    // イベント編集対象として選択中のアニメーションState名。
    std::string selectedEventStateName;

    // カーブ編集対象として選択中のアニメーションState名。
    std::string selectedCurveStateName;

    // イベント編集用State一覧に適用する検索文字列。
    std::string eventStateSearchText;

    // カーブ編集用State一覧に適用する検索文字列。
    std::string curveStateSearchText;

    // 登録済みState一覧に適用する検索文字列。
    std::string registeredStateSearchText;

    // 新規記録するポーズの初期名。
    std::string newPoseName = "Pose";

    // 新規作成するカスタムアニメーションの初期名。
    std::string newCustomName = "CustomAnim";

    // 新規カスタムアニメーションの初期再生時間。
    float newCustomDuration = 1.0f;

    // ベイクして保存するアニメーションの出力名。
    std::string bakedAnimationName = "BakedOriginal";

    // ベイク元として選択中のモデルアニメーション番号。
    int bakeSourceAnimationIndex = 0;

    // アニメーションをポーズへベイクするときのサンプリング間隔。
    float bakeSampleInterval = 0.2f;

    // ベイクおよび保存処理の結果を表示するメッセージ。
    std::string bakeStatusMessage;

    // カスタムアニメーションをプレビューへ適用するか。
    bool customPreviewEnabled = false;

    // エディタ内のカスタムアニメーションを自動再生するか。
    bool editorPlaybackEnabled = false;

    // エディタ内再生をループするか。
    bool editorPlaybackLoop = true;

    // エディタ内プレビューの現在再生時間。
    float editorPlaybackTime = 0.0f;

    // エディタ内プレビューの再生速度倍率。
    float editorPlaybackSpeed = 1.0f;

    // ノード姿勢の直接編集モードが有効か。
    bool poseEditEnabled = false;

    // 編集中のノード姿勢に未保存の変更があるか。
    bool poseEditPoseDirty = false;

    // ポーズ編集の参照元となるアニメーション番号。
    int poseEditAnimIndex = 0;

    // ポーズ編集でサンプリングするアニメーション時刻。
    float poseEditTime = 0.0f;

    // ポーズ編集で現在選択中のモデルノード番号。
    int selectedEditNode = -1;

    // ポーズ編集用スクラブシーケンサーの先頭表示フレーム。
    int firstEditFrame = 0;

    // ポーズ編集用スクラブシーケンサーを展開するか。
    bool editScrubExpanded = true;

    // ノード一覧を親子階層形式で表示するか。
    bool showNodeHierarchy = true;

    // ノード一覧の表示形式を示す番号。
    int poseEditListMode = 0;

    // ImGuizmoで使用する移動、回転、拡縮の操作番号。
    int gizmoOperation = 2;

    // 編集したポーズを保存するときの初期名。
    std::string editPoseName = "EditedPose";

    // ポーズ編集で使用する全ノードの一時姿勢データ。
    std::vector<GltfModelCommponennt::NodePose> editPoseBuffer;

    PhysXSystem::PreviewScene previewPhysics;

    // ゲーム本編から分離された編集専用GameObjectManager。
    std::unique_ptr<GameObjectManager> previewGameObjectManager;

    // プレビューへ複製するObjectを参照するための実シーン側Manager。
    GameObjectManager* sceneGameObjectManager = nullptr;

    // プレビュー描画結果を保持する専用RenderTarget。
    EditorRenderTarget previewTarget;

    // プレビュー内のモデルを描画するRenderSystem。
    ModelRenderSystem previewModelRenderSystem;

    // プレビュー内のParticleを描画するRenderSystem。
    ParticleRenderSystem previewParticleRenderSystem;

    // プレビュー内のMeshVFXを描画するRenderSystem。
    MeshVFXRenderSystem previewMeshVFXRenderSystem;

    // アニメーションやエフェクトのアセット選択に使用するBrowser。
    AssetBrowserWindow assetBrowser;

    // プレビューシーン専用のCamera。
    CameraComponent previewCamera;
    EditorCameraData editorCamera;

    // 上下分割レイアウトにおけるタイムライン領域の高さ。
    float timelineHeight = 300.0f;

    // タイムライン右側に表示するAssetBrowser領域の幅。
    float assetBrowserWidth = 360.0f;

    // 分割レイアウトの初期サイズを設定済みか。
    bool layoutSizeInitialized = false;

    // プレビューシーンの初期化が完了しているか。
    bool isPreviewSceneInitialized = false;

    // 毎フレーム、プレビュー専用Managerを更新するか。
    bool autoUpdatePreview = true;

    // プレビュー内にデバッグ用Gridを表示するか。
    bool showPreviewGrid = true;

    // 選択中イベントから生成したMeshVFXプレビューObject。
    std::weak_ptr<GameObject> selectedEffectPreviewObject;

    // AttackTemplateから生成した複数のプレビューエフェクト。
    std::vector<std::weak_ptr<GameObject>> attackTemplateEffectPreviewObjects;

    // 選択中イベントのMeshVFXを繰り返し再生するか。
    bool loopSelectedEffectPreview = false;

    // AttackTemplateの命中エフェクトを繰り返し再生するか。
    bool loopAttackTemplateEffectPreview = false;

    // 選択中イベントのMeshVFXを再生してからの経過時間。
    float selectedEffectPreviewTimer = 0.0f;

    // AttackTemplateの命中エフェクトを再生してからの経過時間。
    float attackTemplateEffectPreviewTimer = 0.0f;

    // AttackTemplateプレビューを再生成するまでの待機時間。
    float attackTemplateEffectPreviewDuration = 1.0f;

    // 実シーンから複製するときに位置と回転を維持するか。
    bool keepSceneTransformOnCopy = false;

    // 実シーン側のObject一覧で選択中の番号。
    int selectedSceneObjectIndex = -1;

    // 実シーンからプレビューへ複製した結果を表示するメッセージ。
    std::string sceneObjectCopyStatus;
};
