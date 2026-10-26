#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <string>
#include <vector>

class GltfModelCommponennt;
class IKMapComponent;

enum class DRAGON_TAIL_IK_MODE
{
    // Dragon Tail IKの動作Mode。
    DISABLED,
    FOLLOW_BODY,
    AIM_TO_TARGET,
    SWEEP_LEFT,
    SWEEP_RIGHT,
    SPIN_SWEEP
};

struct DragonTailIKCommand
{
    // 現在CommandのTail制御Mode。
    DRAGON_TAIL_IK_MODE mode = DRAGON_TAIL_IK_MODE::FOLLOW_BODY;

    // AIM_TO_TARGET時に追うObject名。
    std::string targetName;

    // Commandの再生時間とBlend時間。
    float duration = 1.0f;
    float blendInTime = 0.1f;
    float blendOutTime = 0.2f;

    // Sweep系Commandの角度設定。
    float sweepAngleDegrees = 70.0f;
    float liftAngleDegrees = 8.0f;
    float expandAngleDegrees = 15.0f;

    // Chain根元から先端へ掛けるWeight範囲。
    float rootWeight = 0.05f;
    float tipWeight = 1.0f;

    // 攻撃判定を有効にするCommand内の時間割合。
    float attackStartRate = 0.25f;
    float attackEndRate = 0.75f;
};

class DragonTailCommandIKComponent : public Component
{
public:
    const std::string& GetName() const override;
    void Start() override;
    bool IsLoadReady() const override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    // ActionRegistryへDragonIK系Actionを登録する。
    void RegisterActions();

    // 通常のBody追従Modeへ戻す。
    void SetFollowBody();
    // Commandを解除してIK制御を止める。
    void ClearCommand();

    // 指定TargetへTail先端を向けるCommandを開始する。
    void AimToTarget(
        const std::string& targetName_,
        float duration_,
        float blendInTime_ = 0.1f,
        float blendOutTime_ = 0.2f);

    // 左/右/回転Sweep Commandを開始する。
    void StartSweepLeft(float duration_);
    void StartSweepRight(float duration_);
    void StartSpinSweep(float duration_);

    bool IsAttackActive() const { return attackActive; }

    // Tail先端のWorld位置/方向を取得する。HitBoxやVFX発生位置に使う。
    bool GetTailTipWorldPosition(DirectX::XMFLOAT3& position_) const;
    // 尻尾全体を覆う攻撃判定の始点として、チェーン根元のWorld位置を取得する。
    bool GetTailRootWorldPosition(DirectX::XMFLOAT3& position_) const;
    // 曲がった尻尾も節ごとに覆えるよう、チェーン全ノードのWorld位置を順番に取得する。
    bool GetTailWorldPositions(std::vector<DirectX::XMFLOAT3>& positions_) const;
    bool GetTailTipWorldForward(DirectX::XMFLOAT3& forward_) const;

private:
    // 操作対象ModelとIK Map。
    GltfModelCommponennt* model = nullptr;
    IKMapComponent* ikMap = nullptr;

    // Tailに使うIK Chain名と、Modelローカル上の前方向。
    std::string chainName = "DragonTail";
    DirectX::XMFLOAT3 localForwardAxis = { 0.0f, 0.0f, 1.0f };

    // Action登録カテゴリと登録済みAction名の管理。
    std::string actionCategory = "DragonIK";
    std::string registeredActionCategory;
    std::string registeredGlobalActionCategory;
    std::vector<std::string> registeredActionNames;
    std::vector<std::string> registeredGlobalActionNames;
    bool registerGlobalActions = false;

    // Editorから再生する標準Action時間。
    float actionDuration = 0.8f;
    float spinActionDuration = 1.6f;

    // 現在Commandと補間中の角度状態。
    DragonTailIKCommand currentCommand;
    float commandTimer = 0.0f;
    float currentYawDegrees = 0.0f;
    float currentPitchDegrees = 0.0f;
    float currentRollDegrees = 0.0f;
    float targetYawDegrees = 0.0f;
    float targetPitchDegrees = 0.0f;
    float targetRollDegrees = 0.0f;
    float followSpeed = 6.0f;
    float spinTipLagRate = 0.18f;
    float spinTipLagStartRate = 0.55f;
    bool attackActive = false;

    // 登録済みActionを解除する。
    void UnregisterActions();
    // BlendIn/BlendOutを考慮した現在Weightを返す。
    float CalculateBlendRate() const;
    // Command全体の0-1進行率を返す。
    float CalculateCommandRate() const;
    // 回転尻尾攻撃中、先端側だけ遅れた進行率を返す。
    float CalculateSpinLagRate(float commandRate_, float normalizedPosition_) const;
    // 現在CommandからTail ChainへIK回転を適用する。
    bool ApplyTailIK(float deltaTime_);
};
