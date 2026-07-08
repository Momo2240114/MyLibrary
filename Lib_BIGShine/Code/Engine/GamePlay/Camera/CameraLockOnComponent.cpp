#include "CameraLockOnComponent.h"

#include "CameraComponent.h"
#include "CameraManager.h"
#include "LockOnTargetComponent.h"
#include "ThirdPersonCameraComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Transform/Transform.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace
{
    DirectX::XMFLOAT3 AddFloat3(
        const DirectX::XMFLOAT3& left_,
        const DirectX::XMFLOAT3& right_)
    {
        return { left_.x + right_.x, left_.y + right_.y, left_.z + right_.z };
    }

    DirectX::XMFLOAT3 LerpFloat3(
        const DirectX::XMFLOAT3& start_,
        const DirectX::XMFLOAT3& end_,
        float rate_)
    {
        return
        {
            start_.x + (end_.x - start_.x) * rate_,
            start_.y + (end_.y - start_.y) * rate_,
            start_.z + (end_.z - start_.z) * rate_
        };
    }

    float ExpSmoothingRate(float speed_, float deltaTime_)
    {
        return 1.0f - std::exp(-speed_ * deltaTime_);
    }
}

void CameraLockOnComponent::Start()
{
    ResolveReferences();
    RegisterActions();
}

void CameraLockOnComponent::LateUpdate(float deltaTime_)
{
    ResolveReferences();
    if (!lockOnEnabled)
    {
        ReleaseThirdPersonCamera();
        return;
    }

    const std::shared_ptr<GameObject> player_ = playerTarget.lock();
    const std::shared_ptr<GameObject> enemy_ = lockOnTarget.lock();
    const std::shared_ptr<CameraComponent> camera_ = camera.lock();
    const std::shared_ptr<ThirdPersonCameraComponent> thirdPerson_ =
        thirdPersonCamera.lock();
    if (!player_ || !enemy_ || !camera_ ||
        !player_->transform || !enemy_->transform)
    {
        SetLockOnEnabled(false);
        return;
    }

    if (thirdPerson_)
    {
        thirdPerson_->SetControlSuppressed(true);
        ownsThirdPersonControl = true;
    }

    const LockOnDistanceInfo distanceInfo_ = CalculateLockOnDistanceInfo();

    if (!distanceInfo_.isValid)
    {
        SetLockOnEnabled(false);
        return;
    }

    const DirectX::XMFLOAT3 playerPosition_ = distanceInfo_.playerPosition;
    const DirectX::XMFLOAT3 enemyPosition_ = distanceInfo_.targetPosition;
    const float differenceX_ = distanceInfo_.difference.x;
    const float differenceY_ = distanceInfo_.difference.y;
    const float differenceZ_ = distanceInfo_.difference.z;
    const float horizontalDistance_ = distanceInfo_.horizontalDistance;
    const float targetDistance_ = distanceInfo_.targetDistance;

    if (targetDistance_ > maxDistance)
    {
        SetLockOnEnabled(false);
        return;
    }

    DirectX::XMFLOAT3 backDirection_{};
    if (horizontalDistance_ > 0.0001f)
    {
        backDirection_ =
        {
            differenceX_ / horizontalDistance_,
            0.0f,
            differenceZ_ / horizontalDistance_
        };
    }
    else
    {
        const DirectX::XMFLOAT3 cameraFront_ = camera_->GetFront();
        const float frontLength_ = std::sqrt(
            cameraFront_.x * cameraFront_.x + cameraFront_.z * cameraFront_.z);
        backDirection_ = frontLength_ > 0.0001f
            ? DirectX::XMFLOAT3{
                -cameraFront_.x / frontLength_,
                0.0f,
                -cameraFront_.z / frontLength_ }
            : DirectX::XMFLOAT3{ 0.0f, 0.0f, -1.0f };
    }

    const DirectX::XMFLOAT3 sideDirection_
    {
        -backDirection_.z,
        0.0f,
        backDirection_.x
    };
    const float desiredDistance_ = std::clamp(
        baseDistance + horizontalDistance_ * distanceScale,
        minCameraDistance,
        maxCameraDistance);
    const float desiredHeight_ = cameraHeight +
        horizontalDistance_ * separationHeightScale +
        (std::max)(enemyPosition_.y - playerPosition_.y, 0.0f) *
            heightDifferenceScale;
    const DirectX::XMFLOAT3 desiredPosition_
    {
        playerPosition_.x + backDirection_.x * desiredDistance_ +
            sideDirection_.x * sideOffset * currentSide,
        playerPosition_.y + desiredHeight_,
        playerPosition_.z + backDirection_.z * desiredDistance_ +
            sideDirection_.z * sideOffset * currentSide
    };

    const float focusRate_ = std::clamp(
        horizontalDistance_ / (std::max)(closeDistance, 0.001f),
        0.0f,
        1.0f);
    const float enemyFocusWeight_ = std::lerp(
        closeFocusBias,
        farFocusBias,
        focusRate_);
    const DirectX::XMFLOAT3 desiredLookAt_ = LerpFloat3(
        playerPosition_,
        enemyPosition_,
        enemyFocusWeight_);

    const std::shared_ptr<GameObject> previousTarget_ =
        previousLockOnTarget.lock();
    if (!hasTrackingPose || previousTarget_ != enemy_)
    {
        interpolatedPosition = camera_->GetEye();
        interpolatedLookAt = camera_->GetFocus();
        previousLockOnTarget = enemy_;
        hasTrackingPose = true;
    }

    interpolatedPosition = LerpFloat3(
        interpolatedPosition,
        desiredPosition_,
        ExpSmoothingRate(positionInterpolationSpeed, deltaTime_));
    interpolatedLookAt = LerpFloat3(
        interpolatedLookAt,
        desiredLookAt_,
        ExpSmoothingRate(lookAtInterpolationSpeed, deltaTime_));
    ApplyCameraPose(interpolatedPosition, interpolatedLookAt);
    if (thirdPerson_)
    {
        thirdPerson_->SynchronizeViewAngles(
            interpolatedPosition,
            interpolatedLookAt);
    }
}

void CameraLockOnComponent::SetLockOnEnabled(bool enabled_)
{
    lockOnEnabled = enabled_ && !lockOnTarget.expired();
    ResetTracking();
    if (!lockOnEnabled) ReleaseThirdPersonCamera();
}

void CameraLockOnComponent::SetPlayerTarget(
    const std::shared_ptr<GameObject>& playerTarget_)
{
    playerTarget = playerTarget_;
    playerTargetName = playerTarget_ && playerTarget_->nameComponent
        ? playerTarget_->nameComponent->name
        : std::string{};
    ResetTracking();
}

void CameraLockOnComponent::SetLockOnTarget(
    const std::shared_ptr<GameObject>& lockOnTarget_)
{
    lockOnTarget = lockOnTarget_;
    lockOnTargetName = lockOnTarget_ && lockOnTarget_->nameComponent
        ? lockOnTarget_->nameComponent->name
        : std::string{};
    if (!lockOnTarget_)
    {
        SetLockOnEnabled(false);
        return;
    }
    ResetTracking();
}

std::shared_ptr<GameObject> CameraLockOnComponent::FindBestLockOnTarget(
    const std::shared_ptr<GameObject>& playerTarget_) const
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->manager || !playerTarget_ || !playerTarget_->transform)
    {
        return nullptr;
    }

    const DirectX::XMFLOAT3 playerPosition_ =
        GetFocusPosition(playerTarget_, playerOffset);
    DirectX::XMFLOAT3 cameraForward_{ 0.0f, 0.0f, 1.0f };
    if (CameraManager* cameraManager_ = owner_->manager->GetCameraManager())
    {
        if (const std::shared_ptr<CameraComponent> activeCamera_ =
            cameraManager_->GetActiveCamera())
        {
            cameraForward_ = activeCamera_->GetFront();
        }
    }

    float forwardLength_ = std::sqrt(
        cameraForward_.x * cameraForward_.x +
        cameraForward_.z * cameraForward_.z);
    if (forwardLength_ <= 0.0001f)
    {
        cameraForward_ = { 0.0f, 0.0f, 1.0f };
        forwardLength_ = 1.0f;
    }
    cameraForward_.x /= forwardLength_;
    cameraForward_.z /= forwardLength_;

    std::shared_ptr<GameObject> bestTarget_;
    float bestScore_ = std::numeric_limits<float>::max();
    for (const std::shared_ptr<GameObject>& candidate_ : owner_->manager->GetObjects())
    {
        if (!candidate_ || candidate_ == playerTarget_ || !candidate_->transform ||
            !candidate_->GetComponent<LockOnTargetComponent>())
        {
            continue;
        }

        const DirectX::XMFLOAT3 targetPosition_ =
            GetFocusPosition(candidate_, targetOffset);
        const float differenceX_ = targetPosition_.x - playerPosition_.x;
        const float differenceZ_ = targetPosition_.z - playerPosition_.z;
        const float horizontalDistance_ = std::sqrt(
            differenceX_ * differenceX_ + differenceZ_ * differenceZ_);
        if (horizontalDistance_ <= 0.0001f || horizontalDistance_ > maxDistance)
        {
            continue;
        }

        const float directionX_ = differenceX_ / horizontalDistance_;
        const float directionZ_ = differenceZ_ / horizontalDistance_;
        const float centerDot_ = std::clamp(
            cameraForward_.x * directionX_ + cameraForward_.z * directionZ_,
            -1.0f,
            1.0f);
        const float centerPenalty_ = (1.0f - centerDot_) * maxDistance;
        const float score_ = horizontalDistance_ + centerPenalty_;
        if (score_ < bestScore_)
        {
            bestScore_ = score_;
            bestTarget_ = candidate_;
        }
    }

    return bestTarget_;
}

bool CameraLockOnComponent::ToggleLockOnForPlayer(
    const std::shared_ptr<GameObject>& playerTarget_)
{
    if (lockOnEnabled)
    {
        SetLockOnEnabled(false);
        return false;
    }

    SetPlayerTarget(playerTarget_);
    const std::shared_ptr<GameObject> target_ = FindBestLockOnTarget(playerTarget_);
    SetLockOnTarget(target_);
    SetLockOnEnabled(target_ != nullptr);
    return lockOnEnabled;
}

void CameraLockOnComponent::ResolveReferences()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    if (camera.expired()) camera = owner_->GetComponent<CameraComponent>();
    if (thirdPersonCamera.expired())
    {
        thirdPersonCamera = owner_->GetComponent<ThirdPersonCameraComponent>();
    }
    if (!owner_->manager) return;
    if (playerTarget.expired() && !playerTargetName.empty())
    {
        playerTarget = owner_->manager->Find(playerTargetName);
    }
    if (lockOnTarget.expired() && !lockOnTargetName.empty())
    {
        lockOnTarget = owner_->manager->Find(lockOnTargetName);
    }
}

void CameraLockOnComponent::RegisterActions()
{
    ActionRegistry& registry_ = ActionRegistry::Instance();
    registry_.RegisterAction("Camera", "LockOn_Enable", [this]()
    {
        ResolveReferences();
        SetLockOnEnabled(true);
    });
    registry_.RegisterAction("Camera", "LockOn_Disable", [this]()
    {
        SetLockOnEnabled(false);
    });
    registry_.RegisterAction("Camera", "LockOn_Toggle", [this]()
    {
        ResolveReferences();
        SetLockOnEnabled(!lockOnEnabled);
    });
    registry_.RegisterAction("Camera", "LockOn_SwitchSide", [this]()
    {
        currentSide = currentSide >= 0.0f ? -1.0f : 1.0f;
    });
}

void CameraLockOnComponent::ResetTracking()
{
    hasTrackingPose = false;
    previousLockOnTarget.reset();
}

void CameraLockOnComponent::ReleaseThirdPersonCamera()
{
    if (const std::shared_ptr<ThirdPersonCameraComponent> thirdPerson_ =
        thirdPersonCamera.lock())
    {
        if (!ownsThirdPersonControl) return;
        thirdPerson_->SetControlSuppressed(false);
        thirdPerson_->BeginReturnBlendFromCurrentCamera();
        ownsThirdPersonControl = false;
        return;
    }
    ownsThirdPersonControl = false;
}

void CameraLockOnComponent::ApplyCameraPose(
    const DirectX::XMFLOAT3& cameraPosition_,
    const DirectX::XMFLOAT3& lookAtPosition_)
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->transform) return;

    const DirectX::XMMATRIX viewMatrix_ = DirectX::XMMatrixLookAtLH(
        DirectX::XMLoadFloat3(&cameraPosition_),
        DirectX::XMLoadFloat3(&lookAtPosition_),
        DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    DirectX::XMVECTOR determinant_;
    owner_->transform->ApplyWorldMatrix(
        DirectX::XMMatrixInverse(&determinant_, viewMatrix_));
    if (const std::shared_ptr<CameraComponent> camera_ = camera.lock())
    {
        camera_->SetLookAt(
            cameraPosition_,
            lookAtPosition_,
            { 0.0f, 1.0f, 0.0f });
    }
}

DirectX::XMFLOAT3 CameraLockOnComponent::GetFocusPosition(
    const std::shared_ptr<GameObject>& object_,
    const DirectX::XMFLOAT3& fallbackOffset_) const
{
    if (!object_ || !object_->transform) return fallbackOffset_;
    if (const std::shared_ptr<LockOnTargetComponent> lockTarget_ =
        object_->GetComponent<LockOnTargetComponent>())
    {
        return lockTarget_->GetFocusPosition();
    }
    return AddFloat3(object_->transform->GetWorldPosition(), fallbackOffset_);
}

CameraLockOnComponent::LockOnDistanceInfo
CameraLockOnComponent::CalculateLockOnDistanceInfo() const
{
    LockOnDistanceInfo info_{};

    const std::shared_ptr<GameObject> player_ = playerTarget.lock();
    const std::shared_ptr<GameObject> target_ = lockOnTarget.lock();

    if (!player_ || !target_ || !player_->transform || !target_->transform)
    {
        return info_;
    }

    info_.playerPosition = GetFocusPosition(player_, playerOffset);
    info_.targetPosition = GetFocusPosition(target_, targetOffset);

    info_.difference =
    {
        info_.playerPosition.x - info_.targetPosition.x,
        info_.playerPosition.y - info_.targetPosition.y,
        info_.playerPosition.z - info_.targetPosition.z
    };

    info_.horizontalDistance = std::sqrt(
        info_.difference.x * info_.difference.x +
        info_.difference.z * info_.difference.z);

    info_.targetDistance = std::sqrt(
        info_.horizontalDistance * info_.horizontalDistance +
        info_.difference.y * info_.difference.y);

    info_.isValid = true;
    return info_;
}

void CameraLockOnComponent::DrawLockOnDebugGui()
{
    ImGui::SeparatorText("Debug");

    ResolveReferences();

    const LockOnDistanceInfo distanceInfo_ = CalculateLockOnDistanceInfo();

    if (!distanceInfo_.isValid)
    {
        ImGui::TextDisabled("LockOn Distance: invalid target");
        return;
    }

    ImGui::Text("Target Distance: %.3f", distanceInfo_.targetDistance);
    ImGui::Text("Horizontal Distance: %.3f", distanceInfo_.horizontalDistance);
    ImGui::Text("Height Difference: %.3f", distanceInfo_.difference.y);
    ImGui::Text("Max Distance: %.3f", maxDistance);

    ImGui::Text(
        "Player Focus: %.3f, %.3f, %.3f",
        distanceInfo_.playerPosition.x,
        distanceInfo_.playerPosition.y,
        distanceInfo_.playerPosition.z);

    ImGui::Text(
        "Target Focus: %.3f, %.3f, %.3f",
        distanceInfo_.targetPosition.x,
        distanceInfo_.targetPosition.y,
        distanceInfo_.targetPosition.z);

    if (distanceInfo_.targetDistance > maxDistance)
    {
        ImGui::TextColored(
            ImVec4(1.0f, 0.25f, 0.25f, 1.0f),
            "Out of LockOn Range");
    }
    else
    {
        ImGui::TextColored(
            ImVec4(0.25f, 1.0f, 0.25f, 1.0f),
            "In LockOn Range");
    }
}

void CameraLockOnComponent::Gui()
{
    ImGui::Checkbox("Lock On Enabled", &lockOnEnabled);

    char playerBuffer_[128]{};
    strcpy_s(playerBuffer_, playerTargetName.c_str());
    if (ImGui::InputText("Player Target", playerBuffer_, sizeof(playerBuffer_)))
    {
        playerTargetName = playerBuffer_;
        playerTarget.reset();
        ResolveReferences();
        ResetTracking();
    }
    char targetBuffer_[128]{};
    strcpy_s(targetBuffer_, lockOnTargetName.c_str());
    if (ImGui::InputText("Lock Target", targetBuffer_, sizeof(targetBuffer_)))
    {
        lockOnTargetName = targetBuffer_;
        lockOnTarget.reset();
        ResolveReferences();
        ResetTracking();
    }

    ImGui::DragFloat3("Player Offset", &playerOffset.x, 0.05f);
    ImGui::DragFloat3("Target Offset", &targetOffset.x, 0.05f);

    DrawLockOnDebugGui();


    ImGui::DragFloat("Max Distance", &maxDistance, 0.1f, 0.1f, 200.0f);
    ImGui::DragFloat("Base Distance", &baseDistance, 0.1f, 0.1f, 100.0f);
    ImGui::DragFloat("Distance Scale", &distanceScale, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloatRange2(
        "Camera Distance Range",
        &minCameraDistance,
        &maxCameraDistance,
        0.1f,
        0.1f,
        100.0f);
    ImGui::DragFloat("Camera Height", &cameraHeight, 0.05f, -20.0f, 50.0f);
    ImGui::DragFloat("Separation Height", &separationHeightScale, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("Height Difference", &heightDifferenceScale, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("Side Offset", &sideOffset, 0.05f, 0.0f, 20.0f);
    if (ImGui::RadioButton("Right Shoulder", currentSide > 0.0f)) currentSide = 1.0f;
    ImGui::SameLine();
    if (ImGui::RadioButton("Left Shoulder", currentSide < 0.0f)) currentSide = -1.0f;
    ImGui::DragFloat("Close Distance", &closeDistance, 0.1f, 0.1f, 100.0f);
    ImGui::DragFloat("Close Focus Bias", &closeFocusBias, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Far Focus Bias", &farFocusBias, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Position Speed", &positionInterpolationSpeed, 0.1f, 0.1f, 100.0f);
    ImGui::DragFloat("LookAt Speed", &lookAtInterpolationSpeed, 0.1f, 0.1f, 100.0f);
    ImGui::TextDisabled("Actions: LockOn_Enable / Disable / Toggle / SwitchSide");
}

void CameraLockOnComponent::Serialize(nlohmann::json& json_) const
{
    json_["playerTargetName"] = playerTargetName;
    json_["lockOnTargetName"] = lockOnTargetName;
    json_["lockOnEnabled"] = lockOnEnabled;
    json_["playerOffset"] = { playerOffset.x, playerOffset.y, playerOffset.z };
    json_["targetOffset"] = { targetOffset.x, targetOffset.y, targetOffset.z };
    json_["maxDistance"] = maxDistance;
    json_["baseDistance"] = baseDistance;
    json_["distanceScale"] = distanceScale;
    json_["minCameraDistance"] = minCameraDistance;
    json_["maxCameraDistance"] = maxCameraDistance;
    json_["cameraHeight"] = cameraHeight;
    json_["separationHeightScale"] = separationHeightScale;
    json_["heightDifferenceScale"] = heightDifferenceScale;
    json_["sideOffset"] = sideOffset;
    json_["currentSide"] = currentSide;
    json_["closeDistance"] = closeDistance;
    json_["closeFocusBias"] = closeFocusBias;
    json_["farFocusBias"] = farFocusBias;
    json_["positionInterpolationSpeed"] = positionInterpolationSpeed;
    json_["lookAtInterpolationSpeed"] = lookAtInterpolationSpeed;
}

void CameraLockOnComponent::Deserialize(const nlohmann::json& json_)
{
    playerTargetName = json_.value("playerTargetName", std::string{});
    lockOnTargetName = json_.value("lockOnTargetName", std::string{});
    lockOnEnabled = json_.value("lockOnEnabled", false);
    auto loadFloat3_ = [&json_](
        const char* key_,
        DirectX::XMFLOAT3& value_)
    {
        if (!json_.contains(key_) || !json_[key_].is_array() ||
            json_[key_].size() < 3) return;
        value_ =
        {
            json_[key_][0].get<float>(),
            json_[key_][1].get<float>(),
            json_[key_][2].get<float>()
        };
    };
    loadFloat3_("playerOffset", playerOffset);
    loadFloat3_("targetOffset", targetOffset);
    maxDistance = json_.value("maxDistance", 28.0f);
    baseDistance = json_.value("baseDistance", 7.0f);
    distanceScale = json_.value("distanceScale", 0.45f);
    minCameraDistance = json_.value("minCameraDistance", 6.0f);
    maxCameraDistance = json_.value("maxCameraDistance", 16.0f);
    cameraHeight = json_.value("cameraHeight", 2.2f);
    separationHeightScale = json_.value("separationHeightScale", 0.08f);
    heightDifferenceScale = json_.value("heightDifferenceScale", 0.35f);
    sideOffset = json_.value("sideOffset", 1.25f);
    currentSide = json_.value("currentSide", 1.0f) >= 0.0f ? 1.0f : -1.0f;
    closeDistance = json_.value("closeDistance", 8.0f);
    closeFocusBias = json_.value("closeFocusBias", 0.28f);
    farFocusBias = json_.value("farFocusBias", 0.5f);
    positionInterpolationSpeed = json_.value("positionInterpolationSpeed", 10.0f);
    lookAtInterpolationSpeed = json_.value("lookAtInterpolationSpeed", 12.5f);
    ResetTracking();
}

REGISTER_COMPONENT(CameraLockOnComponent, "CameraLockOnComponent")
