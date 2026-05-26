#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "AnimationState.h"

#include <unordered_map>
#include <variant>
#include <memory>

class AnimationControllerComponent;


// ステートマシンが判断を下すためのインターフェース
class AnimationStateMachineComponent : public Component
{
public:
    const std::string& GetName() const override {
        static std::string name = "AnimationStateMachineComponent";
        return name;
    }

    void Start() override;
    void Update(float deltaTime) override;

    // AIが「特定のステートへ遷移せよ」と命令するための窓口
    void RequestStateChange(const std::string& stateName, float duration = 0.2f);

private:
    AnimationControllerComponent* m_targetController = nullptr;

    // 判定ロジック
    void EvaluateTransitions();
};