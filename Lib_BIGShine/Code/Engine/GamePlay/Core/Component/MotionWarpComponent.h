#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/Graphics/Resource/Animation/ModelAnimator.h"

#include <memory>
#include <string>
#include <vector>

class CameraLockOnComponent;

enum class MOTION_WARP_TARGET_MODE
{
    // Targetを使わない。
    NONE,

    // 指定GameObjectをTargetにする。
    GAME_OBJECT,
    WORLD_POSITION,
    LOCK_ON_TARGET,
    ATTACK_ASSIST_TARGET
};

enum class MOTION_WARP_ROTATION_MODE
{
    // 回転補正なし。
    NONE,

    // Target方向へ向ける。
    FACE_TARGET,
    MATCH_TARGET_ROTATION
};

enum class MOTION_WARP_POSITION_MODE
{
    // 位置補正なし。
    NONE,

    // RootMotionの方向だけTarget方向へ寄せる。
    DIRECTION_ONLY,
    SCALE_ONLY,
    DIRECTION_AND_SCALE,
    TARGET_END_POSITION
};

enum class MOTION_WARP_PHASE
{
    IDLE,
    ACTIVE,
    RETURN
};

struct MotionWarpPreset
{
    // Actionから指定するPreset名。
    std::string presetName = "WeakAttackWarp";

    // Targetの取り方と、RootMotionの位置/回転補正方法。
    MOTION_WARP_TARGET_MODE targetMode = MOTION_WARP_TARGET_MODE::ATTACK_ASSIST_TARGET;
    MOTION_WARP_POSITION_MODE positionMode = MOTION_WARP_POSITION_MODE::DIRECTION_AND_SCALE;
    MOTION_WARP_ROTATION_MODE rotationMode = MOTION_WARP_ROTATION_MODE::FACE_TARGET;

    // Animationの正規化時間で、Warpを適用する区間。
    float startNormalizedTime = 0.0f;
    float endNormalizedTime = 0.45f;
    float targetDistance = 1.2f;

    // 位置/回転補正の強さ。
    float positionStrength = 1.0f;
    float rotationStrength = 1.0f;
    float minimumPositionScale = 0.75f;
    float maximumPositionScale = 1.25f;
    float maximumPositionCorrectionPerFrame = 0.25f;
    float maximumTotalPositionCorrection = 1.5f;

    // 1回のWarpで許可する最大回転量。
    float maximumRotationDegrees = 45.0f;
    float maximumRotationDegreesPerSecond = 360.0f;
    float minimumRemainingRootMotionDistance = 0.01f;
    float maximumWarpTargetDistance = 12.0f;
    float minimumTargetDistance = 0.15f;

    // 位置/回転それぞれのWarp有効切り替え。
    bool warpPosition = true;
    bool warpRotation = true;
    bool horizontalOnly = true;
    bool lockTargetOnStart = true;
    bool stopWhenTargetInvalid = true;
    bool continueAcrossCombo = false;
    bool useUnscaledTime = false;
};

struct MotionWarpRequest
{
    // 現在動いているWarp要求の名前とPreset名。
    std::string requestName;
    std::string presetName;
    std::weak_ptr<GameObject> targetObject;

    // Lock開始時に確定したTarget位置・向き。
    DirectX::XMFLOAT3 targetPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 targetForward{ 0.0f, 0.0f, 1.0f };
    float normalizedTime = 0.0f;

    // Preset上限を超えないよう、累積補正量を記録する。
    float accumulatedPositionCorrection = 0.0f;
    float accumulatedRotationCorrectionDegrees = 0.0f;
    bool active = false;
};

struct MotionWarpResult
{
    // Warp後にAnimationControllerへ返すRootMotion差分。
    ModelAnimator::RootMotionDelta rootMotionDelta;

    // Debug表示用に、今回のフレームで補正が入ったかを記録する。
    bool positionWarped = false;
    bool rotationWarped = false;
};

class MotionWarpComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "MotionWarpComponent";
        return name_;
    }

    ~MotionWarpComponent() override;
    void Start() override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void Play(const std::string& presetName_ = "");
    void Stop(const std::string& requestName_ = "");
    void Clear();

    // ModelAnimatorが出したRootMotionDeltaを、Targetへ近づくよう補正して返す。
    MotionWarpResult ApplyRootMotionWarp(
        const ModelAnimator::RootMotionDelta& rootMotionDelta_,
        float normalizedTime_,
        float deltaTime_);
    bool IsWarpActive() const { return request.active; }

private:
    // Presetが空の時に、攻撃用の標準Warp設定を補充する。
    void EnsureDefaultPresets();
    MotionWarpPreset* FindPreset(const std::string& presetName_);
    const MotionWarpPreset* FindPreset(const std::string& presetName_) const;
    std::shared_ptr<GameObject> ResolveTarget(const MotionWarpPreset& preset_);

    // 現在カメラからLockOnTargetを取るための参照を探す。
    std::shared_ptr<CameraLockOnComponent> ResolveCameraLockOnComponent();

    // PresetのTargetModeに従って、Warp開始時点のTargetをRequestへ固定する。
    bool ResolveRequestTarget(MotionWarpRequest& request_, const MotionWarpPreset& preset_);
    void RegisterActions();
    void UnregisterActions();

    std::vector<MotionWarpPreset> presets;

    // 現在再生中のWarp要求。1Componentにつき同時に1つを扱う。
    MotionWarpRequest request;
    std::string defaultPresetName = "WeakAttackWarp";
    std::string debugInactiveReason;

    // GUIで補正前後の差分を確認するためのDebug値。
    DirectX::XMFLOAT3 debugOriginalDelta{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 debugWarpedDelta{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 debugIdealStopPosition{ 0.0f, 0.0f, 0.0f };
    float debugAppliedPositionScale = 1.0f;
    float debugAppliedYawDegrees = 0.0f;
    bool actionsRegistered = false;
};
