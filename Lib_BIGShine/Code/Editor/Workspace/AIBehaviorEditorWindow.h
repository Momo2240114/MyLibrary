#pragma once

#include "EditorWorkspaceWindow.h"
#include "Editor/Window/AssetBrowserWindow.h"
#include "Engine/GamePlay/AI/AIBehaviorGraph.h"

#include <memory>
#include <unordered_map>

class AIControllerComponent;
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
    bool ShouldPauseScene() const override { return true; }
    bool ShouldRenderSceneWhenPaused() const override { return true; }

private:
    AIControllerComponent* ResolveTarget();
    void DrawToolbar(AIControllerComponent* aiController_);
    void DrawTargetList();
    void DrawAssetBrowserPanel();
    void DrawGraph(AIControllerComponent* aiController_);
    void DrawGraphAssetBar(AIControllerComponent* aiController_);
    void DrawInspector(AIControllerComponent* aiController_);
    void DrawNode(AIBehaviorNode& node_);
    void AddNode(AIControllerComponent* aiController_, AI_BEHAVIOR_NODE_TYPE type_, const char* name_);
    void DrawCreateNodePopup(AIControllerComponent* aiController_);
    void RequestAddNode(AIControllerComponent* aiController_, AI_BEHAVIOR_NODE_TYPE type_, const char* name_);
    void UpdatePriorityFromNodePositions(AIBehaviorGraph& graph_);
    bool GetPinInfo(const AIBehaviorGraph& graph_, uintptr_t pinId_, bool& isInput_, uintptr_t& nodeId_) const;
    const char* GetNodeTypeName(AI_BEHAVIOR_NODE_TYPE type_) const;

private:
    std::weak_ptr<GameObject> targetObject;
    ax::NodeEditor::EditorContext* nodeEditorContext = nullptr;
    AssetBrowserWindow assetBrowser;
    std::unordered_map<uintptr_t, ImVec2> pinPositions;
    uintptr_t selectedNodeId = 0;
    uintptr_t nodeToPlaceId = 0;
    DirectX::XMFLOAT2 nextNodePosition = { 220.0f, 180.0f };
    bool restoreNodePositions = true;
    std::string graphAssetStatus;
    float targetListWidth = 260.0f;
    float inspectorWidth = 340.0f;
    float assetBrowserHeight = 260.0f;
    bool layoutSizeInitialized = false;
};
