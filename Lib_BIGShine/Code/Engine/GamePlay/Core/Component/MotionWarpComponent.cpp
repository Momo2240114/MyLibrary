#include "PrecompiledHeader.h"

#include "MotionWarpComponent.h"

#include "Engine/GamePlay/Camera/CameraLockOnComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Game/Charcter/Player/PlayerControllerComponent.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <imgui_stdlib.h>

namespace
{
    constexpr float MINIMUM_HORIZONTAL_LENGTH_SQUARED = 0.000001f;
    constexpr float DEGREES_TO_RADIANS = DirectX::XM_PI / 180.0f;
    constexpr float RADIANS_TO_DEGREES = 180.0f / DirectX::XM_PI;

    bool NormalizeHorizontal(DirectX::XMFLOAT3& direction_)
    {
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

    float GetYawDegrees(const DirectX::XMFLOAT3& direction_)
    {
        return std::atan2(direction_.x, direction_.z) * RADIANS_TO_DEGREES;
    }

    float NormalizeAngleDegrees(float angle_)
    {
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
    EnsureDefaultPresets();
    RegisterActions();
}

void MotionWarpComponent::EnsureDefaultPresets()
{
    if (!presets.empty()) return;

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
    request.requestName = preset_->presetName;
    request.presetName = preset_->presetName;
    if (!ResolveRequestTarget(request, *preset_) &&
        preset_->stopWhenTargetInvalid)
    {
        request.active = false;
        debugInactiveReason = "Target not found";
    }
}

void MotionWarpComponent::Stop(const std::string& requestName_)
{
    if (!requestName_.empty() && request.requestName != requestName_)
    {
        return;
    }

    request.active = false;
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

    if (!request.active)
    {
        debugInactiveReason = "Inactive";
        return result_;
    }

    const MotionWarpPreset* preset_ = FindPreset(request.presetName);
    if (!preset_)
    {
        request.active = false;
        debugInactiveReason = "Preset missing";
        return result_;
    }

    request.normalizedTime = std::clamp(normalizedTime_, 0.0f, 1.0f);
    const float weight_ = CalculateWarpWeight(request.normalizedTime, *preset_);
    if (weight_ <= 0.0f)
    {
        debugInactiveReason = "Outside time";
        if (request.normalizedTime > preset_->endNormalizedTime)
        {
            request.active = false;
        }
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
        request.targetPosition = target_->transform->GetWorldPosition();
    }

    const DirectX::XMFLOAT3 ownerPosition_ = owner_->transform->GetWorldPosition();
    DirectX::XMFLOAT3 toTarget_
    {
        request.targetPosition.x - ownerPosition_.x,
        0.0f,
        request.targetPosition.z - ownerPosition_.z
    };
    const float targetDistance_ = GetHorizontalLength(toTarget_);
    if (targetDistance_ < preset_->minimumTargetDistance ||
        targetDistance_ > preset_->maximumWarpTargetDistance ||
        !NormalizeHorizontal(toTarget_))
    {
        debugInactiveReason = "Target distance rejected";
        return result_;
    }

    debugIdealStopPosition =
    {
        request.targetPosition.x - toTarget_.x * preset_->targetDistance,
        ownerPosition_.y,
        request.targetPosition.z - toTarget_.z * preset_->targetDistance
    };

    if (preset_->warpPosition &&
        preset_->positionMode != MOTION_WARP_POSITION_MODE::NONE)
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

    if (preset_->warpRotation &&
        preset_->rotationMode == MOTION_WARP_ROTATION_MODE::FACE_TARGET)
    {
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
                weight_;
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
    debugInactiveReason.clear();
    return result_;
}

void MotionWarpComponent::RegisterActions()
{
    if (actionsRegistered) return;
    ActionRegistry& registry_ = ActionRegistry::Instance();
    registry_.RegisterAction("MotionWarp", "MotionWarp.Play", [this]() { Play(); });
    registry_.RegisterAction("MotionWarp", "MotionWarp.Stop", [this]() { Stop(); });
    registry_.RegisterAction("MotionWarp", "MotionWarp.Clear", [this]() { Clear(); });
    actionsRegistered = true;
}

void MotionWarpComponent::UnregisterActions()
{
    if (!actionsRegistered) return;
    ActionRegistry& registry_ = ActionRegistry::Instance();
    registry_.RemoveAction("MotionWarp", "MotionWarp.Play");
    registry_.RemoveAction("MotionWarp", "MotionWarp.Stop");
    registry_.RemoveAction("MotionWarp", "MotionWarp.Clear");
    actionsRegistered = false;
}

void MotionWarpComponent::Gui()
{
    EnsureDefaultPresets();
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
        ImGui::TreePop();
    }
}

void MotionWarpComponent::Serialize(nlohmann::json& json_) const
{
    json_["DefaultPresetName"] = defaultPresetName;
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
                { "UseUnscaledTime", preset_.useUnscaledTime }
            });
    }
    json_["Presets"] = presetsJson_;
}

void MotionWarpComponent::Deserialize(const nlohmann::json& json_)
{
    defaultPresetName = json_.value("DefaultPresetName", defaultPresetName);
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
            presets.push_back(preset_);
        }
    }
    EnsureDefaultPresets();
}

REGISTER_COMPONENT(MotionWarpComponent, "MotionWarpComponent")
