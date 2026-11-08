#pragma once

#include "EditorWorkspaceWindow.h"
#include "Editor/EditorCameraData.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

class BossFinishDirector;
class AssetBrowserWindow;
class CinematicSequenceEditor;

// ActionSequenceの映像付き編集を中心に、既存のとどめプリセット編集も保持するWorkspace。
class CinematicEditorWindow : public EditorWorkspaceWindow
{
public:
    explicit CinematicEditorWindow(GameObjectManager* manager_);
    ~CinematicEditorWindow() override;
    // Workspaceの映像、Inspector、トラック、タイムラインを描画する。
    void Draw() override;
    // シーク要求を集約し、ループとプレビュー倍率を更新する。
    void Update(float deltaTime_) override;
    // Workspaceを離れる時は、このEditorが開始したプレビューだけを復元する。
    void OnVisibilityChanged(bool visible_) override;
    bool ShouldPauseScene() const override { return true; }
    bool ShouldRenderSceneWhenPaused() const override { return true; }
    bool RequiresSceneRenderTarget() const override { return true; }
    bool OpenCinematicSequence(const std::string& ownerName_, int sequenceIndex_) override;

private:
    enum class TrackType { PLAYER, BOSS, CAMERA, EFFECT, IMPACT, CAPTURE };
    enum class DragMode { NONE, MOVE, START, END };
    // 表示用トラックは毎フレーム本番設定から再構築し、独自の再生データを持たない。
    struct Track
    {
        TrackType type = TrackType::PLAYER;
        int index = -1;
        std::string label;
        float start = 0.0f;
        float end = 0.0f;
        ImU32 color = 0;
    };
    // 対象Directorと、現在のプレビュー時刻・表示範囲を取得する。
    BossFinishDirector* GetDirector() const;
    // シーク中は要求時刻を表示し、映像の再構築待ちでもカーソルを追従させる。
    float GetTime() const;
    // 決め絵の撮影とクリア文字の表示までを編集範囲へ含める。
    float GetEndTime() const;
    // Scene内のモデルを確認し、本番とは別のプレビュー再生を開始する。
    void StartPreview(bool playing_);
    // 自分が開始したプレビューを停止し、Directorの復元処理へ委譲する。
    void StopPreview();
    // 重いVFXの再評価はUpdateへ予約し、GUI走査中にリソースを作り直さない。
    void RequestSeek(float time_);
    // 再生、フレーム送り、ループ、保存、Undoをまとめる。
    void DrawToolbar();
    // 本番の最終出力画像と、構図調整用のガイドを表示する。
    void DrawPreview();
    // 選択したアニメーション、カメラ、VFXの実データを編集する。
    void DrawInspector();
    // 秒とフレームの目盛り、範囲クリップ、命中位置を同じ時間軸に配置する。
    void DrawTimeline();
    // プリセット内の基準時刻を、画面上の共通時計へ変換する。
    void BuildTracks();
    // JSONの保存値を維持し、フレーム格子に沿って移動・範囲変更する。
    void ApplyTrackDrag(const Track& track_, float delta_);
    // 継続するドラッグを一回のUndoとして記録し、プレビューを再構築する。
    void CommitEdits(const nlohmann::json& before_);
    // 設定を履歴から復元し、対応するプレビューの再構築を予約する。
    void RestoreHistory(bool redo_);
    // ファイル型を確認してVFXを追加し、欠けた必須項目を作らない。
    void AddEffect(const std::string& path_);
    // 現在見ているカメラの構図を、演出時計上のキーへ記録する。
    void AddCameraKey();

    std::vector<Track> tracks;
    std::vector<nlohmann::json> undoHistory;
    std::vector<nlohmann::json> redoHistory;
    nlohmann::json editStart;
    EditorCameraData freeCamera;
    std::unique_ptr<AssetBrowserWindow> assetBrowser;
    std::unique_ptr<CinematicSequenceEditor> sequenceEditor;
    bool useSequenceEditor = true;
    std::string presetPath = "Data/Presentation/AnathemaBossFinish.json";
    std::string status;
    TrackType selectedType = TrackType::PLAYER;
    int selectedIndex = -1;
    int selectedCameraKey = -1;
    int draggingTrack = -1;
    DragMode dragMode = DragMode::NONE;
    float dragMouseX = 0.0f;
    float dragStart = 0.0f;
    float dragEnd = 0.0f;
    float seekTime = 0.0f;
    float seekCooldown = 0.0f;
    float previewRate = 1.0f;
    float loopStart = 0.0f;
    float loopEnd = 4.5f;
    float viewStart = 0.0f;
    float viewDuration = 8.0f;
    int frameRate = 60;
    bool ownsPreview = false;
    bool ownsFinishScene = false;
    bool playWhenFinishReady = false;
    bool seekPending = false;
    bool editing = false;
    bool dirty = false;
    bool loop = false;
    bool snap = true;
    bool exploreCamera = false;
    bool safeFrame = true;
};
