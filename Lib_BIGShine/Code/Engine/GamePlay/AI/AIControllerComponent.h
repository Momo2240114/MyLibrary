#pragma once

#include "AIBehaviorGraph.h"
#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <string>

class BlackboardComponent;

class AIControllerComponent : public Component
{
public:
    AIControllerComponent() = default;

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
    std::string& GetGraphAssetPath() { return graphAssetPath; }
    const std::string& GetGraphAssetPath() const { return graphAssetPath; }
    void SetGraphAssetPath(const std::string& path_) { graphAssetPath = path_; }

    AIBehaviorGraph& GetGraph() { return graph; }
    const AIBehaviorGraph& GetGraph() const { return graph; }

private:
    void EnsureBlackboard();

private:
    AIBehaviorGraph graph;
    BlackboardComponent* blackboardComponent = nullptr;
    std::string graphAssetPath = "Data/AI/BehaviorGraph/DefaultAI.json";
    std::string statusText;
    bool isRunning = true;
};
