#include "AnimationStateMachineComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "AnimationControllerComponent.h"
#include "AnimationState.h"

void AnimationStateMachineComponent::Start() {
    // 同じオブジェクトにあるコントローラーを自動的に掴む
    m_targetController = GetGameObject()->GetComponent<AnimationControllerComponent>().get();
}

void AnimationStateMachineComponent::Update(float deltaTime) 
{
    if (!m_targetController) return;

    // 遷移条件の評価
    EvaluateTransitions();
}

void AnimationStateMachineComponent::EvaluateTransitions() 
{
    if (!m_targetController) return;

    // 1. 遷移のリストを拝借
    const auto& transitions = m_targetController->GetTransitions();
 
   const AnimState* currentState = m_targetController->GetCurrentState();
    if (!currentState) return;

    // 2. 遷移の判定
    for (const auto& transition : transitions) 
    {
        // 現在のステートから出ているリンクか判定
        // ※pinIdからノードを引く処理はコントローラー側に移譲するか、ヘルパーを作る必要がある
        auto* startNode = m_targetController->FindNodeByName(transition.fromStateName);

        if (startNode && startNode->stateName == currentState->name) {

            // 全条件チェック
            bool allMet = true;
            for (const auto& cond : transition.conditions) {
                if (!m_targetController->EvaluateCondition(cond)) {
                    allMet = false;
                    break;
                }
            }

            // 3. 条件を満たせば遷移
            if (allMet) {
                auto* endNode = m_targetController->FindNodeByName(transition.toStateName);
                if (endNode) {
                    RequestStateChange(endNode->stateName, transition.duration);
                    return; // 遷移したら評価を終了
                }
            }
        }
    }
}


void AnimationStateMachineComponent::RequestStateChange(const std::string& stateName, float duration) {
    if (m_targetController) {
        m_targetController->ChangeState(stateName, duration);
    }
}