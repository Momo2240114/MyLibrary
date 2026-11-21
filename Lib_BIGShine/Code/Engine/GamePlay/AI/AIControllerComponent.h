#pragma once

#include "AIBehaviorGraph.h"
#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class BlackboardComponent;
class CharacterMovementComponent;

// 敵AI制御の入口となるComponent。
// Owner・Target・初期位置の関係を毎フレーム計算してBlackboardへ書き込み、
// BehaviorGraphをTickすることで条件判定とAction実行を進める。
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

    std::string& GetGraphAssetPath()
    {
        return graphAssetPath;
    }

    const std::string& GetGraphAssetPath() const
    {
        return graphAssetPath;
    }

    void SetGraphAssetPath(const std::string& path_)
    {
        graphAssetPath = path_;
    }

    AIBehaviorGraph& GetGraph()
    {
        return graph;
    }

    const AIBehaviorGraph& GetGraph() const
    {
        return graph;
    }

    const AIBehaviorContext& GetContext() const
    {
        return context;
    }

    float GetChaseSpeed() const
    {
        return chaseSpeed;
    }

    float GetReturnSpeed() const
    {
        return returnSpeed;
    }

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
    // 毎フレームBehaviorGraphへ渡す、ターゲット・距離・スポーン位置などの判断材料。
    // JSON Assetから復元され、毎フレーム評価されるAI判断グラフ。
    AIBehaviorGraph graph;

    // 現在のTarget、距離、方向、Blackboard参照などをGraphへ渡す実行Context。
    AIBehaviorContext context;

    // Ownerが保持するLocal Blackboardへの非所有参照。
    BlackboardComponent* blackboardComponent = nullptr;

    // 読み書きするBehaviorGraph JSON Assetのパス。
    std::string graphAssetPath = "Data/AI/BehaviorGraph/DefaultAI.json";

    // 優先的にTargetとして検索するGameObject名。
    std::string targetName = "Player";
    std::weak_ptr<GameObject> cachedTarget;
    std::string cachedTargetName;
    std::uint64_t cachedTargetRegistrationSerial = 0;
    bool cachedTargetIsNamed = false;

    // Graph保存・読み込み結果をImGuiへ表示する文字列。
    std::string statusText;

    // 帰還行動の目的地となるAI生成時のWorld座標。
    DirectX::XMFLOAT3 spawnPosition = { 0.0f, 0.0f, 0.0f };

    // Targetを発見済みと判定する最大距離。
    float detectRange = 12.0f;

    // Targetが攻撃可能範囲内にいると判定する距離。
    float attackRange = 2.0f;

    // Targetを見失ったと判定する距離。
    float lostRange = 16.0f;

    // 初期位置への帰還を完了し、移動を停止する距離。
    float returnStopDistance = 0.25f;

    // Target追跡時に使用する水平移動速度。
    float chaseSpeed = 4.0f;

    // 初期位置へ戻る際に使用する水平移動速度。
    float returnSpeed = 3.0f;

    // trueの間だけUpdateでBehaviorGraphをTickする。
    bool isRunning = true;

    // Contextから計算した判断値をBlackboardへ書き込むかを指定する。
    bool writeDebugBlackboard = true;

    // SpawnPositionがJSONから明示的に復元されたかを示す。
    bool hasSerializedSpawnPosition = false;

    // 標準AI Actionを重複なく解除するための登録名一覧。
    std::vector<std::string> registeredDefaultActionNames;
};
