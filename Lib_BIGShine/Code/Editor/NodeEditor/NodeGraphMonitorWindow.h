#pragma once

#include "Editor/Window/EditorWindow.h"

#include <memory>
#include <string>

class AIBehaviorGraph;
class GameObject;

enum class NODE_GRAPH_MONITOR_TYPE
{
    // AUTOは対象が持つコンポーネントから優先順位付きで表示グラフを決める。
    AUTO,
    AI_BEHAVIOR,
    DRAGON_AI,
    ANIMATION
};

enum class NODE_GRAPH_MONITOR_FOCUS_MODE
{
    // 表示位置をグラフ左上、スコープ中心、実行中ノードのどれへ合わせるか。
    GRAPH_ORIGIN,
    SCOPE_CENTER,
    ACTIVE_NODE
};

class NodeGraphMonitorWindow : public EditorWindow
{
public:
    explicit NodeGraphMonitorWindow(GameObjectManager* manager_);

    void Draw() override;

private:
    std::shared_ptr<GameObject> ResolveTargetObject();
    void DrawTargetControls(std::shared_ptr<GameObject>& targetObject_);
    void DrawRuntimeSummary(const std::shared_ptr<GameObject>& targetObject_);
    void DrawGraphView(const std::shared_ptr<GameObject>& targetObject_);
    void DrawAIBehaviorGraph(AIBehaviorGraph& graph_);
    void DrawAnimationGraph(const std::shared_ptr<GameObject>& targetObject_);
    NODE_GRAPH_MONITOR_TYPE ResolveGraphType(const std::shared_ptr<GameObject>& targetObject_) const;

private:
    std::weak_ptr<GameObject> targetObject;
    // 表示対象グラフと追従設定。実行中のAI/Animation確認に使う。
    NODE_GRAPH_MONITOR_TYPE graphType = NODE_GRAPH_MONITOR_TYPE::AUTO;
    bool followSelectedObject = true;
    bool followRuntimeScope = true;
    float zoom = 0.55f;
    NODE_GRAPH_MONITOR_FOCUS_MODE focusMode = NODE_GRAPH_MONITOR_FOCUS_MODE::ACTIVE_NODE;
    // AIとAnimationはそれぞれ別スコープを持つため、最後に見ていたスコープを個別に保持する。
    std::string aiScopeId = "Root";
    std::string animationScopeId = "Root";
};
