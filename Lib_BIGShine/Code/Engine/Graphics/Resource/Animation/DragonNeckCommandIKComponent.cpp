#include "DragonNeckCommandIKComponent.h"

#include "IKMapComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/Graphics/Resource/GlTFModel.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace
{
    constexpr float DEG_TO_RAD = 3.1415926535f / 180.0f;
    constexpr float RAD_TO_DEG = 180.0f / 3.1415926535f;
    constexpr const char* ACTION_CLEAR = "DragonNeck_Clear";
    constexpr const char* ACTION_LOOK_LEFT = "DragonNeck_LookLeft";
    constexpr const char* ACTION_LOOK_RIGHT = "DragonNeck_LookRight";
    constexpr const char* ACTION_LOOK_CENTER = "DragonNeck_LookCenter";
    constexpr const char* ACTION_SWEEP_YAW = "DragonNeck_SweepYaw";
    constexpr const char* ACTION_LOOK_WORLD_POSITION = "DragonNeck_LookWorldPosition";
    constexpr const char* ACTION_LOOK_TARGET_OBJECT_ON = "DragonNeck_TargetObjectLookOn";
    constexpr const char* ACTION_LOOK_TARGET_OBJECT_OFF = "DragonNeck_TargetObjectLookOff";

    DirectX::XMFLOAT3 GetTranslation(const DirectX::XMFLOAT4X4& matrix_)
    {
        return { matrix_._41, matrix_._42, matrix_._43 };
    }

    DirectX::XMFLOAT3 Normalize(const DirectX::XMFLOAT3& value_)
    {
        const float length_ = std::sqrt(
            value_.x * value_.x +
            value_.y * value_.y +
            value_.z * value_.z);

        if (length_ <= 0.00001f) return { 0.0f, 0.0f, 0.0f };

        return {
            value_.x / length_,
            value_.y / length_,
            value_.z / length_
        };
    }

    float SmoothStep(float value_)
    {
        const float clampedValue_ = std::clamp(value_, 0.0f, 1.0f);
        return clampedValue_ * clampedValue_ * (3.0f - 2.0f * clampedValue_);
    }

    float NormalizeAngleDegrees(float degrees_)
    {
        while (degrees_ > 180.0f) degrees_ -= 360.0f;
        while (degrees_ < -180.0f) degrees_ += 360.0f;
        return degrees_;
    }

    float CalculateYawDegrees(const DirectX::XMFLOAT3& direction_)
    {
        return std::atan2(direction_.x, direction_.z) * RAD_TO_DEG;
    }

    float CalculatePitchDegrees(const DirectX::XMFLOAT3& direction_)
    {
        const float horizontalLength_ =
            std::sqrt(direction_.x * direction_.x + direction_.z * direction_.z);
        return std::atan2(direction_.y, horizontalLength_) * RAD_TO_DEG;
    }
}

const std::string& DragonNeckCommandIKComponent::GetName() const
{
    static std::string name_ = "DragonNeckCommandIKComponent";
    return name_;
}

void DragonNeckCommandIKComponent::Start()
{
    const auto owner_ = GetGameObject();
    model = owner_ ? owner_->GetComponent<GltfModelCommponennt>().get() : nullptr;
    ikMap = owner_ ? owner_->GetComponent<IKMapComponent>().get() : nullptr;

    if (ikMap)
    {
        ikMap->ResolveChainByName(chainName);
    }

    if (model && !mouthNodeName.empty())
    {
        mouthNodeIndex = model->GetNodeIndex(mouthNodeName.c_str());
    }

    RegisterActions();
}

void DragonNeckCommandIKComponent::LateUpdate(float deltaTime_)
{
    if (!model || !ikMap) return;

    commandTimer += deltaTime_;
    ApplyTargetObjectLookMode();
    ApplyNeckIK(deltaTime_);

    const float totalTime_ =
        currentCommand.blendInTime +
        currentCommand.duration +
        currentCommand.blendOutTime;

    if (currentCommand.mode != DRAGON_NECK_IK_MODE::DISABLED &&
        commandTimer >= totalTime_ &&
        !currentCommand.keepLastDirection)
    {
        ClearCommand();
    }
}

void DragonNeckCommandIKComponent::ClearCommand()
{
    currentCommand = {};
    commandTimer = 0.0f;
}

void DragonNeckCommandIKComponent::UnregisterActions()
{
    const auto owner_ = GetGameObject();
    if (!owner_) return;

    const std::string localCategory_ =
        registeredActionCategory.empty() ? actionCategory : registeredActionCategory;
    for (const std::string& actionName_ : registeredActionNames)
    {
        owner_->GetLocalRegistry().RemoveAction(localCategory_, actionName_);
    }
    registeredActionNames.clear();
    registeredActionCategory.clear();

    const std::string globalCategory_ =
        registeredGlobalActionCategory.empty() ? actionCategory : registeredGlobalActionCategory;
    for (const std::string& actionName_ : registeredGlobalActionNames)
    {
        ActionRegistry::Instance().RemoveAction(globalCategory_, actionName_);
    }
    registeredGlobalActionNames.clear();
    registeredGlobalActionCategory.clear();
}

void DragonNeckCommandIKComponent::RegisterActions()
{
    const auto owner_ = GetGameObject();
    if (!owner_) return;

    UnregisterActions();
    registeredActionCategory = actionCategory;
    registeredGlobalActionCategory = actionCategory;

    auto registerAction_ = [this, &owner_](const std::string& actionName_, const ActionRegistry::ActionFunc& action_)
        {
            owner_->RegisterAction(actionCategory, actionName_, action_);
            registeredActionNames.push_back(owner_->BuildActionName(actionName_));

            if (registerGlobalActions)
            {
                const std::string globalActionName_ =
                    owner_->RegisterGlobalAction(actionCategory, actionName_, action_);
                if (!globalActionName_.empty()) registeredGlobalActionNames.push_back(globalActionName_);
            }
        };

    registerAction_(ACTION_CLEAR, [this]()
        {
            ClearCommand();
        });
    registerAction_(ACTION_LOOK_LEFT, [this]()
        {
            LookAtYawOffset(
                -actionYawDegrees,
                actionDuration,
                actionBlendInTime,
                actionBlendOutTime);
        });
    registerAction_(ACTION_LOOK_RIGHT, [this]()
        {
            LookAtYawOffset(
                actionYawDegrees,
                actionDuration,
                actionBlendInTime,
                actionBlendOutTime);
        });
    registerAction_(ACTION_LOOK_CENTER, [this]()
        {
            LookAtYawOffset(
                0.0f,
                actionDuration,
                actionBlendInTime,
                actionBlendOutTime);
        });
    registerAction_(ACTION_SWEEP_YAW, [this]()
        {
            SweepYaw(
                actionSweepStartYawDegrees,
                actionSweepEndYawDegrees,
                actionDuration,
                actionBlendInTime,
                actionBlendOutTime);
        });
    registerAction_(ACTION_LOOK_WORLD_POSITION, [this]()
        {
            LookAtWorldPosition(
                actionWorldTargetPosition,
                actionDuration,
                actionBlendInTime,
                actionBlendOutTime);
        });
    registerAction_(ACTION_LOOK_TARGET_OBJECT_ON, [this]()
        {
            SetTargetObjectLookMode(true);
        });
    registerAction_(ACTION_LOOK_TARGET_OBJECT_OFF, [this]()
        {
            SetTargetObjectLookMode(false);
        });
}

void DragonNeckCommandIKComponent::LookAtTarget(
    const std::string& targetName_,
    float duration_,
    float blendInTime_,
    float blendOutTime_)
{
    currentCommand = {};
    currentCommand.mode = DRAGON_NECK_IK_MODE::LOOK_AT_TARGET;
    currentCommand.targetName = targetName_;
    currentCommand.duration = duration_;
    currentCommand.blendInTime = blendInTime_;
    currentCommand.blendOutTime = blendOutTime_;
    commandTimer = 0.0f;
}

void DragonNeckCommandIKComponent::LookAtWorldPosition(
    const DirectX::XMFLOAT3& worldPosition_,
    float duration_,
    float blendInTime_,
    float blendOutTime_)
{
    currentCommand = {};
    currentCommand.mode = DRAGON_NECK_IK_MODE::LOOK_AT_WORLD_POSITION;
    currentCommand.worldTargetPosition = worldPosition_;
    currentCommand.duration = duration_;
    currentCommand.blendInTime = blendInTime_;
    currentCommand.blendOutTime = blendOutTime_;
    commandTimer = 0.0f;
}

void DragonNeckCommandIKComponent::LookAtYawOffset(
    float yawDegrees_,
    float duration_,
    float blendInTime_,
    float blendOutTime_)
{
    currentCommand = {};
    currentCommand.mode = DRAGON_NECK_IK_MODE::FIXED_YAW_OFFSET;
    currentCommand.startYawDegrees = yawDegrees_;
    currentCommand.endYawDegrees = yawDegrees_;
    currentCommand.duration = duration_;
    currentCommand.blendInTime = blendInTime_;
    currentCommand.blendOutTime = blendOutTime_;
    commandTimer = 0.0f;
}

void DragonNeckCommandIKComponent::SweepYaw(
    float startYawDegrees_,
    float endYawDegrees_,
    float duration_,
    float blendInTime_,
    float blendOutTime_)
{
    currentCommand = {};
    currentCommand.mode = DRAGON_NECK_IK_MODE::SWEEP_YAW;
    currentCommand.startYawDegrees = startYawDegrees_;
    currentCommand.endYawDegrees = endYawDegrees_;
    currentCommand.duration = duration_;
    currentCommand.blendInTime = blendInTime_;
    currentCommand.blendOutTime = blendOutTime_;
    commandTimer = 0.0f;
}

void DragonNeckCommandIKComponent::SetTargetObjectLookMode(bool enabled_)
{
    lookTargetObjectMode = enabled_;
    if (!lookTargetObjectMode)
    {
        ClearCommand();
        return;
    }

    currentCommand = {};
    currentCommand.mode = DRAGON_NECK_IK_MODE::LOOK_AT_TARGET;
    currentCommand.targetName = lookTargetObjectName;
    currentCommand.targetOffset = lookTargetObjectOffset;
    currentCommand.yawOffsetDegrees = lookTargetYawOffsetDegrees;
    currentCommand.pitchOffsetDegrees = lookTargetPitchOffsetDegrees;
    currentCommand.duration = 999999.0f;
    currentCommand.blendInTime = lookTargetBlendInTime;
    currentCommand.blendOutTime = 0.0f;
    currentCommand.maxYawDegrees = lookTargetMaxYawDegrees;
    currentCommand.maxPitchDegrees = lookTargetMaxPitchDegrees;
    currentCommand.keepLastDirection = true;
    commandTimer = 0.0f;
}

void DragonNeckCommandIKComponent::ApplyTargetObjectLookMode()
{
    if (!lookTargetObjectMode) return;

    if (currentCommand.mode == DRAGON_NECK_IK_MODE::LOOK_AT_TARGET &&
        currentCommand.targetName == lookTargetObjectName &&
        currentCommand.keepLastDirection)
    {
        currentCommand.targetOffset = lookTargetObjectOffset;
        currentCommand.yawOffsetDegrees = lookTargetYawOffsetDegrees;
        currentCommand.pitchOffsetDegrees = lookTargetPitchOffsetDegrees;
        currentCommand.maxYawDegrees = lookTargetMaxYawDegrees;
        currentCommand.maxPitchDegrees = lookTargetMaxPitchDegrees;
        return;
    }

    SetTargetObjectLookMode(true);
}

float DragonNeckCommandIKComponent::CalculateBlendRate() const
{
    if (currentCommand.mode == DRAGON_NECK_IK_MODE::DISABLED) return 0.0f;

    if (commandTimer < currentCommand.blendInTime)
    {
        if (currentCommand.blendInTime <= 0.0001f) return 1.0f;
        return SmoothStep(commandTimer / currentCommand.blendInTime);
    }

    const float holdEndTime_ = currentCommand.blendInTime + currentCommand.duration;
    if (commandTimer <= holdEndTime_) return 1.0f;

    const float blendOutTimer_ = commandTimer - holdEndTime_;
    if (currentCommand.blendOutTime <= 0.0001f) return 0.0f;

    return 1.0f - SmoothStep(blendOutTimer_ / currentCommand.blendOutTime);
}

bool DragonNeckCommandIKComponent::ResolveTargetPosition(DirectX::XMFLOAT3& targetPosition_)
{
    if (currentCommand.mode == DRAGON_NECK_IK_MODE::LOOK_AT_WORLD_POSITION)
    {
        targetPosition_ = currentCommand.worldTargetPosition;
        return true;
    }

    if (currentCommand.mode == DRAGON_NECK_IK_MODE::LOOK_AT_TARGET)
    {
        const auto owner_ = GetGameObject();
        if (!owner_ || currentCommand.targetName.empty()) return false;

        const std::shared_ptr<GameObject> targetObject_ =
            owner_->GetManager().Find(currentCommand.targetName);
        if (!targetObject_ || !targetObject_->transform) return false;

        const DirectX::XMFLOAT3 objectPosition_ = targetObject_->transform->GetWorldPosition();
        targetPosition_ = {
            objectPosition_.x + currentCommand.targetOffset.x,
            objectPosition_.y + currentCommand.targetOffset.y,
            objectPosition_.z + currentCommand.targetOffset.z
        };
        return true;
    }

    return false;
}

bool DragonNeckCommandIKComponent::ApplyNeckIK(float deltaTime_)
{
    (void)deltaTime_;

    auto* chain_ = ikMap->FindChain(chainName);
    if (!chain_ || !chain_->enabled) return false;
    if (!ikMap->ResolveChainByName(chainName)) return false;
    if (chain_->nodeIndices.size() < 2) return false;

    auto& nodes_ = model->GetNodes();

    float targetYawDegrees_ = 0.0f;
    float targetPitchDegrees_ = currentCommand.pitchOffsetDegrees;

    if (currentCommand.mode == DRAGON_NECK_IK_MODE::FIXED_YAW_OFFSET)
    {
        targetYawDegrees_ = currentCommand.startYawDegrees;
    }
    else if (currentCommand.mode == DRAGON_NECK_IK_MODE::SWEEP_YAW)
    {
        const float t_ = currentCommand.duration <= 0.0001f
            ? 1.0f
            : std::clamp(
                (commandTimer - currentCommand.blendInTime) / currentCommand.duration,
                0.0f,
                1.0f);

        targetYawDegrees_ =
            currentCommand.startYawDegrees +
            (currentCommand.endYawDegrees - currentCommand.startYawDegrees) * SmoothStep(t_);
    }
    else
    {
        DirectX::XMFLOAT3 targetPosition_;
        if (!ResolveTargetPosition(targetPosition_)) return false;

        const int rootNodeIndex_ = chain_->nodeIndices.front();
        const auto& rootNode_ = nodes_.at(rootNodeIndex_);
        const DirectX::XMMATRIX rootWorldMatrix_ =
            DirectX::XMLoadFloat4x4(&rootNode_.worldTransform);
        const DirectX::XMMATRIX inverseRootWorldMatrix_ =
            DirectX::XMMatrixInverse(nullptr, rootWorldMatrix_);
        DirectX::XMFLOAT3 localTargetPosition_;
        DirectX::XMStoreFloat3(
            &localTargetPosition_,
            DirectX::XMVector3TransformCoord(
                DirectX::XMLoadFloat3(&targetPosition_),
                inverseRootWorldMatrix_));
        const DirectX::XMFLOAT3 toTarget_ = Normalize(localTargetPosition_);

        if (toTarget_.x == 0.0f && toTarget_.y == 0.0f && toTarget_.z == 0.0f) return false;

        const DirectX::XMFLOAT3 forwardAxis_ = Normalize(localForwardAxis);
        const float baseYawDegrees_ = CalculateYawDegrees(forwardAxis_);
        const float basePitchDegrees_ = CalculatePitchDegrees(forwardAxis_);

        targetYawDegrees_ =
            NormalizeAngleDegrees(CalculateYawDegrees(toTarget_) - baseYawDegrees_) +
            currentCommand.yawOffsetDegrees;
        targetPitchDegrees_ =
            NormalizeAngleDegrees(CalculatePitchDegrees(toTarget_) - basePitchDegrees_) +
            currentCommand.pitchOffsetDegrees;
    }

    targetYawDegrees_ = std::clamp(
        targetYawDegrees_,
        -currentCommand.maxYawDegrees,
        currentCommand.maxYawDegrees);

    targetPitchDegrees_ = std::clamp(
        targetPitchDegrees_,
        -currentCommand.maxPitchDegrees,
        currentCommand.maxPitchDegrees);

    const float blendRate_ = CalculateBlendRate() * std::clamp(chain_->influence, 0.0f, 1.0f);
    if (blendRate_ <= 0.0001f) return false;

    const size_t boneCount_ = chain_->nodeIndices.size();
    for (size_t boneIndex_ = 0; boneIndex_ < boneCount_; ++boneIndex_)
    {
        const int nodeIndex_ = chain_->nodeIndices[boneIndex_];
        auto& node_ = nodes_.at(nodeIndex_);

        const float boneRate_ = boneCount_ <= 1
            ? 1.0f
            : static_cast<float>(boneIndex_) / static_cast<float>(boneCount_ - 1);

        const float weight_ =
            currentCommand.rootWeight +
            (currentCommand.tipWeight - currentCommand.rootWeight) * boneRate_;

        const float yawRadians_ = targetYawDegrees_ * DEG_TO_RAD * weight_ * blendRate_;
        const float pitchRadians_ = targetPitchDegrees_ * DEG_TO_RAD * weight_ * blendRate_;

        const DirectX::XMVECTOR localRotation_ = DirectX::XMLoadFloat4(&node_.rotation);
        const DirectX::XMVECTOR yawRotation_ = DirectX::XMQuaternionRotationAxis(
            DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
            yawRadians_);

        const DirectX::XMVECTOR pitchRotation_ = DirectX::XMQuaternionRotationAxis(
            DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),
            pitchRadians_);

        const DirectX::XMVECTOR resultRotation_ = DirectX::XMQuaternionNormalize(
            DirectX::XMQuaternionMultiply(localRotation_, DirectX::XMQuaternionMultiply(pitchRotation_, yawRotation_)));

        DirectX::XMStoreFloat4(&node_.rotation, resultRotation_);
        model->UpdateNodeTransformOnly(nodeIndex_);
    }

    model->UpdateNodeTransformFrom(chain_->nodeIndices.front());
    return true;
}

bool DragonNeckCommandIKComponent::GetMouthWorldPosition(DirectX::XMFLOAT3& position_) const
{
    if (!model || mouthNodeIndex < 0) return false;

    const auto& nodes_ = model->GetNodes();
    if (mouthNodeIndex >= static_cast<int>(nodes_.size())) return false;

    position_ = GetTranslation(nodes_.at(mouthNodeIndex).worldTransform);
    return true;
}

bool DragonNeckCommandIKComponent::GetMouthWorldForward(DirectX::XMFLOAT3& forward_) const
{
    if (!model || mouthNodeIndex < 0) return false;

    const auto& nodes_ = model->GetNodes();
    if (mouthNodeIndex >= static_cast<int>(nodes_.size())) return false;

    const DirectX::XMMATRIX worldMatrix_ = DirectX::XMLoadFloat4x4(&nodes_.at(mouthNodeIndex).worldTransform);
    DirectX::XMStoreFloat3(
        &forward_,
        DirectX::XMVector3Normalize(
            DirectX::XMVector3TransformNormal(
                DirectX::XMLoadFloat3(&localForwardAxis),
                worldMatrix_)));

    return true;
}

void DragonNeckCommandIKComponent::Gui()
{
    ImGui::TextDisabled("AI command driven neck IK.");

    char chainNameBuffer_[64];
    strncpy_s(chainNameBuffer_, chainName.c_str(), sizeof(chainNameBuffer_));
    if (ImGui::InputText("Chain Name", chainNameBuffer_, sizeof(chainNameBuffer_)))
    {
        chainName = chainNameBuffer_;
    }

    char mouthNodeNameBuffer_[64];
    strncpy_s(mouthNodeNameBuffer_, mouthNodeName.c_str(), sizeof(mouthNodeNameBuffer_));
    if (ImGui::InputText("Mouth Node", mouthNodeNameBuffer_, sizeof(mouthNodeNameBuffer_)))
    {
        mouthNodeName = mouthNodeNameBuffer_;
        mouthNodeIndex = model ? model->GetNodeIndex(mouthNodeName.c_str()) : -1;
    }

    ImGui::DragFloat3("Local Forward Axis", &localForwardAxis.x, 0.01f, -1.0f, 1.0f);

    ImGui::SeparatorText("Action");
    char actionCategoryBuffer_[64];
    strncpy_s(actionCategoryBuffer_, actionCategory.c_str(), sizeof(actionCategoryBuffer_));
    if (ImGui::InputText("Action Category", actionCategoryBuffer_, sizeof(actionCategoryBuffer_)))
    {
        actionCategory = actionCategoryBuffer_;
        RegisterActions();
    }

    if (ImGui::Checkbox("Register Global Actions", &registerGlobalActions))
    {
        RegisterActions();
    }

    ImGui::DragFloat("Action Duration", &actionDuration, 0.01f, 0.0f, 30.0f);
    ImGui::DragFloat("Action Blend In", &actionBlendInTime, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Action Blend Out", &actionBlendOutTime, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Action Yaw", &actionYawDegrees, 0.5f, 0.0f, 180.0f);
    ImGui::DragFloat("Sweep Start Yaw", &actionSweepStartYawDegrees, 0.5f, -180.0f, 180.0f);
    ImGui::DragFloat("Sweep End Yaw", &actionSweepEndYawDegrees, 0.5f, -180.0f, 180.0f);
    ImGui::DragFloat3("World Target", &actionWorldTargetPosition.x, 0.1f);

    ImGui::SeparatorText("Target Object Look");
    char lookTargetObjectNameBuffer_[64];
    strncpy_s(lookTargetObjectNameBuffer_, lookTargetObjectName.c_str(), sizeof(lookTargetObjectNameBuffer_));
    if (ImGui::InputText("Look Target Object", lookTargetObjectNameBuffer_, sizeof(lookTargetObjectNameBuffer_)))
    {
        lookTargetObjectName = lookTargetObjectNameBuffer_;
        if (lookTargetObjectMode)
        {
            SetTargetObjectLookMode(true);
        }
    }
    if (ImGui::Checkbox("Look Target Object Mode", &lookTargetObjectMode))
    {
        SetTargetObjectLookMode(lookTargetObjectMode);
    }
    ImGui::DragFloat("Look Blend In", &lookTargetBlendInTime, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat3("Look Target Offset", &lookTargetObjectOffset.x, 0.05f);
    ImGui::DragFloat("Look Base Yaw", &lookTargetYawOffsetDegrees, 0.5f, -180.0f, 180.0f);
    ImGui::DragFloat("Look Base Pitch", &lookTargetPitchOffsetDegrees, 0.5f, -90.0f, 90.0f);
    ImGui::DragFloat("Look Max Yaw", &lookTargetMaxYawDegrees, 0.5f, 0.0f, 180.0f);
    ImGui::DragFloat("Look Max Pitch", &lookTargetMaxPitchDegrees, 0.5f, 0.0f, 90.0f);
    if (ImGui::Button("Look Target Now"))
    {
        SetTargetObjectLookMode(true);
    }

    if (ImGui::Button("Register Actions"))
    {
        RegisterActions();
    }

    if (ImGui::Button("Test Sweep -30 to 30"))
    {
        SweepYaw(
            actionSweepStartYawDegrees,
            actionSweepEndYawDegrees,
            actionDuration,
            actionBlendInTime,
            actionBlendOutTime);
    }

    ImGui::SameLine();

    if (ImGui::Button("Clear"))
    {
        ClearCommand();
    }
}

void DragonNeckCommandIKComponent::Serialize(nlohmann::json& json_) const
{
    json_["ChainName"] = chainName;
    json_["MouthNodeName"] = mouthNodeName;
    json_["LocalForwardAxis"] = {
        localForwardAxis.x,
        localForwardAxis.y,
        localForwardAxis.z
    };
    json_["ActionCategory"] = actionCategory;
    json_["RegisterGlobalActions"] = registerGlobalActions;
    json_["ActionDuration"] = actionDuration;
    json_["ActionBlendInTime"] = actionBlendInTime;
    json_["ActionBlendOutTime"] = actionBlendOutTime;
    json_["ActionYawDegrees"] = actionYawDegrees;
    json_["ActionSweepStartYawDegrees"] = actionSweepStartYawDegrees;
    json_["ActionSweepEndYawDegrees"] = actionSweepEndYawDegrees;
    json_["ActionWorldTargetPosition"] = {
        actionWorldTargetPosition.x,
        actionWorldTargetPosition.y,
        actionWorldTargetPosition.z
    };
    json_["LookTargetObjectMode"] = lookTargetObjectMode;
    json_["LookTargetObjectName"] = lookTargetObjectName;
    json_["LookTargetObjectOffset"] = {
        lookTargetObjectOffset.x,
        lookTargetObjectOffset.y,
        lookTargetObjectOffset.z
    };
    json_["LookTargetBlendInTime"] = lookTargetBlendInTime;
    json_["LookTargetYawOffsetDegrees"] = lookTargetYawOffsetDegrees;
    json_["LookTargetPitchOffsetDegrees"] = lookTargetPitchOffsetDegrees;
    json_["LookTargetMaxYawDegrees"] = lookTargetMaxYawDegrees;
    json_["LookTargetMaxPitchDegrees"] = lookTargetMaxPitchDegrees;
}

void DragonNeckCommandIKComponent::Deserialize(const nlohmann::json& json_)
{
    chainName = json_.value("ChainName", std::string("DragonNeck"));
    mouthNodeName = json_.value("MouthNodeName", std::string("Mouth"));

    if (json_.contains("LocalForwardAxis"))
    {
        localForwardAxis = {
            json_["LocalForwardAxis"][0],
            json_["LocalForwardAxis"][1],
            json_["LocalForwardAxis"][2]
        };
    }

    actionCategory = json_.value("ActionCategory", std::string("DragonIK"));
    registerGlobalActions = json_.value("RegisterGlobalActions", false);
    actionDuration = json_.value("ActionDuration", 1.0f);
    actionBlendInTime = json_.value("ActionBlendInTime", 0.15f);
    actionBlendOutTime = json_.value("ActionBlendOutTime", 0.2f);
    actionYawDegrees = json_.value("ActionYawDegrees", 30.0f);
    actionSweepStartYawDegrees = json_.value("ActionSweepStartYawDegrees", -30.0f);
    actionSweepEndYawDegrees = json_.value("ActionSweepEndYawDegrees", 30.0f);

    if (json_.contains("ActionWorldTargetPosition"))
    {
        actionWorldTargetPosition = {
            json_["ActionWorldTargetPosition"][0],
            json_["ActionWorldTargetPosition"][1],
            json_["ActionWorldTargetPosition"][2]
        };
    }
    lookTargetObjectMode = json_.value("LookTargetObjectMode", false);
    lookTargetObjectName = json_.value("LookTargetObjectName", std::string("PlayerModel"));
    if (json_.contains("LookTargetObjectOffset"))
    {
        lookTargetObjectOffset = {
            json_["LookTargetObjectOffset"][0],
            json_["LookTargetObjectOffset"][1],
            json_["LookTargetObjectOffset"][2]
        };
    }
    lookTargetBlendInTime = json_.value("LookTargetBlendInTime", 0.15f);
    lookTargetYawOffsetDegrees = json_.value("LookTargetYawOffsetDegrees", 0.0f);
    lookTargetPitchOffsetDegrees = json_.value("LookTargetPitchOffsetDegrees", 0.0f);
    lookTargetMaxYawDegrees = json_.value("LookTargetMaxYawDegrees", 65.0f);
    lookTargetMaxPitchDegrees = json_.value("LookTargetMaxPitchDegrees", 35.0f);

    RegisterActions();
    if (lookTargetObjectMode)
    {
        SetTargetObjectLookMode(true);
    }
}

REGISTER_COMPONENT(DragonNeckCommandIKComponent, "DragonNeckCommandIKComponent")
