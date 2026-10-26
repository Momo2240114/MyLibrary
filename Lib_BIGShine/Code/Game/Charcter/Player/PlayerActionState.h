#pragma once

#include "PlayerState.h"

class PlayerControllerComponent;

// 指定Actionの開始と共通入力を扱い、通常ガード中は移動と注視を更新するState。
class PlayerActionState : public PlayerState
{
public:
    PlayerActionState(
        const std::shared_ptr<GameObject>& owner_,
        PlayerControllerComponent& playerController_,
        PLAYER_ACTION action_);

    const char* GetName() const override { return GetPlayerActionName(action); }
    // 担当するActionの受付状態を初期化し、防御・反撃では水平移動を止める。
    void Enter() override;
    // ガード中の移動と注視を更新し、共通の入力処理へ次の行動判断を渡す。
    void Execute(float elapsedTime_) override;

private:
    // このStateが開始するAction種別。
    PLAYER_ACTION action = PLAYER_ACTION::NONE;
};
