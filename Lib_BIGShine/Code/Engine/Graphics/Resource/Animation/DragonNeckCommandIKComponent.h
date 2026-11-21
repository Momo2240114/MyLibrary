#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <string>
#include <vector>

class GltfModelCommponennt;
class IKMapComponent;

enum class DRAGON_NECK_IK_MODE
{
    // Dragon Neck IKの動作Mode。
    DISABLED,
    LOOK_AT_TARGET,
    LOOK_AT_WORLD_POSITION,
    FIXED_YAW_OFFSET,
    SWEEP_YAW
};

struct DragonNeckIKCommand
{
    // 現在CommandのNeck制御Mode。
    DRAGON_NECK_IK_MODE mode = DRAGON_NECK_IK_MODE::DISABLED;

    // Target ObjectまたはWorld座標を注視対象として使う。
    std::string targetName;
    DirectX::XMFLOAT3 worldTargetPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 targetOffset = { 0.0f, 0.0f, 0.0f };

    // 固定Yaw/Sweep時に使う角度設定。
    float startYawDegrees = 0.0f;
    float endYawDegrees = 0.0f;
    float yawOffsetDegrees = 0.0f;
    float pitchOffsetDegrees = 0.0f;

    // 首が曲がりすぎないようにするClamp角度。
    float maxYawDegrees = 55.0f;
    float maxPitchDegrees = 30.0f;

    // Command時間とBlend時間。
    float duration = 1.0f;
    float blendInTime = 0.15f;
    float blendOutTime = 0.2f;

    // Chain根元から先端へ掛けるWeight範囲。
    float rootWeight = 0.2f;
    float tipWeight = 1.0f;

    // Command終了後も最後の方向を維持するかどうか。
    bool keepLastDirection = false;
};

class DragonNeckCommandIKComponent : public Component
{
public:
    const std::string& GetName() const override;
    void Start() override;
    bool IsLoadReady() const override;
    void Update(float deltaTime_) override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    // ActionRegistryへDragonIK系Actionを登録する。
    void RegisterActions();

    // 現在Commandを解除する。
    void ClearCommand();

    // 指定Objectを注視するCommandを開始する。
    void LookAtTarget(
        const std::string& targetName_,
        float duration_,
        float blendInTime_ = 0.15f,
        float blendOutTime_ = 0.2f);

    // 指定World座標を注視するCommandを開始する。
    void LookAtWorldPosition(
        const DirectX::XMFLOAT3& worldPosition_,
        float duration_,
        float blendInTime_ = 0.15f,
        float blendOutTime_ = 0.2f);

    // 現在向きにYaw Offsetを加えるCommandを開始する。
    void LookAtYawOffset(
        float yawDegrees_,
        float duration_,
        float blendInTime_ = 0.15f,
        float blendOutTime_ = 0.2f);

    // Yawを左右にSweepするCommandを開始する。
    void SweepYaw(
        float startYawDegrees_,
        float endYawDegrees_,
        float duration_,
        float blendInTime_ = 0.15f,
        float blendOutTime_ = 0.2f);

    // 常時Target Objectを見るModeを切り替える。
    void SetTargetObjectLookMode(bool enabled_);

    void SetTargetObjectName(const std::string& targetName_)
    {
        lookTargetObjectName = targetName_;
    }

    bool IsCommandActive() const
    {
        return currentCommand.mode != DRAGON_NECK_IK_MODE::DISABLED;
    }

    // 口元のWorld位置/方向を取得する。BreathやVFXの基準に使う。
    bool GetMouthWorldPosition(DirectX::XMFLOAT3& position_) const;
    bool GetMouthWorldForward(DirectX::XMFLOAT3& forward_) const;

private:
    // 操作対象ModelとIK Map。
    GltfModelCommponennt* model = nullptr;
    IKMapComponent* ikMap = nullptr;

    // Neck Chain名と口元Node名。
    std::string chainName = "DragonNeck";
    std::string mouthNodeName = "Mouth";
    int mouthNodeIndex = -1;

    // Modelローカル上の前方向と、Action用の固定World Target。
    DirectX::XMFLOAT3 localForwardAxis = { 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 actionWorldTargetPosition = { 0.0f, 2.0f, 6.0f };

    // Action登録カテゴリと登録済みAction名の管理。
    std::string actionCategory = "DragonIK";
    std::string registeredActionCategory;
    std::string registeredGlobalActionCategory;
    std::vector<std::string> registeredActionNames;
    std::vector<std::string> registeredGlobalActionNames;
    bool registerGlobalActions = false;

    // Editorから再生するAction標準値。
    float actionDuration = 1.0f;
    float actionBlendInTime = 0.15f;
    float actionBlendOutTime = 0.2f;
    float actionYawDegrees = 30.0f;
    float actionSweepStartYawDegrees = -30.0f;
    float actionSweepEndYawDegrees = 30.0f;

    // 常時Target Objectを見るModeの設定。
    bool lookTargetObjectMode = false;
    std::string lookTargetObjectName = "PlayerModel";
    DirectX::XMFLOAT3 lookTargetObjectOffset = { 0.0f, 1.2f, 0.0f };
    float lookTargetBlendInTime = 0.15f;
    float lookTargetYawOffsetDegrees = 0.0f;
    float lookTargetPitchOffsetDegrees = 0.0f;
    float lookTargetMaxYawDegrees = 65.0f;
    float lookTargetMaxPitchDegrees = 35.0f;

    // 現在Commandと補間中のYaw/Pitch状態。
    DragonNeckIKCommand currentCommand;
    float commandTimer = 0.0f;
    float currentYawDegrees = 0.0f;
    float currentPitchDegrees = 0.0f;
    float targetYawDegrees = 0.0f;
    float targetPitchDegrees = 0.0f;
    float followSpeed = 6.0f;

    // 登録済みActionを解除する。
    void UnregisterActions();
    // 常時Target Objectを見るModeからCommandを生成/更新する。
    void ApplyTargetObjectLookMode();
    // BlendIn/BlendOutを考慮した現在Weightを返す。
    float CalculateBlendRate() const;
    // Commandに応じた注視Target座標を解決する。
    bool ResolveTargetPosition(DirectX::XMFLOAT3& targetPosition_);
    // 現在CommandからNeck ChainへIK回転を適用する。
    bool ApplyNeckIK(float deltaTime_);
};
