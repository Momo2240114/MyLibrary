#pragma once

#include "EditorWorkspaceWindow.h"
#include "Editor/Window/AssetBrowserWindow.h"
#include "Engine/GamePlay/AI/AIBehaviorGraph.h"

#include <memory>
#include <string>

class AIControllerComponent;
class DragonAIComponent;
class HumanBossAIComponent;
class GameObject;

namespace ax::NodeEditor
{
    struct EditorContext;
}

class AIBehaviorEditorWindow : public EditorWorkspaceWindow
{
public:
    explicit AIBehaviorEditorWindow(GameObjectManager* manager_);
    ~AIBehaviorEditorWindow() override;

    void Draw() override;

    bool ShouldPauseScene() const override
    {
        return true;
    }

    bool ShouldRenderSceneWhenPaused() const override
    {
        return true;
    }

private:
    struct BehaviorGraphTarget
    {
        // 通常AI、DragonAI、人型ボスAIの編集対象を共通に扱うための一時ターゲット情報。
        AIControllerComponent* aiController = nullptr;
        DragonAIComponent* dragonAI = nullptr;
        HumanBossAIComponent* humanBossAI = nullptr;
        GameObject* owner = nullptr;

        bool IsValid() const
        {
            return aiController != nullptr || dragonAI != nullptr || humanBossAI != nullptr;
        }
    };

    BehaviorGraphTarget ResolveTarget();
    void DrawToolbar(const BehaviorGraphTarget& target_);
    void DrawTargetList();
    void DrawAssetBrowserPanel();
    void DrawGraph(const BehaviorGraphTarget& target_);
    void DrawGraphAssetBar(const BehaviorGraphTarget& target_);
    void DrawInspector(const BehaviorGraphTarget& target_);
    void DrawNode(AIBehaviorNode& node_);
    // コメント領域を背景として描き、内側のノードとまとめて移動できるようにする。
    void DrawArea(AIBehaviorArea& area_);
    // 選択中ノードを囲む領域を作成する。
    void CreateAreaFromSelection(AIBehaviorGraph& graph_);
    bool IsRuntimeActiveNode(const AIBehaviorNode& node_) const;
    void AddNode(const BehaviorGraphTarget& target_, AI_BEHAVIOR_NODE_TYPE type_, const char* name_);
    void DrawCreateNodePopup(const BehaviorGraphTarget& target_);
    void RequestAddNode(const BehaviorGraphTarget& target_, AI_BEHAVIOR_NODE_TYPE type_, const char* name_);
    void UpdatePriorityFromNodePositions(AIBehaviorGraph& graph_);
    bool GetPinInfo(const AIBehaviorGraph& graph_, uintptr_t pinId_, bool& isInput_, uintptr_t& nodeId_) const;
    const char* GetNodeTypeName(AI_BEHAVIOR_NODE_TYPE type_) const;
    AIBehaviorGraph* GetGraph(const BehaviorGraphTarget& target_) const;
    std::string* GetGraphAssetPath(const BehaviorGraphTarget& target_) const;
    void ResetGraphRuntime(const BehaviorGraphTarget& target_) const;
    bool SaveGraphAsset(const BehaviorGraphTarget& target_, const std::string& path_) const;
    bool LoadGraphAsset(const BehaviorGraphTarget& target_, const std::string& path_) const;

private:
    std::weak_ptr<GameObject> targetObject;
    ax::NodeEditor::EditorContext* nodeEditorContext = nullptr;
    AssetBrowserWindow assetBrowser;
    // 選択したBoxやノードに関係するリンクを見分けやすくする表示設定。
    bool focusSelectedLinks = true;
    // DragonAIの現在アクション名。PLAY_ACTIONノードのランタイム強調に使う。
    std::string activeActionName;
    uintptr_t selectedNodeId = 0;
    uintptr_t focusAreaId = 0;
    uintptr_t nodeToPlaceId = 0;
    // 右クリックで新規作成したノードを置くキャンバス座標。
    DirectX::XMFLOAT2 nextNodePosition = { 220.0f, 180.0f };
    bool restoreNodePositions = true;
    std::string graphAssetStatus;
    float targetListWidth = 260.0f;
    float inspectorWidth = 340.0f;
    float assetBrowserHeight = 260.0f;
    // 初回だけ現在ウィンドウサイズから分割レイアウトを初期化するためのフラグ。
    bool layoutSizeInitialized = false;
};
