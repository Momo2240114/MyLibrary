#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <string>

#include "PlayerState.h"

class PlayerControllerComponent;

// 地上回避と空中回避を扱い、ジャスト回避判定とカウンター受付を管理するState。
class PlayerDodgeState : public PlayerState
{
public:
    PlayerDodgeState(
        const std::shared_ptr<GameObject>& owner_,
        PlayerControllerComponent& playerController_,
        PLAYER_ACTION action_ = PLAYER_ACTION::DODGE);

    const char* GetName() const override
    {
        return GetPlayerActionName(action);
    }

    // 回避方向とBlend入力を確定し、空中回避の場合は落下を一時停止する。
    void Enter() override;
    // ジャスト回避の受付時間を進め、回避中の入力と空中姿勢を更新する。
    void Execute(float elapsedTime_) override;
    // 中断された場合も受付判定を閉じ、変更した重力と移動制御を復元する。
    void Exit() override;

    // Colliderからの成功通知を、回避状態と受付窓を確認して一度だけ受け付ける。
    void NotifyJustDodgeSucceeded();

private:
    // Timeline/ActionSequenceから呼ばれるジャスト回避関連Actionを登録する。
    void RegisterJustDodgeActions();
    // 回避方向入力をPlayer/Anim両方のBlackboardへ送る。
    void SendDodgeInputToBlackboard(const DirectX::XMFLOAT3& input_);

    // 無敵化とジャスト回避Collider有効化をOwner Action経由で実行する。
    void ActivateDodgeCollision();
    // ジャスト回避Colliderを閉じ、HurtBoxの無敵を戻す。
    void DeactivateDodgeCollision();
    // 空中回避中に落下しないよう重力と垂直移動を一時停止する。
    void SuspendAirDodgeVerticalMovement();
    // 空中回避で一時変更した重力と垂直移動制御を元に戻す。
    void RestoreAirDodgeVerticalMovement();

    // ジャスト回避受付フレームを開始する。
    void StartJustDodgeFrame();
    // ジャスト回避受付フレームを終了する。
    void EndJustDodgeFrame();

    // ジャスト回避成功時のBlackboard更新とReaction実行をまとめる。
    void HandleJustDodgeSuccess();
    // 回避開始時に前回のジャスト回避結果を消す。
    void ClearJustDodgeBlackboard();
    // カウンター受付状態をBlackboardへ反映する。
    void SetCounterAvailable(bool enabled_);
    // Ownerに登録されたAction名を実行する。
    void ExecuteOwnerAction(const std::string& actionName_);

private:
    // 地上回避または空中回避のAction種別。
    PLAYER_ACTION action = PLAYER_ACTION::DODGE;
    // 実際に移動や向きへ使うWorld空間の回避方向。
    DirectX::XMFLOAT3 dodgeDirection{ 0.0f, 0.0f, 1.0f };
    // AnimationBlendへ渡す入力方向。
    DirectX::XMFLOAT3 dodgeBlendInput{ 0.0f, 0.0f, 1.0f };

    bool justDodgeActionsRegistered = false;

    bool dodgeActive = false;
    bool justDodgeFrameActive = false;
    bool justDodgeSucceeded = false;
    bool counterAvailable = false;
    // 空中回避の最初の停止時にだけ元の重力設定を保存し、終了時の復元に使う。
    bool airDodgeGravityOverridden = false;
    bool previousAirDodgeUseGravity = false;

    // ジャスト回避受付をAction側で閉じ忘れた場合の保険時間。
    float justDodgeFrameDuration = 0.18f;
    // 現在のジャスト回避受付残り時間。
    float justDodgeFrameTimer = 0.0f;

    std::string justDodgeColliderActivateActionName =
        "JustDodge_EnableJustDodgeCollider";
    std::string justDodgeColliderDeactivateActionName =
        "JustDodge_DisableJustDodgeCollider";

    std::string frameStartActionName = "JustDodge_FrameStart";
    std::string frameEndActionName = "JustDodge_FrameEnd";
    std::string consumeCounterActionName = "JustDodge_ConsumeCounter";
};
