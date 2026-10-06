#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include "Engine/GamePlay/Combat/SkillExecution.h"

// 行動の防御受付を、既存Colliderの操作へ対応付ける。
enum class HumanBossDefense { NONE, DODGE, GUARD, JUST_GUARD };

// 防御受付と移動を既存Componentへ渡す、人型ボスの行動実行担当。
class HumanBossActionComponent : public Component, public IActionProvider
{
public:
    const std::string& GetName() const override;
    // Animation EventとCombat Systemから受け取るActionを登録する。
    void Start() override;
    // 終了と成功の通知をweak参照でOwnerへ接続する。
    void RegisterActions() override;
    // 共通Skillの終了理由と素材不足を表示する。
    void Gui() override;
    // AIから一度だけ進め、共通SkillExecutionと更新を重複させない。
    void Tick(float deltaTime_);
    // 実行に必要なAnimation、Sequence、防御Colliderの存在を確認する。
    bool CanStart(const SkillDefinition& skill_, HumanBossDefense defense_) const;
    // 共通Skillを開始し、防御行動の方向を固定する。
    bool Begin(const SkillDefinition& skill_, HumanBossDefense defense_, const DirectX::XMFLOAT3& velocity_);
    // 共通Skillと防御窓を中断し、攻撃イベントを終了する。
    void Stop(const std::string& idleState_ = "SS_Idle");
    bool IsActive() const { return execution.IsActive(); }
    // Combat Systemが確定した成功を一度だけAIへ返す。
    bool ConsumeJustGuardSuccess();
    SkillExecutionResult GetResult() const { return execution.GetResult(); }
    // 移動アニメーションと注視を引き受け、AIの判断から再生処理を分離する。
    void SetLocomotion(const std::string& state_, const DirectX::XMFLOAT3& velocity_,
        const DirectX::XMFLOAT3& facing_, const DirectX::XMFLOAT2& blendInput_);
    // 注視方向を既存Movementへ予約する。
    void FaceTarget(const DirectX::XMFLOAT3& direction_);
private:
    // 行動の中断・終了時に、ボス自身の防御窓だけを閉じる。
    void CloseDefense();
    SkillExecution execution;
    DirectX::XMFLOAT3 actionVelocity{ 0.0f, 0.0f, 0.0f };
    bool actionOwnsVelocity = false;
    bool justGuardSucceeded = false;
    HumanBossDefense currentDefense = HumanBossDefense::NONE;
    std::string status;
};
