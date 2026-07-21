#pragma once

#include "Editor/Window/EditorWindow.h"

#include <memory>

class AIBehaviorGraph;
class GameObject;

enum class NODE_GRAPH_MONITOR_TYPE
{
    AUTO,
    AI_BEHAVIOR,
    DRAGON_AI,
    ANIMATION
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
    NODE_GRAPH_MONITOR_TYPE graphType = NODE_GRAPH_MONITOR_TYPE::AUTO;
    bool followSelectedObject = true;
    float zoom = 0.55f;
};
