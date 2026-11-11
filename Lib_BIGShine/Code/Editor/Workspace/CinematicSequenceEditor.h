#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

class GameObjectManager;

// ActionSequenceと同じデータを編集し、Sceneの本番再生と復元をWorkspaceへ接続する。
class CinematicSequenceEditor
{
public:
    explicit CinematicSequenceEditor(GameObjectManager* manager_) : manager(manager_) {}
    ~CinematicSequenceEditor();
    // Action Sequencerで選んだObjectとSequenceを引き継ぐ。
    bool Bind(const std::string& ownerName_, int sequenceIndex_);
    // 再構築待ち、ループ、シーク予約を更新する。
    void Update(float deltaTime_);
    // 汎用トラック、実映像、Action設定とScene内の演出Componentを表示する。
    void Draw();
    // メモリへ退避したSceneの復元を、Managerの次回更新へ予約する。
    void Stop();
private:
    // 選択中Sequenceの有効性と、試写する範囲を取得する。
    bool HasSequence() const;
    float GetDuration() const;
    void SelectSequence(int index_);
    // 元Sceneの設定へ再生用データを渡し、必要なら先頭から再評価する。
    void Start(bool playing_);
    void Seek(float time_);
    // 編集したSequenceを元Componentへ反映する。生成物や再生中の値は保存しない。
    void Apply();
    // 実行経路は作らず、既存のActionRegistryから呼び出し名を選ぶ。
    void DrawActionInspector();
    // Animation、Particle、MeshVFXの実際の経過時間と寿命を表示する。
    void DrawLiveState();
    // 描画用時計へ対応しないアニメーション基準Actionは、別表記で扱う。
    void DrawTimeline();
    // 現在のScene Objectへ付いている演出設定を、既存GUIで編集する。
    void DrawSceneInspector();
    GameObjectManager* manager = nullptr;
    nlohmann::json draft;
    std::string ownerName;
    std::string inspectObjectName;
    std::string status;
    int sequenceIndex = -1;
    int selectedAction = -1;
    int contextIndex = 0;
    int frameRate = 60;
    float rate = 1.0f;
    float previewDuration = 10.0f;
    float pendingSeek = -1.0f;
    float seekDelay = 0.0f;
    float viewDuration = 12.0f;
    float dragMouseX = 0.0f;
    float dragTime = 0.0f;
    bool ownsPreview = false;
    bool restartPending = false;
    bool restartPlaying = false;
    bool dirty = false;
    bool loop = false;
    bool snap = true;
};
