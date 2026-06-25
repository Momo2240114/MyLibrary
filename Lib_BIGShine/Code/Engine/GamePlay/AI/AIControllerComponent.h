#pragma once

#include "AIBehaviorGraph.h"
#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <string>

namespace ax { namespace NodeEditor { struct EditorContext; } }

class BlackboardComponent;

class AIControllerComponent : public Component
{
public:
    AIControllerComponent() = default;
    ~AIControllerComponent() override;

    const std::string& GetName() const override
    {
        static std::string name_ = "AIControllerComponent";
        return name_;
    }

    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void Play();
    void Stop();
    void ResetTree();
    bool SaveGraphAsset(const std::string& path_) const;
    bool LoadGraphAsset(const std::string& path_);

    AIBehaviorGraph& GetGraph() { return graph; }
    const AIBehaviorGraph& GetGraph() const { return graph; }

private:
    void EnsureBlackboard();
    void EnsureNodeEditorContext();
    void DrawNodeEditor();
    void DrawInspector();
    void DrawNode(AIBehaviorNode& node_);
    void AddNodeFromMenu(AI_BEHAVIOR_NODE_TYPE type_, const char* name_);
    const char* GetNodeTypeName(AI_BEHAVIOR_NODE_TYPE type_) const;
    bool GetPinInfo(uintptr_t pinId_, bool& isInput_, uintptr_t& nodeId_) const;

private:
    AIBehaviorGraph graph;
    BlackboardComponent* blackboardComponent = nullptr;
    ax::NodeEditor::EditorContext* nodeEditorContext = nullptr;
    std::string graphAssetPath = "Data/AI/BehaviorGraph/DefaultAI.json";
    std::string statusText;
    bool isRunning = true;
    bool showNodeEditor = true;
    bool restoreNodePositions = true;
    uintptr_t selectedNodeId = 0;
    uintptr_t nodeToPlaceId = 0;
    DirectX::XMFLOAT2 nextNodePosition = { 200.0f, 200.0f };
};
