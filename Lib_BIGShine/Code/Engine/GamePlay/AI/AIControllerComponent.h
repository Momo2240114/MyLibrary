#pragma once

#include "AIBehaviorGraph.h"
#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <string>
#include <vector>

class BlackboardComponent;
class CharacterMovementComponent;

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
    const AIBehaviorContext& GetContext() const { return context; }
    float GetChaseSpeed() const { return chaseSpeed; }
    float GetReturnSpeed() const { return returnSpeed; }

private:
    void EnsureBlackboard();
    void UpdateAIContext(float deltaTime_);
    void ResolveTarget();
    void WriteContextBlackboard();
    void RegisterDefaultAIActions();
    void UnregisterDefaultAIActions();
    void ActionChaseTarget();
    void ActionReturnToSpawn();
    void ActionStopMove();
    void ActionAttack();
    void ActionNotifyActionEnd();
    void RequestMove(const DirectX::XMFLOAT3& direction_, float speed_);

private:
    AIBehaviorGraph graph;
    AIBehaviorContext context;
    BlackboardComponent* blackboardComponent = nullptr;
    std::string graphAssetPath = "Data/AI/BehaviorGraph/DefaultAI.json";
    std::string targetName = "Player";
    std::string statusText;
    DirectX::XMFLOAT3 spawnPosition = { 0.0f, 0.0f, 0.0f };
    float detectRange = 12.0f;
    float attackRange = 2.0f;
    float lostRange = 16.0f;
    float returnStopDistance = 0.25f;
    float chaseSpeed = 4.0f;
    float returnSpeed = 3.0f;
    bool isRunning = true;
    bool writeDebugBlackboard = true;
    bool hasSerializedSpawnPosition = false;
    std::vector<std::string> registeredDefaultActionNames;
};
