#include "PrecompiledHeader.h"

#include "PlayerStateMachine.h"
#include "PlayerControllerComponent.h"
#include "Engine/Common/DebugLog.h"

void PlayerStateMachine::Initialize(
    PlayerControllerComponent& playerController_)
{
    // 各StateがControllerと同じGameObjectを参照できるよう、初期化時に入口情報だけ保持する。
    playerController = &playerController_;
    owner = playerController_.GetGameObject();
}

void PlayerStateMachine::ChangeState(PLAYER_ACTION nextAction_)
{
    auto it_ = states.find(nextAction_);
    if (it_ == states.end())
    {
        return;
    }

    // 状態遷移はExit、参照差し替え、Enterの順に固定して、Stateごとの初期化漏れを防ぐ。
    const char* previousStateName_ = currentState ? currentState->GetName() : "None";
    const char* nextStateName_ = it_->second ? it_->second->GetName() : "None";
    const std::shared_ptr<GameObject> owner_ = this->owner.lock();
    const std::string ownerName_ = owner_ ? owner_->GetName() : "None";
    BS_LOG_INFO(
        LOG_CATEGORY::GAME,
        "Player state change: " + ownerName_ + " " + previousStateName_ + " -> " + nextStateName_);

    if (currentState)
    {
        currentState->Exit();
    }

    currentState = it_->second.get();
    currentAction = nextAction_;
    currentState->Enter();
}

void PlayerStateMachine::Update(float dt_)
{
    if (currentState)
    {
        currentState->Execute(dt_);
    }
}

const char* PlayerStateMachine::GetCurrentStateName() const
{
    return currentState ? currentState->GetName() : "None";
}
