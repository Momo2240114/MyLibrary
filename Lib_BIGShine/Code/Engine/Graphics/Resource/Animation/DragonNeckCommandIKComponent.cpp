#include "DragonNeckCommandIKComponent.h"

#include "IKMapComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/Graphics/Resource/GlTFModel.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace
{
    constexpr float DEG_TO_RAD = 3.1415926535f / 180.0f;
    constexpr float RAD_TO_DEG = 180.0f / 3.1415926535f;

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

    DirectX::XMFLOAT3 Subtract(const DirectX::XMFLOAT3& a_, const DirectX::XMFLOAT3& b_)
    {
        return { a_.x - b_.x, a_.y - b_.y, a_.z - b_.z };
    }

    float SmoothStep(float value_)
    {
        const float clampedValue_ = std::clamp(value_, 0.0f, 1.0f);
        return clampedValue_ * clampedValue_ * (3.0f - 2.0f * clampedValue_);
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
}

void DragonNeckCommandIKComponent::LateUpdate(float deltaTime_)
{
    if (!model || !ikMap) return;

    commandTimer += deltaTime_;
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
        // TODO: Resolve by name through GameObjectManager.
        // auto target_ = GetGameObject()->GetManager().Find(currentCommand.targetName);
        // if (!target_ || !target_->transform) return false;
        // targetPosition_ = target_->transform->GetWorldPosition();

        return false;
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
        const DirectX::XMFLOAT3 rootPosition_ = GetTranslation(rootNode_.worldTransform);
        const DirectX::XMFLOAT3 toTarget_ = Normalize(Subtract(targetPosition_, rootPosition_));

        if (toTarget_.x == 0.0f && toTarget_.y == 0.0f && toTarget_.z == 0.0f) return false;

        const float yawRadians_ = std::atan2(toTarget_.x, toTarget_.z);
        const float horizontalLength_ = std::sqrt(toTarget_.x * toTarget_.x + toTarget_.z * toTarget_.z);
        const float pitchRadians_ = std::atan2(toTarget_.y, horizontalLength_);

        targetYawDegrees_ = yawRadians_ * RAD_TO_DEG;
        targetPitchDegrees_ = pitchRadians_ * RAD_TO_DEG;
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

    if (ImGui::Button("Test Sweep -30 to 30"))
    {
        SweepYaw(-30.0f, 30.0f, 1.2f);
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
}

REGISTER_COMPONENT(DragonNeckCommandIKComponent, "DragonNeckCommandIKComponent")
