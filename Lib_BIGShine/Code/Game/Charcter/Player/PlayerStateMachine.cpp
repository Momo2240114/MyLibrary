#include "PlayerStateMachine.h"
#include "PlayerInput.h"
#include "Movement.h"
#include "AnimationController.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"

void PlayerStateMachine::Update(float deltaTime)
{
    auto obj = GetGameObject();
    auto input = obj->GetComponent<PlayerInput>();
    if (!input || m_states.empty()) return;

    // 現在の状態を取得
    auto& currentState = m_states[m_currentStateID];

    // 1. 遷移チェック (ノードエディタで繋いだ線を走査)
    for (const auto& transition : currentState.transitions) {
        // PlayerInputで記録されたフラグを確認
        if (input->HasInput(transition.triggerAction)) {
            TransitionTo(transition.targetStateID);
            input->ClearInput(transition.triggerAction); // 入力を消費
            break;
        }
    }
}

void PlayerStateMachine::TransitionTo(int nextID)
{
    if (m_states.find(nextID) == m_states.end()) return;

    m_currentStateID = nextID;
    auto& newState = m_states[nextID];

    // アニメーションの再生
    if (auto anim = GetGameObject()->GetComponent<AnimationController>()) {
        anim->Play(newState.animationIndex);
    }
}