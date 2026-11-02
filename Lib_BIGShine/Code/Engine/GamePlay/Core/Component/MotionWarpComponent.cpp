#include "PrecompiledHeader.h"

#include "MotionWarpComponent.h"

#include "Engine/GamePlay/Camera/CameraLockOnComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/Math/Curve/CurveComponent.h"
#include "Game/Charcter/Player/PlayerControllerComponent.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <imgui.h>
#include <imgui_stdlib.h>

namespace
{
    // モーションワープは水平面上の補正だけを扱うため、XZ成分の有効判定に使う。
    constexpr float MINIMUM_HORIZONTAL_LENGTH_SQUARED = 0.000001f;
    constexpr float DEGREES_TO_RADIANS = DirectX::XM_PI / 180.0f;
    constexpr float RADIANS_TO_DEGREES = 180.0f / DirectX::XM_PI;

    bool NormalizeHorizontal(DirectX::XMFLOAT3& direction_)
    {
        // 高低差で攻撃方向がぶれないよう、Y成分を落として水平ベクトルとして正規化する。
        direction_.y = 0.0f;
        const float lengthSquared_ =
            direction_.x * direction_.x + direction_.z * direction_.z;
        if (lengthSquared_ <= MINIMUM_HORIZONTAL_LENGTH_SQUARED) return false;

        const float inverseLength_ = 1.0f / std::sqrt(lengthSquared_);
        direction_.x *= inverseLength_;
        direction_.z *= inverseLength_;
        return true;
    }

    float GetHorizontalLength(const DirectX::XMFLOAT3& vector_)
    {
        return std::sqrt(vector_.x * vector_.x + vector_.z * vector_.z);
    }

    // 水平移動の経路が停止距離へ入り込む場合だけ、RootMotionの接近を制限する。
    bool LimitRootMotionToStopDistance(
        DirectX::SimpleMath::Vector3& delta_,
        const DirectX::XMFLOAT3& ownerPosition_,
        const DirectX::XMFLOAT3& targetPosition_,
        float stopDistance_)
    {
        const double offsetX_ = static_cast<double>(ownerPosition_.x) - targetPosition_.x;
        const double offsetZ_ = static_cast<double>(ownerPosition_.z) - targetPosition_.z;
        const double distanceSquared_ = offsetX_ * offsetX_ + offsetZ_ * offsetZ_;
        const double movementSquared_ =
            static_cast<double>(delta_.x) * delta_.x +
            static_cast<double>(delta_.z) * delta_.z;
        const double approachDot_ = offsetX_ * delta_.x + offsetZ_ * delta_.z;
        if (movementSquared_ <= 0.0 || approachDot_ >= 0.0) return false;

        const double radius_ = (std::max)(stopDistance_, 0.0f);
        const double boundaryDifference_ = distanceSquared_ - radius_ * radius_;
        if (boundaryDifference_ <= 0.0)
        {
            // 境界上や内側では内向き成分だけを除き、横移動と離脱方向の移動を残す。
            const double inwardScale_ = approachDot_ / distanceSquared_;
            delta_.x -= static_cast<float>(offsetX_ * inwardScale_);
            delta_.z -= static_cast<float>(offsetZ_ * inwardScale_);
            return true;
        }

        // 終点が外側でも途中で対象を突き抜けるため、線分と停止円の最初の交点を求める。
        const double discriminant_ =
            approachDot_ * approachDot_ - movementSquared_ * boundaryDifference_;
        if (discriminant_ < 0.0 || (discriminant_ == 0.0 && radius_ > 0.0)) return false;

        // 境界付近の桁落ちを避ける形で、移動を許可する割合を計算する。
        const double movementFraction_ =
            boundaryDifference_ / (-approachDot_ + std::sqrt(discriminant_));
        if (movementFraction_ >= 1.0) return false;

        delta_.x = static_cast<float>(delta_.x * movementFraction_);
        delta_.z = static_cast<float>(delta_.z * movementFraction_);
        return true;
    }

    float GetYawDegrees(const DirectX::XMFLOAT3& direction_)
    {
        return std::atan2(direction_.x, direction_.z) * RADIANS_TO_DEGREES;
    }

    float NormalizeAngleDegrees(float angle_)
    {
        // Yaw差分を最短回転として扱えるよう、-180〜180度に丸める。
        while (angle_ > 180.0f) angle_ -= 360.0f;
        while (angle_ < -180.0f) angle_ += 360.0f;
        return angle_;
    }

    DirectX::XMFLOAT3 DirectionFromYawDegrees(float yawDegrees_)
    {
        const float radians_ = yawDegrees_ * DEGREES_TO_RADIANS;
        return { std::sin(radians_), 0.0f, std::cos(radians_) };
    }

    float CalculateWarpWeight(
        float normalizedTime_,
        const MotionWarpPreset& preset_)
    {
        // 区間外の補正を止め、追加補正カーブは要求の進行率で別途評価する。
        const float start_ = std::clamp(preset_.startNormalizedTime, 0.0f, 1.0f);
        const float end_ = std::clamp(preset_.endNormalizedTime, start_, 1.0f);
        if (normalizedTime_ < start_ || normalizedTime_ > end_) return 0.0f;
        return 1.0f;
    }

    const char* ToString(MOTION_WARP_TARGET_MODE value_)
    {
        switch (value_)
        {
        case MOTION_WARP_TARGET_MODE::NONE: return "NONE";
        case MOTION_WARP_TARGET_MODE::GAME_OBJECT: return "GAME_OBJECT";
        case MOTION_WARP_TARGET_MODE::WORLD_POSITION: return "WORLD_POSITION";
        case MOTION_WARP_TARGET_MODE::LOCK_ON_TARGET: return "LOCK_ON_TARGET";
        case MOTION_WARP_TARGET_MODE::ATTACK_ASSIST_TARGET: return "ATTACK_ASSIST_TARGET";
        default: return "UNKNOWN";
        }
    }

    const char* ToString(MOTION_WARP_POSITION_MODE value_)
    {
        switch (value_)
        {
        case MOTION_WARP_POSITION_MODE::NONE: return "NONE";
        case MOTION_WARP_POSITION_MODE::DIRECTION_ONLY: return "DIRECTION_ONLY";
        case MOTION_WARP_POSITION_MODE::SCALE_ONLY: return "SCALE_ONLY";
        case MOTION_WARP_POSITION_MODE::DIRECTION_AND_SCALE: return "DIRECTION_AND_SCALE";
        case MOTION_WARP_POSITION_MODE::TARGET_END_POSITION: return "TARGET_END_POSITION";
        default: return "UNKNOWN";
        }
    }

    const char* ToString(MOTION_WARP_ROTATION_MODE value_)
    {
        switch (value_)
        {
        case MOTION_WARP_ROTATION_MODE::NONE: return "NONE";
        case MOTION_WARP_ROTATION_MODE::FACE_TARGET: return "FACE_TARGET";
        case MOTION_WARP_ROTATION_MODE::MATCH_TARGET_ROTATION: return "MATCH_TARGET_ROTATION";
        default: return "UNKNOWN";
        }
    }
}

MotionWarpComponent::~MotionWarpComponent()
{
    UnregisterActions();
}

void MotionWarpComponent::Start()
{
    // アセット側にプリセットが無い場合でも、攻撃用の最低限のワープ設定で動けるようにする。
    EnsureDefaultPresets();
    RegisterActions();
}

void MotionWarpComponent::EnsureDefaultPresets()
{
    auto hasPreset_ = [this](const std::string& presetName_)
    {
        return std::any_of(
            presets.begin(),
            presets.end(),
            [&presetName_](const MotionWarpPreset& preset_)
            {
                return preset_.presetName == presetName_;
            });
    };

    // 弱攻撃、強攻撃、溜め、空中攻撃で補正量を分け、手触りの初期値を用意する。
    if (!hasPreset_("WeakAttackWarp"))
    {
        MotionWarpPreset weak_;
        weak_.presetName = "WeakAttackWarp";
        weak_.positionMode = MOTION_WARP_POSITION_MODE::DIRECTION_ONLY;
        weak_.endNormalizedTime = 0.28f;
        weak_.targetDistance = 1.1f;
        weak_.positionStrength = 0.35f;
        weak_.rotationStrength = 0.6f;
        weak_.minimumPositionScale = 0.9f;
        weak_.maximumPositionScale = 1.1f;
        weak_.maximumTotalPositionCorrection = 0.4f;
        weak_.maximumRotationDegrees = 30.0f;
        weak_.maximumRotationDegreesPerSecond = 300.0f;
        presets.push_back(weak_);
    }

    if (!hasPreset_("StrongAttackWarp"))
    {
        MotionWarpPreset strong_;
        strong_.presetName = "StrongAttackWarp";
        strong_.endNormalizedTime = 0.38f;
        strong_.targetDistance = 1.25f;
        strong_.positionStrength = 0.65f;
        strong_.rotationStrength = 0.8f;
        strong_.minimumPositionScale = 0.8f;
        strong_.maximumPositionScale = 1.25f;
        strong_.maximumTotalPositionCorrection = 0.9f;
        strong_.maximumRotationDegrees = 45.0f;
        strong_.maximumRotationDegreesPerSecond = 360.0f;
        presets.push_back(strong_);
    }

    if (!hasPreset_("ChargeAttackWarp"))
    {
        MotionWarpPreset charge_;
        charge_.presetName = "ChargeAttackWarp";
        charge_.positionMode = MOTION_WARP_POSITION_MODE::TARGET_END_POSITION;
        charge_.endNormalizedTime = 0.5f;
        charge_.targetDistance = 1.4f;
        charge_.positionStrength = 0.8f;
        charge_.rotationStrength = 0.65f;
        charge_.minimumPositionScale = 0.75f;
        charge_.maximumPositionScale = 1.35f;
        charge_.maximumTotalPositionCorrection = 1.5f;
        charge_.maximumRotationDegrees = 35.0f;
        charge_.maximumRotationDegreesPerSecond = 280.0f;
        presets.push_back(charge_);
    }

    if (!hasPreset_("AirAttackWarp"))
    {
        MotionWarpPreset air_;
        air_.presetName = "AirAttackWarp";
        air_.positionMode = MOTION_WARP_POSITION_MODE::DIRECTION_ONLY;
        air_.endNormalizedTime = 0.3f;
        air_.targetDistance = 1.0f;
        air_.positionStrength = 0.25f;
        air_.rotationStrength = 0.5f;
        air_.minimumPositionScale = 0.95f;
        air_.maximumPositionScale = 1.05f;
        air_.maximumTotalPositionCorrection = 0.3f;
        air_.maximumRotationDegrees = 30.0f;
        presets.push_back(air_);
    }

    if (!hasPreset_("CounterAttackWarp"))
    {
        MotionWarpPreset counter_;
        counter_.presetName = "CounterAttackWarp";
        counter_.positionMode = MOTION_WARP_POSITION_MODE::TARGET_END_POSITION;
        counter_.endNormalizedTime = 0.65f;
        counter_.targetDistance = 1.15f;
        counter_.positionStrength = 0.95f;
        counter_.rotationStrength = 1.0f;
        counter_.minimumPositionScale = 0.65f;
        counter_.maximumPositionScale = 1.6f;
        counter_.maximumPositionCorrectionPerFrame = 0.45f;
        counter_.maximumTotalPositionCorrection = 2.4f;
        counter_.maximumRotationDegrees = 75.0f;
        counter_.maximumRotationDegreesPerSecond = 540.0f;
        counter_.maximumWarpTargetDistance = 16.0f;
        presets.push_back(counter_);
    }
}

MotionWarpPreset* MotionWarpComponent::FindPreset(const std::string& presetName_)
{
    const std::string& name_ = presetName_.empty() ? defaultPresetName : presetName_;
    auto it_ = std::find_if(
        presets.begin(),
        presets.end(),
        [&](const MotionWarpPreset& preset_) { return preset_.presetName == name_; });
    return it_ != presets.end() ? &(*it_) : nullptr;
}

const MotionWarpPreset* MotionWarpComponent::FindPreset(
    const std::string& presetName_) const
{
    const std::string& name_ = presetName_.empty() ? defaultPresetName : presetName_;
    auto it_ = std::find_if(
        presets.begin(),
        presets.end(),
        [&](const MotionWarpPreset& preset_) { return preset_.presetName == name_; });
    return it_ != presets.end() ? &(*it_) : nullptr;
}

void MotionWarpComponent::Play(const std::string& presetName_)
{
    EnsureDefaultPresets();
    const MotionWarpPreset* preset_ = FindPreset(presetName_);
    if (!preset_)
    {
        debugInactiveReason = "Preset not found";
        return;
    }

    if (request.active &&
        request.presetName == preset_->presetName &&
        request.requestName == preset_->presetName)
    {
        return;
    }

    request = {};
    request.active = true;
    // リクエスト名とプリセット名を揃え、Stop側から現在のワープだけを止められるようにする。
    request.requestName = preset_->presetName;
    request.presetName = preset_->presetName;
    if (!ResolveRequestTarget(request, *preset_) &&
        preset_->stopWhenTargetInvalid)
    {
        request.active = false;
        debugInactiveReason = "Target not found";
    }
}

void MotionWarpComponent::SetRegisterGlobalActions(bool enabled_)
{
    if (registerGlobalActions == enabled_) return;
    const bool registered_ = actionsRegistered;
    UnregisterActions();
    registerGlobalActions = enabled_;
    if (registered_) RegisterActions();
}

void MotionWarpComponent::AddPresetIfMissing(const MotionWarpPreset& preset_)
{
    if (!FindPreset(preset_.presetName)) presets.push_back(preset_);
}

bool MotionWarpComponent::PlayForTarget(const std::string& presetName_,
    const std::shared_ptr<GameObject>& target_, const DirectX::XMFLOAT3& plannedRootMotion_,
    float stopDistance_, float startNormalizedTime_, float endNormalizedTime_, const DirectX::XMFLOAT3* fixedTargetPosition_,
    std::function<bool(const DirectX::XMFLOAT3&, const DirectX::XMFLOAT3&)> pathValidator_,
    float priorRotationCorrectionDegrees_, const DirectX::XMFLOAT3* fixedPathStart_)
{
    BS_PROFILE_SCOPE("MotionWarpComponent::PlayForTarget");
    Stop();
    const auto owner_ = GetGameObject();
    const auto preset_ = FindPreset(presetName_);
    if (!IsEnabled() || !preset_ || !owner_ || !owner_->transform || !target_ || !target_->transform) return false;
    // 保存プリセットは書き換えず、判定開始時刻とColliderに応じた停止距離だけ要求へ反映する。
    explicitTargetPreset = *preset_;
    explicitTargetPreset.startNormalizedTime = (std::max)(preset_->startNormalizedTime, startNormalizedTime_);
    explicitTargetPreset.rotationStartNormalizedTime = (std::max)(preset_->rotationStartNormalizedTime, startNormalizedTime_);
    explicitTargetPreset.endNormalizedTime = (std::min)(preset_->endNormalizedTime, endNormalizedTime_);
    explicitTargetPreset.rotationEndNormalizedTime = (std::min)(preset_->rotationEndNormalizedTime, endNormalizedTime_);
    explicitTargetPreset.targetDistance = (std::max)(preset_->targetDistance, stopDistance_);
    const bool matchFixedPath_ = fixedPathStart_ != nullptr && fixedTargetPosition_ != nullptr;
    if (matchFixedPath_)
    {
        // 予兆と共有する明示経路では、保存プリセットの停止距離や補正窓で寸法を変えない。
        explicitTargetPreset.startNormalizedTime = startNormalizedTime_;
        explicitTargetPreset.endNormalizedTime = endNormalizedTime_;
        explicitTargetPreset.targetDistance = 0.0f;
        if (!preset_->warpPosition) return false;
    }
    if (explicitTargetPreset.endNormalizedTime <= explicitTargetPreset.startNormalizedTime) return false;
    request = {};
    request.requestName = presetName_;
    request.presetName = presetName_;
    request.targetObject = target_;
    request.explicitTarget = true;
    // 予備動作で消費した旋回量も含め、同じ攻撃で最大角度を使い直さない。
    request.accumulatedRotationCorrectionDegrees = std::clamp(priorRotationCorrectionDegrees_, 0.0f, preset_->maximumRotationDegrees);
    request.maintainStopDistance = true;
    request.matchFixedPath = matchFixedPath_;
    request.freezeAfterWarp = fixedTargetPosition_ != nullptr && explicitTargetPreset.targetDistance <= 0.0001f;
    request.targetLocked = preset_->lockTargetOnStart || fixedTargetPosition_ != nullptr;
    request.startPosition = request.matchFixedPath ? *fixedPathStart_ : owner_->transform->GetWorldPosition();
    request.targetPosition = fixedTargetPosition_ ? *fixedTargetPosition_ : target_->transform->GetWorldPosition();
    request.initialTargetPosition = request.targetPosition;
    request.plannedRootMotion = plannedRootMotion_;
    request.pathValidator = std::move(pathValidator_);
    request.active = true;
    debugInactiveReason.clear();
    return true;
}

void MotionWarpComponent::Stop(const std::string& requestName_)
{
    if (!requestName_.empty() && request.requestName != requestName_)
    {
        return;
    }

    request.active = false;
    request.maintainStopDistance = false;
}

void MotionWarpComponent::Clear()
{
    request = {};
    debugInactiveReason.clear();
}

std::shared_ptr<GameObject> MotionWarpComponent::ResolveTarget(
    const MotionWarpPreset& preset_)
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return nullptr;

    // 攻撃補助ターゲット、ロックオンターゲットなど、プリセットに応じた対象を実行時に解決する。
    if (preset_.targetMode == MOTION_WARP_TARGET_MODE::ATTACK_ASSIST_TARGET)
    {
        if (const std::shared_ptr<PlayerControllerComponent> player_ =
            owner_->GetComponent<PlayerControllerComponent>())
        {
            return player_->GetAttackAssistTarget();
        }
    }

    if (preset_.targetMode == MOTION_WARP_TARGET_MODE::LOCK_ON_TARGET)
    {
        const std::shared_ptr<CameraLockOnComponent> lockOn_ =
            ResolveCameraLockOnComponent();
        return lockOn_ ? lockOn_->GetLockOnTarget() : nullptr;
    }

    return nullptr;
}

std::shared_ptr<CameraLockOnComponent>
MotionWarpComponent::ResolveCameraLockOnComponent()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return nullptr;

    // LockOnComponentは所有者に限らないため、シーン内から最初に見つかったものを使う。
    for (const std::shared_ptr<GameObject>& object_ : owner_->GetManager().GetObjects())
    {
        if (!object_) continue;
        const std::shared_ptr<CameraLockOnComponent> component_ =
            object_->GetComponent<CameraLockOnComponent>();
        if (component_) return component_;
    }

    return nullptr;
}

bool MotionWarpComponent::ResolveRequestTarget(
    MotionWarpRequest& request_,
    const MotionWarpPreset& preset_)
{
    // WorldPosition指定は、外部からrequest_.targetPositionが設定されている前提で成立させる。
    if (preset_.targetMode == MOTION_WARP_TARGET_MODE::WORLD_POSITION)
    {
        return true;
    }

    const std::shared_ptr<GameObject> target_ = ResolveTarget(preset_);
    if (!target_ || !target_->transform) return false;

    request_.targetObject = target_;
    request_.targetPosition = target_->transform->GetWorldPosition();
    DirectX::XMFLOAT3 forward_
    {
        target_->transform->GetFront().x,
        0.0f,
        target_->transform->GetFront().z
    };
    if (NormalizeHorizontal(forward_))
    {
        // ターゲット側の向きも保存しておき、将来的な回転一致モードで利用できるようにする。
        request_.targetForward = forward_;
    }
    return true;
}

MotionWarpResult MotionWarpComponent::ApplyRootMotionWarp(
    const ModelAnimator::RootMotionDelta& rootMotionDelta_,
    float normalizedTime_,
    float deltaTime_)
{
    BS_PROFILE_SCOPE("MotionWarpComponent::ApplyRootMotionWarp");

    MotionWarpResult result_;
    result_.rootMotionDelta = rootMotionDelta_;
    debugOriginalDelta =
    {
        rootMotionDelta_.deltaPosition.x,
        rootMotionDelta_.deltaPosition.y,
        rootMotionDelta_.deltaPosition.z
    };
    debugWarpedDelta = debugOriginalDelta;
    debugAppliedPositionScale = 1.0f;
    debugAppliedYawDegrees = 0.0f;

    // Off時は停止距離制限も加えず、入力された自然なRootMotionをそのまま返す。
    if (!warpEnabled)
    {
        debugInactiveReason = "Comparison: MotionWarp OFF";
        return result_;
    }

    // 追加移動と旋回を終了した後も、同じPhaseの自然なRootMotionに停止円だけを適用する。
    auto limitCompletedWarp_ = [&]()
    {
        if (!IsEnabled() || !request.explicitTarget || !request.maintainStopDistance || !explicitTargetPreset.warpPosition || explicitTargetPreset.positionMode == MOTION_WARP_POSITION_MODE::NONE) return;
        const auto owner_ = GetGameObject();
        const auto target_ = request.targetObject.lock();
        if (!owner_ || !owner_->transform || !target_ || !target_->transform) return;
        if (request.freezeAfterWarp)
        {
            // 突進や中央移動の確定経路を、補正終了後の自然RootMotionで越えないようにする。
            result_.rootMotionDelta.deltaPosition.x = 0.0f;
            result_.rootMotionDelta.deltaPosition.z = 0.0f;
            result_.positionWarped = true;
            debugWarpedDelta = result_.rootMotionDelta.deltaPosition;
            return;
        }
        result_.positionWarped = LimitRootMotionToStopDistance(result_.rootMotionDelta.deltaPosition,
            owner_->transform->GetWorldPosition(), (explicitTargetPreset.discardUnappliedCorrection && request.targetLocked) ? request.targetPosition : target_->transform->GetWorldPosition(), explicitTargetPreset.targetDistance);
        debugWarpedDelta = result_.rootMotionDelta.deltaPosition;
    };
    auto limitFixedPath_ = [&]()
    {
        if (!request.explicitTarget || !request.freezeAfterWarp) return;
        const auto owner_ = GetGameObject();
        if (!owner_ || !owner_->transform) return;
        DirectX::XMFLOAT3 direction_{ request.targetPosition.x - request.startPosition.x, 0.0f,
            request.targetPosition.z - request.startPosition.z };
        const float length_ = GetHorizontalLength(direction_);
        // 開始地点が終点と同じ場合も、移動クリップの自然RootMotionで離れないようにする。
        if (!NormalizeHorizontal(direction_))
        {
            result_.rootMotionDelta.deltaPosition.x = 0.0f;
            result_.rootMotionDelta.deltaPosition.z = 0.0f;
        }
        else
        {
            const auto position_ = owner_->transform->GetWorldPosition();
            const float progress_ = (position_.x - request.startPosition.x) * direction_.x +
                (position_.z - request.startPosition.z) * direction_.z;
            const float remaining_ = (std::max)(0.0f, length_ - progress_);
            const float advance_ = std::clamp(result_.rootMotionDelta.deltaPosition.x * direction_.x +
                result_.rootMotionDelta.deltaPosition.z * direction_.z, 0.0f, remaining_);
            // 停止距離0の固定終点要求だけを直線へ制限し、横揺れや終点の通過を防ぐ。
            result_.rootMotionDelta.deltaPosition.x = direction_.x * advance_;
            result_.rootMotionDelta.deltaPosition.z = direction_.z * advance_;
        }
        result_.positionWarped = true;
        debugWarpedDelta = result_.rootMotionDelta.deltaPosition;
    };
    if (!IsEnabled() || !request.active)
    {
        limitCompletedWarp_();
        debugInactiveReason = "Inactive";
        return result_;
    }

    // アニメーション時間で補正窓を判定し、終了後は必要な停止距離制限だけを残す。
    const MotionWarpPreset* preset_ = request.explicitTarget ? &explicitTargetPreset : FindPreset(request.presetName);
    if (!preset_)
    {
        request.active = false;
        debugInactiveReason = "Preset missing";
        return result_;
    }

    const float sampleNormalizedTime_ = std::clamp(normalizedTime_, 0.0f, 1.0f);
    const bool finishExplicitWarp_ = request.explicitTarget && request.normalizedTime < preset_->endNormalizedTime &&
        sampleNormalizedTime_ >= preset_->endNormalizedTime;
    // 終端をまたいだフレームも最後の補正を適用し、FPSの違いで届かない分を残さない。
    request.normalizedTime = finishExplicitWarp_ ? preset_->endNormalizedTime : sampleNormalizedTime_;
    const float weight_ = finishExplicitWarp_ ? 1.0f : CalculateWarpWeight(request.normalizedTime, *preset_);
    const bool facingWindow_ = request.explicitTarget && preset_->warpRotation &&
        request.normalizedTime >= preset_->rotationStartNormalizedTime &&
        request.normalizedTime <= preset_->rotationEndNormalizedTime;
    if (weight_ <= 0.0f && !facingWindow_)
    {
        debugInactiveReason = "Outside time";
        if (request.normalizedTime > preset_->endNormalizedTime)
        {
            request.active = false;
            request.targetLocked = true;
            limitCompletedWarp_();
        }
        else limitFixedPath_();
        return result_;
    }

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->transform)
    {
        request.active = false;
        debugInactiveReason = "Owner invalid";
        return result_;
    }

    std::shared_ptr<GameObject> target_ = request.targetObject.lock();
    if (!target_ && !ResolveRequestTarget(request, *preset_))
    {
        if (preset_->stopWhenTargetInvalid)
        {
            request.active = false;
        }
        debugInactiveReason = "Target invalid";
        return result_;
    }
    target_ = request.targetObject.lock();

    if (target_ && target_->transform)
    {
        if (request.explicitTarget)
        {
            // 予備動作中の小さな移動だけを追い、回避で制限を超えた時点で最後の位置を固定する。
            if (!request.targetLocked && request.normalizedTime >= preset_->lockTargetNormalizedTime) request.targetLocked = true;
            if (!request.targetLocked)
            {
                const auto candidate_ = target_->transform->GetWorldPosition();
                const DirectX::XMFLOAT3 travel_{ candidate_.x - request.initialTargetPosition.x, 0.0f, candidate_.z - request.initialTargetPosition.z };
                DirectX::XMFLOAT3 initialDirection_{ request.initialTargetPosition.x - request.startPosition.x, 0.0f, request.initialTargetPosition.z - request.startPosition.z };
                DirectX::XMFLOAT3 candidateDirection_{ candidate_.x - request.startPosition.x, 0.0f, candidate_.z - request.startPosition.z };
                const bool hasDirections_ = NormalizeHorizontal(initialDirection_) && NormalizeHorizontal(candidateDirection_);
                const float angle_ = hasDirections_ ? std::acos(std::clamp(initialDirection_.x * candidateDirection_.x + initialDirection_.z * candidateDirection_.z, -1.0f, 1.0f)) * RADIANS_TO_DEGREES : 180.0f;
                if (GetHorizontalLength(travel_) <= preset_->maximumTargetTravelDistance && angle_ <= preset_->maximumTargetTravelAngleDegrees)
                    request.targetPosition = candidate_;
                else request.targetLocked = true;
            }
        }
        else
        {
            // 従来のPlayer要求は既存の追従経路を維持する。
            request.targetPosition = target_->transform->GetWorldPosition();
        }
    }

    const DirectX::XMFLOAT3 ownerPosition_ = owner_->transform->GetWorldPosition();
    if (request.matchFixedPath)
    {
        // 予兆で確定した線分をアニメーション時間でたどり、最大・最小補正距離による短縮を防ぐ。
        float progress_ = std::clamp((request.normalizedTime - preset_->startNormalizedTime) /
            (std::max)(preset_->endNormalizedTime - preset_->startNormalizedTime, 0.0001f), 0.0f, 1.0f);
        if (progress_ > 0.0f && progress_ < 1.0f)
            if (const auto curve_ = owner_->GetComponent<CurveComponent>();
                curve_ && !preset_->positionCorrectionCurveName.empty() && curve_->HasTrack(preset_->positionCorrectionCurveName))
                progress_ = std::clamp(curve_->GetValue(preset_->positionCorrectionCurveName, progress_), 0.0f, 1.0f);
        const DirectX::XMFLOAT3 destination_ = {
            request.startPosition.x + (request.targetPosition.x - request.startPosition.x) * progress_,
            ownerPosition_.y,
            request.startPosition.z + (request.targetPosition.z - request.startPosition.z) * progress_ };
        result_.rootMotionDelta.deltaPosition.x = destination_.x - ownerPosition_.x;
        result_.rootMotionDelta.deltaPosition.z = destination_.z - ownerPosition_.z;
        result_.positionWarped = true;
        // Transformは直接移動せず、従来のRootMotion・CharacterMovement・物理同期へ差分を渡す。
        if (request.pathValidator && !request.pathValidator(ownerPosition_, destination_))
        {
            request.active = false;
            result_.rootMotionDelta.deltaPosition.x = 0.0f;
            result_.rootMotionDelta.deltaPosition.z = 0.0f;
            debugInactiveReason = "Warp path blocked or unsupported";
        }
        else
        {
            if (finishExplicitWarp_)
            {
                request.active = false;
                request.targetLocked = true;
                request.normalizedTime = sampleNormalizedTime_;
            }
            debugInactiveReason.clear();
        }
        debugIdealStopPosition = request.targetPosition;
        debugWarpedDelta = result_.rootMotionDelta.deltaPosition;
        return result_;
    }
    DirectX::XMFLOAT3 toTarget_
    {
        request.targetPosition.x - ownerPosition_.x,
        0.0f,
        request.targetPosition.z - ownerPosition_.z
    };
    const float targetDistance_ = GetHorizontalLength(toTarget_);
    const bool limitPosition_ = (!request.explicitTarget || weight_ > 0.0f) && preset_->warpPosition &&
        preset_->positionMode != MOTION_WARP_POSITION_MODE::NONE;
    if (targetDistance_ > preset_->maximumWarpTargetDistance)
    {
        debugInactiveReason = "Target distance rejected";
        return result_;
    }
    if (targetDistance_ < preset_->minimumTargetDistance ||
        !NormalizeHorizontal(toTarget_))
    {
        // 方向補正できない近距離でも、停止距離の内側へ進む成分は制限する。
        if (limitPosition_)
        {
            result_.positionWarped = LimitRootMotionToStopDistance(
                result_.rootMotionDelta.deltaPosition,
                ownerPosition_, request.targetPosition, preset_->targetDistance);
            debugWarpedDelta = result_.rootMotionDelta.deltaPosition;
        }
        debugInactiveReason = "Target distance rejected";
        return result_;
    }

    debugIdealStopPosition =
    {
        // ターゲットの手前targetDistanceで止まる位置を理想停止位置として使う。
        request.targetPosition.x - toTarget_.x * preset_->targetDistance,
        ownerPosition_.y,
        request.targetPosition.z - toTarget_.z * preset_->targetDistance
    };

    // 後退や横移動を接近方向へ曲げないよう、元のRootMotionが対象へ向かう場合だけ補正する。
    const float originalApproach_ =
        rootMotionDelta_.deltaPosition.x * toTarget_.x +
        rootMotionDelta_.deltaPosition.z * toTarget_.z;
    if ((!request.explicitTarget || preset_->positionMode != MOTION_WARP_POSITION_MODE::TARGET_END_POSITION) && limitPosition_ && originalApproach_ > 0.0f)
    {
        DirectX::XMFLOAT3 delta_
        {
            result_.rootMotionDelta.deltaPosition.x,
            result_.rootMotionDelta.deltaPosition.y,
            result_.rootMotionDelta.deltaPosition.z
        };
            const float originalHorizontalLength_ = GetHorizontalLength(delta_);
            if (originalHorizontalLength_ >= preset_->minimumRemainingRootMotionDistance)
            {
                // 元のRootMotion方向と理想停止位置方向をブレンドし、攻撃の踏み込み方向だけ補正する。
                DirectX::XMFLOAT3 originalDirection_ = delta_;
                NormalizeHorizontal(originalDirection_);

            DirectX::XMFLOAT3 toStop_
            {
                debugIdealStopPosition.x - ownerPosition_.x,
                0.0f,
                debugIdealStopPosition.z - ownerPosition_.z
            };
            const float stopDistance_ = GetHorizontalLength(toStop_);
            if (NormalizeHorizontal(toStop_))
            {
                DirectX::XMFLOAT3 warpedDirection_ = originalDirection_;
                if (preset_->positionMode == MOTION_WARP_POSITION_MODE::DIRECTION_ONLY ||
                    preset_->positionMode == MOTION_WARP_POSITION_MODE::DIRECTION_AND_SCALE ||
                    preset_->positionMode == MOTION_WARP_POSITION_MODE::TARGET_END_POSITION)
                {
                    const float directionStrength_ =
                        std::clamp(preset_->positionStrength * weight_, 0.0f, 1.0f);
                    warpedDirection_.x += (toStop_.x - warpedDirection_.x) * directionStrength_;
                    warpedDirection_.z += (toStop_.z - warpedDirection_.z) * directionStrength_;
                    NormalizeHorizontal(warpedDirection_);
                }

                float scale_ = 1.0f;
                if (preset_->positionMode == MOTION_WARP_POSITION_MODE::SCALE_ONLY ||
                    preset_->positionMode == MOTION_WARP_POSITION_MODE::DIRECTION_AND_SCALE ||
                    preset_->positionMode == MOTION_WARP_POSITION_MODE::TARGET_END_POSITION)
                {
                    // 目標までの残り距離に合わせてRootMotionの水平距離を伸縮する。
                    scale_ = std::clamp(
                        stopDistance_ / (std::max)(originalHorizontalLength_, 0.001f),
                        preset_->minimumPositionScale,
                        preset_->maximumPositionScale);
                    scale_ = 1.0f + (scale_ - 1.0f) * preset_->positionStrength * weight_;
                }

                const float desiredHorizontalLength_ = originalHorizontalLength_ * scale_;
                DirectX::XMFLOAT3 warpedDelta_
                {
                    warpedDirection_.x * desiredHorizontalLength_,
                    delta_.y,
                    warpedDirection_.z * desiredHorizontalLength_
                };
                DirectX::XMFLOAT3 correction_
                {
                    warpedDelta_.x - delta_.x,
                    0.0f,
                    warpedDelta_.z - delta_.z
                };
                float correctionLength_ = GetHorizontalLength(correction_);
                const float remainingCorrection_ =
                    preset_->maximumTotalPositionCorrection -
                    request.accumulatedPositionCorrection;
                const float frameCorrectionLimit_ =
                    (std::min)(preset_->maximumPositionCorrectionPerFrame, remainingCorrection_);
                // 1フレーム/1リクエスト全体の補正量を制限し、急な吸い付きにならないようにする。
                if (correctionLength_ > frameCorrectionLimit_ &&
                    correctionLength_ > MINIMUM_HORIZONTAL_LENGTH_SQUARED)
                {
                    const float correctionScale_ = frameCorrectionLimit_ / correctionLength_;
                    correction_.x *= correctionScale_;
                    correction_.z *= correctionScale_;
                    correctionLength_ = frameCorrectionLimit_;
                }

                if (correctionLength_ > 0.0f && remainingCorrection_ > 0.0f)
                {
                    result_.rootMotionDelta.deltaPosition.x += correction_.x;
                    result_.rootMotionDelta.deltaPosition.z += correction_.z;
                    request.accumulatedPositionCorrection += correctionLength_;
                    debugAppliedPositionScale = scale_;
                    result_.positionWarped = true;
                }
            }
        }
    }

    if (request.explicitTarget && limitPosition_ && preset_->positionMode == MOTION_WARP_POSITION_MODE::TARGET_END_POSITION)
    {
        // 区間全体の予定RootMotionに不足する量だけを足し、元の毎フレーム移動は残す。
        DirectX::XMFLOAT3 startDirection_{ request.targetPosition.x - request.startPosition.x, 0.0f, request.targetPosition.z - request.startPosition.z };
        const float startDistance_ = GetHorizontalLength(startDirection_);
        if (NormalizeHorizontal(startDirection_))
        {
            const float neededDistance_ = (std::max)(0.0f, startDistance_ - preset_->targetDistance);
            const float plannedApproach_ = request.plannedRootMotion.x * startDirection_.x + request.plannedRootMotion.z * startDirection_.z;
            DirectX::XMFLOAT3 totalCorrection_{ 0.0f, 0.0f, 0.0f };
            if (startDistance_ > preset_->targetDistance + plannedApproach_)
            {
                totalCorrection_ = { startDirection_.x * neededDistance_ - request.plannedRootMotion.x, 0.0f,
                    startDirection_.z * neededDistance_ - request.plannedRootMotion.z };
                const float length_ = GetHorizontalLength(totalCorrection_);
                const float maximum_ = preset_->maximumTotalPositionCorrection * preset_->positionStrength;
                if (length_ > maximum_ && length_ > 0.0001f)
                {
                    totalCorrection_.x *= maximum_ / length_;
                    totalCorrection_.z *= maximum_ / length_;
                }
            }
            float progress_ = std::clamp((request.normalizedTime - preset_->startNormalizedTime) /
                (std::max)(preset_->endNormalizedTime - preset_->startNormalizedTime, 0.0001f), 0.0f, 1.0f);
            const auto curve_ = owner_->GetComponent<CurveComponent>();
            if (curve_ && !preset_->positionCorrectionCurveName.empty() && curve_->HasTrack(preset_->positionCorrectionCurveName))
                progress_ = std::clamp(curve_->GetValue(preset_->positionCorrectionCurveName, progress_), 0.0f, 1.0f);
            const DirectX::XMFLOAT3 scheduled_{ totalCorrection_.x * progress_, 0.0f, totalCorrection_.z * progress_ };
            DirectX::XMFLOAT3 correction_{ scheduled_.x - request.appliedPlannedCorrection.x, 0.0f,
                scheduled_.z - request.appliedPlannedCorrection.z };
            if (preset_->discardUnappliedCorrection)
            {
                // 物理結果が予定より短くても、未消化分を後からまとめて加算しない。
                correction_ = { scheduled_.x - request.scheduledPositionCorrection.x, 0.0f,
                    scheduled_.z - request.scheduledPositionCorrection.z };
                request.scheduledPositionCorrection = scheduled_;
            }
            const float length_ = GetHorizontalLength(correction_);
            const float remaining_ = (std::max)(0.0f, preset_->maximumTotalPositionCorrection - request.accumulatedPositionCorrection);
            const float limit_ = (std::min)(remaining_, (std::min)(preset_->maximumPositionCorrectionPerFrame,
                preset_->maximumPositionCorrectionPerSecond * (std::max)(deltaTime_, 0.0f)));
            if (length_ > limit_ && length_ > 0.0001f)
            {
                correction_.x *= limit_ / length_;
                correction_.z *= limit_ / length_;
            }
            result_.rootMotionDelta.deltaPosition.x += correction_.x;
            result_.rootMotionDelta.deltaPosition.z += correction_.z;
            request.appliedPlannedCorrection.x += correction_.x;
            request.appliedPlannedCorrection.z += correction_.z;
            request.accumulatedPositionCorrection += GetHorizontalLength(correction_);
            result_.positionWarped = GetHorizontalLength(correction_) > 0.0f;
        }
    }

    limitFixedPath_();
    // 吸い付き補正の上限とは独立して最終差分を制限し、大きなDeltaでも停止円を越えさせない。
    if (limitPosition_ && LimitRootMotionToStopDistance(
        result_.rootMotionDelta.deltaPosition,
        ownerPosition_, request.targetPosition, preset_->targetDistance))
    {
        result_.positionWarped = true;
    }

    // 壁や地面のない経路が生じた場合は補正を停止し、実移動は通常のCharacterMovementに任せる。
    if (request.explicitTarget && request.pathValidator)
    {
        const DirectX::XMFLOAT3 destination_{ ownerPosition_.x + result_.rootMotionDelta.deltaPosition.x,
            ownerPosition_.y, ownerPosition_.z + result_.rootMotionDelta.deltaPosition.z };
        if (!request.pathValidator(ownerPosition_, destination_))
        {
            request.active = false;
            result_.rootMotionDelta.deltaPosition.x = 0.0f;
            result_.rootMotionDelta.deltaPosition.z = 0.0f;
            debugInactiveReason = "Warp path blocked or unsupported";
            debugWarpedDelta = result_.rootMotionDelta.deltaPosition;
            return result_;
        }
    }
    float rotationWeight_ = weight_;
    if (request.explicitTarget)
    {
        rotationWeight_ = facingWindow_ ? 1.0f : 0.0f;
        if (request.normalizedTime > preset_->rotationFullStrengthEndNormalizedTime)
            rotationWeight_ *= std::clamp((preset_->rotationEndNormalizedTime - request.normalizedTime) /
                (std::max)(preset_->rotationEndNormalizedTime - preset_->rotationFullStrengthEndNormalizedTime, 0.0001f), 0.0f, 1.0f);
    }

    if (preset_->warpRotation &&
        preset_->rotationMode == MOTION_WARP_ROTATION_MODE::FACE_TARGET)
    {
        // 位置補正とは別に、所有者のYawだけをターゲット方向へ少しずつ向ける。
        DirectX::XMFLOAT3 currentForward_
        {
            owner_->transform->GetFront().x,
            0.0f,
            owner_->transform->GetFront().z
        };
        if (NormalizeHorizontal(currentForward_))
        {
            const float currentYaw_ = GetYawDegrees(currentForward_);
            const float targetYaw_ = GetYawDegrees(toTarget_);
            const float yawDifference_ = NormalizeAngleDegrees(targetYaw_ - currentYaw_);
            const float remainingDegrees_ =
                preset_->maximumRotationDegrees -
                request.accumulatedRotationCorrectionDegrees;
            const float frameDegrees_ =
                preset_->maximumRotationDegreesPerSecond *
                (std::max)(deltaTime_, 0.0f) *
                preset_->rotationStrength *
                rotationWeight_;
            const float allowedDegrees_ =
                (std::min)((std::max)(remainingDegrees_, 0.0f), frameDegrees_);
            const float appliedDegrees_ =
                std::clamp(yawDifference_, -allowedDegrees_, allowedDegrees_);
            if (std::abs(appliedDegrees_) > 0.001f)
            {
                const DirectX::XMFLOAT3 correctedForward_ =
                    DirectionFromYawDegrees(currentYaw_ + appliedDegrees_);
                owner_->transform->SetForwardYawOnly(correctedForward_, 1.0f, 1.0f);
                request.accumulatedRotationCorrectionDegrees += std::abs(appliedDegrees_);
                debugAppliedYawDegrees = appliedDegrees_;
                result_.rotationWarped = true;
            }
        }
    }

    debugWarpedDelta =
    {
        result_.rootMotionDelta.deltaPosition.x,
        result_.rootMotionDelta.deltaPosition.y,
        result_.rootMotionDelta.deltaPosition.z
    };
    if (finishExplicitWarp_)
    {
        request.active = false;
        request.targetLocked = true;
        request.normalizedTime = sampleNormalizedTime_;
    }
    debugInactiveReason.clear();
    return result_;
}

void MotionWarpComponent::RegisterActions()
{
    if (actionsRegistered) return;
    // アニメーションイベントや外部スクリプトから、名前指定でワープを開始/停止できるようにする。
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    ActionRegistry& registry_ = registerGlobalActions ? ActionRegistry::Instance() : owner_->GetLocalRegistry();
    registry_.RegisterAction("MotionWarp", "MotionWarp.Play", [this]() { Play(); });
    registry_.RegisterAction("MotionWarp", "MotionWarp.Stop", [this]() { Stop(); });
    registry_.RegisterAction("MotionWarp", "MotionWarp.Clear", [this]() { Clear(); });
    actionsRegistered = true;
}

void MotionWarpComponent::UnregisterActions()
{
    if (!actionsRegistered) return;
    const auto owner_ = GetGameObject();
    if (!registerGlobalActions && !owner_) { actionsRegistered = false; return; }
    ActionRegistry& registry_ = registerGlobalActions ? ActionRegistry::Instance() : owner_->GetLocalRegistry();
    registry_.RemoveAction("MotionWarp", "MotionWarp.Play");
    registry_.RemoveAction("MotionWarp", "MotionWarp.Stop");
    registry_.RemoveAction("MotionWarp", "MotionWarp.Clear");
    actionsRegistered = false;
}

void MotionWarpComponent::DrawComparisonGui()
{
    ImGui::PushID(this);
    ImGui::Checkbox("MotionWarp ON (A/B Comparison)", &warpEnabled);
    ImGui::Text("Warp: %s / Preset: %s", warpEnabled ? "ON" : "OFF", request.presetName.c_str());
    ImGui::Text("Request: %s", request.active ? "ACTIVE" : "INACTIVE");
    ImGui::Text("Original movement: %.4f, %.4f, %.4f", debugOriginalDelta.x, debugOriginalDelta.y, debugOriginalDelta.z);
    ImGui::Text("Applied movement: %.4f, %.4f, %.4f", debugWarpedDelta.x, debugWarpedDelta.y, debugWarpedDelta.z);
    ImGui::Text("Phase correction total: %.3f m / %.1f deg", request.accumulatedPositionCorrection, request.accumulatedRotationCorrectionDegrees);
    ImGui::Text("Warp status: %s", debugInactiveReason.c_str());
    ImGui::PopID();
}

void MotionWarpComponent::Gui()
{
    EnsureDefaultPresets();
    DrawComparisonGui();
    // デバッグ値は、補正前後のRootMotionと補正が止まった理由を確認するために表示する。
    ImGui::Text("Active: %s", request.active ? "true" : "false");
    ImGui::Text("Preset: %s", request.presetName.c_str());
    ImGui::Text("Inactive Reason: %s", debugInactiveReason.c_str());
    ImGui::Text("Original Delta: %.3f, %.3f, %.3f", debugOriginalDelta.x, debugOriginalDelta.y, debugOriginalDelta.z);
    ImGui::Text("Warped Delta: %.3f, %.3f, %.3f", debugWarpedDelta.x, debugWarpedDelta.y, debugWarpedDelta.z);
    ImGui::Text("Ideal Stop: %.3f, %.3f, %.3f", debugIdealStopPosition.x, debugIdealStopPosition.y, debugIdealStopPosition.z);
    ImGui::Text("Position Scale: %.3f", debugAppliedPositionScale);
    ImGui::Text("Yaw Delta: %.3f", debugAppliedYawDegrees);
    ImGui::Text("Accum Position: %.3f", request.accumulatedPositionCorrection);
    ImGui::Text("Accum Rotation: %.3f", request.accumulatedRotationCorrectionDegrees);
    ImGui::Text("Start: %.3f, %.3f, %.3f", request.startPosition.x, request.startPosition.y, request.startPosition.z);
    ImGui::Text("Target: %.3f, %.3f, %.3f / Locked: %s", request.targetPosition.x, request.targetPosition.y, request.targetPosition.z, request.targetLocked ? "YES" : "NO");
    ImGui::Text("Normalized Time: %.3f", request.normalizedTime);

    ImGui::SeparatorText("Presets");
    ImGui::InputText("Default Preset", &defaultPresetName);
    for (MotionWarpPreset& preset_ : presets)
    {
        if (!ImGui::TreeNode(preset_.presetName.c_str())) continue;
        ImGui::InputText("Preset Name", &preset_.presetName);
        ImGui::Text("Target Mode: %s", ToString(preset_.targetMode));
        ImGui::Text("Position Mode: %s", ToString(preset_.positionMode));
        ImGui::Text("Rotation Mode: %s", ToString(preset_.rotationMode));
        ImGui::SliderFloat("Start Time", &preset_.startNormalizedTime, 0.0f, 1.0f);
        ImGui::SliderFloat("End Time", &preset_.endNormalizedTime, 0.0f, 1.0f);
        ImGui::DragFloat("Target Distance", &preset_.targetDistance, 0.01f, 0.0f, 10.0f);
        ImGui::SliderFloat("Position Strength", &preset_.positionStrength, 0.0f, 1.0f);
        ImGui::SliderFloat("Rotation Strength", &preset_.rotationStrength, 0.0f, 1.0f);
        ImGui::DragFloat("Min Position Scale", &preset_.minimumPositionScale, 0.01f, 0.0f, 5.0f);
        ImGui::DragFloat("Max Position Scale", &preset_.maximumPositionScale, 0.01f, 0.0f, 5.0f);
        ImGui::DragFloat("Max Correction Per Frame", &preset_.maximumPositionCorrectionPerFrame, 0.01f, 0.0f, 5.0f);
        ImGui::DragFloat("Max Total Position Correction", &preset_.maximumTotalPositionCorrection, 0.01f, 0.0f, 20.0f);
        ImGui::DragFloat("Max Rotation", &preset_.maximumRotationDegrees, 0.1f, 0.0f, 180.0f);
        ImGui::DragFloat("Max Rotation Per Second", &preset_.maximumRotationDegreesPerSecond, 1.0f, 0.0f, 1080.0f);
        ImGui::Checkbox("Warp Position", &preset_.warpPosition);
        ImGui::Checkbox("Warp Rotation", &preset_.warpRotation);
        ImGui::Checkbox("Horizontal Only", &preset_.horizontalOnly);
        ImGui::Checkbox("Lock Target On Start", &preset_.lockTargetOnStart);
        ImGui::Checkbox("Stop When Target Invalid", &preset_.stopWhenTargetInvalid);
        ImGui::Checkbox("Continue Across Combo", &preset_.continueAcrossCombo);
        ImGui::Checkbox("Use Unscaled Time", &preset_.useUnscaledTime);
        ImGui::DragFloat("MaximumPositionCorrectionPerSecond", &preset_.maximumPositionCorrectionPerSecond, 0.01f, 0.0f, 180.0f);
        ImGui::Checkbox("Discard Unapplied Correction", &preset_.discardUnappliedCorrection);
        ImGui::DragFloat("LockTargetNormalizedTime", &preset_.lockTargetNormalizedTime, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("MaximumTargetTravelDistance", &preset_.maximumTargetTravelDistance, 0.01f, 0.0f, 180.0f);
        ImGui::DragFloat("MaximumTargetTravelAngleDegrees", &preset_.maximumTargetTravelAngleDegrees, 0.01f, 0.0f, 180.0f);
        ImGui::DragFloat("RotationStartNormalizedTime", &preset_.rotationStartNormalizedTime, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("RotationFullStrengthEndNormalizedTime", &preset_.rotationFullStrengthEndNormalizedTime, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("RotationEndNormalizedTime", &preset_.rotationEndNormalizedTime, 0.01f, 0.0f, 1.0f);
        ImGui::InputText("Position Correction Curve", &preset_.positionCorrectionCurveName);
        ImGui::TreePop();
    }
}

void MotionWarpComponent::Serialize(nlohmann::json& json_) const
{
    // プリセットはアニメーションごとの調整値なので、コンポーネント設定としてまとめて保存する。
    json_["DefaultPresetName"] = defaultPresetName;
    json_["WarpEnabled"] = warpEnabled;
    json_["RegisterGlobalActions"] = registerGlobalActions;
    nlohmann::json presetsJson_ = nlohmann::json::array();
    for (const MotionWarpPreset& preset_ : presets)
    {
        presetsJson_.push_back(
            {
                { "PresetName", preset_.presetName },
                { "TargetMode", static_cast<int>(preset_.targetMode) },
                { "PositionMode", static_cast<int>(preset_.positionMode) },
                { "RotationMode", static_cast<int>(preset_.rotationMode) },
                { "StartNormalizedTime", preset_.startNormalizedTime },
                { "EndNormalizedTime", preset_.endNormalizedTime },
                { "TargetDistance", preset_.targetDistance },
                { "PositionStrength", preset_.positionStrength },
                { "RotationStrength", preset_.rotationStrength },
                { "MinimumPositionScale", preset_.minimumPositionScale },
                { "MaximumPositionScale", preset_.maximumPositionScale },
                { "MaximumPositionCorrectionPerFrame", preset_.maximumPositionCorrectionPerFrame },
                { "MaximumTotalPositionCorrection", preset_.maximumTotalPositionCorrection },
                { "MaximumRotationDegrees", preset_.maximumRotationDegrees },
                { "MaximumRotationDegreesPerSecond", preset_.maximumRotationDegreesPerSecond },
                { "MinimumRemainingRootMotionDistance", preset_.minimumRemainingRootMotionDistance },
                { "MaximumWarpTargetDistance", preset_.maximumWarpTargetDistance },
                { "MinimumTargetDistance", preset_.minimumTargetDistance },
                { "WarpPosition", preset_.warpPosition },
                { "WarpRotation", preset_.warpRotation },
                { "HorizontalOnly", preset_.horizontalOnly },
                { "LockTargetOnStart", preset_.lockTargetOnStart },
                { "StopWhenTargetInvalid", preset_.stopWhenTargetInvalid },
                { "ContinueAcrossCombo", preset_.continueAcrossCombo },
                { "UseUnscaledTime", preset_.useUnscaledTime },
                { "MaximumPositionCorrectionPerSecond", preset_.maximumPositionCorrectionPerSecond },
                { "LockTargetNormalizedTime", preset_.lockTargetNormalizedTime },
                { "DiscardUnappliedCorrection", preset_.discardUnappliedCorrection },
                { "MaximumTargetTravelDistance", preset_.maximumTargetTravelDistance },
                { "MaximumTargetTravelAngleDegrees", preset_.maximumTargetTravelAngleDegrees },
                { "RotationStartNormalizedTime", preset_.rotationStartNormalizedTime },
                { "RotationFullStrengthEndNormalizedTime", preset_.rotationFullStrengthEndNormalizedTime },
                { "RotationEndNormalizedTime", preset_.rotationEndNormalizedTime },
                { "PositionCorrectionCurveName", preset_.positionCorrectionCurveName }
            });
    }
    json_["Presets"] = presetsJson_;
}

void MotionWarpComponent::Deserialize(const nlohmann::json& json_)
{
    const bool registered_ = actionsRegistered;
    UnregisterActions();
    Clear();
    defaultPresetName = json_.value("DefaultPresetName", defaultPresetName);
    warpEnabled = json_.value("WarpEnabled", true);
    registerGlobalActions = json_.value("RegisterGlobalActions", true);
    // 保存済みプリセットを読み直し、不正値は範囲内へ丸めてRootMotion補正の暴走を防ぐ。
    presets.clear();
    if (json_.contains("Presets") && json_["Presets"].is_array())
    {
        for (const auto& presetJson_ : json_["Presets"])
        {
            MotionWarpPreset preset_;
            preset_.presetName = presetJson_.value("PresetName", preset_.presetName);
            preset_.targetMode = static_cast<MOTION_WARP_TARGET_MODE>(
                presetJson_.value("TargetMode", static_cast<int>(preset_.targetMode)));
            preset_.positionMode = static_cast<MOTION_WARP_POSITION_MODE>(
                presetJson_.value("PositionMode", static_cast<int>(preset_.positionMode)));
            preset_.rotationMode = static_cast<MOTION_WARP_ROTATION_MODE>(
                presetJson_.value("RotationMode", static_cast<int>(preset_.rotationMode)));
            preset_.startNormalizedTime = std::clamp(presetJson_.value("StartNormalizedTime", preset_.startNormalizedTime), 0.0f, 1.0f);
            preset_.endNormalizedTime = std::clamp(presetJson_.value("EndNormalizedTime", preset_.endNormalizedTime), 0.0f, 1.0f);
            preset_.targetDistance = (std::max)(presetJson_.value("TargetDistance", preset_.targetDistance), 0.0f);
            preset_.positionStrength = std::clamp(presetJson_.value("PositionStrength", preset_.positionStrength), 0.0f, 1.0f);
            preset_.rotationStrength = std::clamp(presetJson_.value("RotationStrength", preset_.rotationStrength), 0.0f, 1.0f);
            preset_.minimumPositionScale = (std::max)(presetJson_.value("MinimumPositionScale", preset_.minimumPositionScale), 0.0f);
            preset_.maximumPositionScale = (std::max)(presetJson_.value("MaximumPositionScale", preset_.maximumPositionScale), preset_.minimumPositionScale);
            preset_.maximumPositionCorrectionPerFrame = (std::max)(presetJson_.value("MaximumPositionCorrectionPerFrame", preset_.maximumPositionCorrectionPerFrame), 0.0f);
            preset_.maximumTotalPositionCorrection = (std::max)(presetJson_.value("MaximumTotalPositionCorrection", preset_.maximumTotalPositionCorrection), 0.0f);
            preset_.maximumRotationDegrees = std::clamp(presetJson_.value("MaximumRotationDegrees", preset_.maximumRotationDegrees), 0.0f, 180.0f);
            preset_.maximumRotationDegreesPerSecond = (std::max)(presetJson_.value("MaximumRotationDegreesPerSecond", preset_.maximumRotationDegreesPerSecond), 0.0f);
            preset_.minimumRemainingRootMotionDistance = (std::max)(presetJson_.value("MinimumRemainingRootMotionDistance", preset_.minimumRemainingRootMotionDistance), 0.0f);
            preset_.maximumWarpTargetDistance = (std::max)(presetJson_.value("MaximumWarpTargetDistance", preset_.maximumWarpTargetDistance), 0.0f);
            preset_.minimumTargetDistance = (std::max)(presetJson_.value("MinimumTargetDistance", preset_.minimumTargetDistance), 0.0f);
            preset_.warpPosition = presetJson_.value("WarpPosition", preset_.warpPosition);
            preset_.warpRotation = presetJson_.value("WarpRotation", preset_.warpRotation);
            preset_.horizontalOnly = presetJson_.value("HorizontalOnly", preset_.horizontalOnly);
            preset_.lockTargetOnStart = presetJson_.value("LockTargetOnStart", preset_.lockTargetOnStart);
            preset_.stopWhenTargetInvalid = presetJson_.value("StopWhenTargetInvalid", preset_.stopWhenTargetInvalid);
            preset_.continueAcrossCombo = presetJson_.value("ContinueAcrossCombo", preset_.continueAcrossCombo);
            preset_.useUnscaledTime = presetJson_.value("UseUnscaledTime", preset_.useUnscaledTime);
            preset_.maximumPositionCorrectionPerSecond = (std::max)(presetJson_.value("MaximumPositionCorrectionPerSecond", 24.0f), 0.0f);
            preset_.discardUnappliedCorrection = presetJson_.value("DiscardUnappliedCorrection", false);
            preset_.lockTargetNormalizedTime = std::clamp(presetJson_.value("LockTargetNormalizedTime", 0.0f), 0.0f, 1.0f);
            preset_.maximumTargetTravelDistance = (std::max)(presetJson_.value("MaximumTargetTravelDistance", 0.6f), 0.0f);
            preset_.maximumTargetTravelAngleDegrees = std::clamp(presetJson_.value("MaximumTargetTravelAngleDegrees", 20.0f), 0.0f, 180.0f);
            preset_.rotationStartNormalizedTime = std::clamp(presetJson_.value("RotationStartNormalizedTime", 0.0f), 0.0f, 1.0f);
            preset_.rotationFullStrengthEndNormalizedTime = std::clamp(presetJson_.value("RotationFullStrengthEndNormalizedTime", 0.2f), 0.0f, 1.0f);
            preset_.rotationEndNormalizedTime = std::clamp(presetJson_.value("RotationEndNormalizedTime", 0.4f), 0.0f, 1.0f);
            preset_.positionCorrectionCurveName = presetJson_.value("PositionCorrectionCurveName", std::string());
            presets.push_back(preset_);
        }
    }
    EnsureDefaultPresets();
    if (registered_) RegisterActions();
}

REGISTER_COMPONENT(MotionWarpComponent, "MotionWarpComponent")
