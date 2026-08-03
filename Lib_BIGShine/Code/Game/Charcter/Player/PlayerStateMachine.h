#pragma once

#include <memory>
#include <unordered_map>
#include <utility>

#include "PlayerState.h"

class PlayerControllerComponent;

// Playerの現在行動を1つだけ保持し、Idle/Move/Jump/Attack/Dodge/Hitへ処理を振り分ける。
// 入力解釈はController側、行動中の細かい責務は各PlayerState側に置く。
class PlayerStateMachine
{
public:
    template<class T, class... Args>
    void AddState(PLAYER_ACTION action_, Args&&... args_)
    {
        // PlayerControllerの初期化前はOwnerを解決できないため、State登録を行わない。
        if (!playerController)
        {
            return;
        }

        // 全Stateが同じOwnerとControllerを参照できるよう、登録時に入口情報を渡す。
        std::unique_ptr<T> state_ =
            std::make_unique<T>(
                owner.lock(),
                *playerController,
                std::forward<Args>(args_)...);
        states[action_] = std::move(state_);
    }

    template<class T>
    T* GetCurrentStateAs() const
    {
        // 呼び出し側が特定State固有の情報を確認したい場合だけ型付きで取り出す。
        return dynamic_cast<T*>(currentState);
    }

    // Controller参照とOwner参照を保存し、以降のState生成に使用する。
    void Initialize(PlayerControllerComponent& playerController_);
    // 登録済みActionに対応するStateへ遷移する。
    void ChangeState(PLAYER_ACTION nextAction_);

    // 現在のStateを1フレーム実行する。
    void Update(float dt_);

    const char* GetCurrentStateName() const;
    PLAYER_ACTION GetCurrentAction() const { return currentAction; }

private:
    // Action種別ごとに所有するPlayerState。
    std::unordered_map<PLAYER_ACTION, std::unique_ptr<PlayerState>> states;
    // Stateが操作するPlayerのGameObject。
    std::weak_ptr<GameObject> owner;
    // 入力、Blackboard、Timeline連携を持つControllerへの非所有参照。
    PlayerControllerComponent* playerController = nullptr;
    // 現在実行中のState。
    PlayerState* currentState = nullptr;
    // 現在のAction種別。
    PLAYER_ACTION currentAction = PLAYER_ACTION::NONE;
};
