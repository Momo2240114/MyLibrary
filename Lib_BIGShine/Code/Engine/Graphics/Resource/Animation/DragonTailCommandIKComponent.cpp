#include "DragonTailCommandIKComponent.h"

#include "IKMapComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/Graphics/Resource/GlTFModel.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace
{
    constexpr float DEG_TO_RAD = 3.1415926535f / 180.0f;
    constexpr const char* ACTION_FOLLOW_BODY = "DragonTail_FollowBody";
    constexpr const char* ACTION_CLEAR = "DragonTail_Clear";
    constexpr const char* ACTION_SWEEP_LEFT = "DragonTail_SweepLeft";
    constexpr const char* ACTION_SWEEP_RIGHT = "DragonTail_SweepRight";
    constexpr const char* ACTION_SPIN_SWEEP = "DragonTail_SpinSweep";

    DirectX::XMFLOAT3 GetTranslation(const DirectX::XMFLOAT4X4& matrix_)
    {
        return { matrix_._41, matrix_._42, matrix_._43 };
    }

    float SmoothStep(float value_)
    {
        const float clampedValue_ = std::clamp(value_, 0.0f, 1.0f);
        return clampedValue_ * clampedValue_ * (3.0f - 2.0f * clampedValue_);
    }
}

const std::string& DragonTailCommandIKComponent::GetName() const
{
    static std::string name_ = "DragonTailCommandIKComponent";
    return name_;
}

void DragonTailCommandIKComponent::Start()
{
    const auto owner_ = GetGameObject();
    model = owner_ ? owner_->GetComponent<GltfModelCommponennt>().get() : nullptr;
    ikMap = owner_ ? owner_->GetComponent<IKMapComponent>().get() : nullptr;

    if (ikMap)
    {
        ikMap->ResolveChainByName(chainName);
    }

    RegisterActions();
}

void DragonTailCommandIKComponent::LateUpdate(float deltaTime_)
{
    if (!model || !ikMap) return;

    commandTimer += deltaTime_;
    ApplyTailIK(deltaTime_);

    const float commandRate_ = CalculateCommandRate();
    attackActive =
        currentCommand.mode == DRAGON_TAIL_IK_MODE::SWEEP_LEFT ||
        currentCommand.mode == DRAGON_TAIL_IK_MODE::SWEEP_RIGHT ||
        currentCommand.mode == DRAGON_TAIL_IK_MODE::SPIN_SWEEP;

    attackActive =
        attackActive &&
        commandRate_ >= currentCommand.attackStartRate &&
        commandRate_ <= currentCommand.attackEndRate;

    const float totalTime_ =
        currentCommand.blendInTime +
        currentCommand.duration +
        currentCommand.blendOutTime;

    if (currentCommand.mode != DRAGON_TAIL_IK_MODE::FOLLOW_BODY &&
        currentCommand.mode != DRAGON_TAIL_IK_MODE::DISABLED &&
        commandTimer >= totalTime_)
    {
        SetFollowBody();
    }
}

void DragonTailCommandIKComponent::SetFollowBody()
{
    currentCommand = {};
    currentCommand.mode = DRAGON_TAIL_IK_MODE::FOLLOW_BODY;
    commandTimer = 0.0f;
    attackActive = false;
}

void DragonTailCommandIKComponent::UnregisterActions()
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

void DragonTailCommandIKComponent::RegisterActions()
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

    registerAction_(ACTION_FOLLOW_BODY, [this]()
        {
            SetFollowBody();
        });
    registerAction_(ACTION_CLEAR, [this]()
        {
            ClearCommand();
        });
    registerAction_(ACTION_SWEEP_LEFT, [this]()
        {
            StartSweepLeft(actionDuration);
        });
    registerAction_(ACTION_SWEEP_RIGHT, [this]()
        {
            StartSweepRight(actionDuration);
        });
    registerAction_(ACTION_SPIN_SWEEP, [this]()
        {
            StartSpinSweep(spinActionDuration);
        });
}

void DragonTailCommandIKComponent::ClearCommand()
{
    currentCommand = {};
    currentCommand.mode = DRAGON_TAIL_IK_MODE::DISABLED;
    commandTimer = 0.0f;
    attackActive = false;
}

void DragonTailCommandIKComponent::AimToTarget(
    const std::string& targetName_,
    float duration_,
    float blendInTime_,
    float blendOutTime_)
{
    currentCommand = {};
    currentCommand.mode = DRAGON_TAIL_IK_MODE::AIM_TO_TARGET;
    currentCommand.targetName = targetName_;
    currentCommand.duration = duration_;
    currentCommand.blendInTime = blendInTime_;
    currentCommand.blendOutTime = blendOutTime_;
    commandTimer = 0.0f;
}

void DragonTailCommandIKComponent::StartSweepLeft(float duration_)
{
    currentCommand = {};
    currentCommand.mode = DRAGON_TAIL_IK_MODE::SWEEP_LEFT;
    currentCommand.duration = duration_;
    currentCommand.sweepAngleDegrees = 70.0f;
    commandTimer = 0.0f;
}

void DragonTailCommandIKComponent::StartSweepRight(float duration_)
{
    currentCommand = {};
    currentCommand.mode = DRAGON_TAIL_IK_MODE::SWEEP_RIGHT;
    currentCommand.duration = duration_;
    currentCommand.sweepAngleDegrees = 70.0f;
    commandTimer = 0.0f;
}

void DragonTailCommandIKComponent::StartSpinSweep(float duration_)
{
    currentCommand = {};
    currentCommand.mode = DRAGON_TAIL_IK_MODE::SPIN_SWEEP;
    currentCommand.duration = duration_;
    currentCommand.sweepAngleDegrees = 120.0f;
    currentCommand.liftAngleDegrees = 12.0f;
    currentCommand.expandAngleDegrees = 20.0f;
    commandTimer = 0.0f;
}

float DragonTailCommandIKComponent::CalculateCommandRate() const
{
    if (currentCommand.duration <= 0.0001f) return 1.0f;

    return std::clamp(
        (commandTimer - currentCommand.blendInTime) / currentCommand.duration,
        0.0f,
        1.0f);
}

float DragonTailCommandIKComponent::CalculateBlendRate() const
{
    if (currentCommand.mode == DRAGON_TAIL_IK_MODE::DISABLED) return 0.0f;

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

bool DragonTailCommandIKComponent::ApplyTailIK(float deltaTime_)
{
    (void)deltaTime_;

    auto* chain_ = ikMap->FindChain(chainName);
    if (!chain_ || !chain_->enabled) return false;
    if (!ikMap->ResolveChainByName(chainName)) return false;
    if (chain_->nodeIndices.size() < 2) return false;

    if (currentCommand.mode == DRAGON_TAIL_IK_MODE::DISABLED) return false;

    auto& nodes_ = model->GetNodes();

    const float blendRate_ = CalculateBlendRate() * std::clamp(chain_->influence, 0.0f, 1.0f);
    if (blendRate_ <= 0.0001f) return false;

    const float commandRate_ = CalculateCommandRate();
    const float swingRate_ = std::sin(commandRate_ * 3.1415926535f);

    float yawDegrees_ = 0.0f;
    float pitchDegrees_ = 0.0f;
    float rollDegrees_ = 0.0f;

    switch (currentCommand.mode)
    {
    case DRAGON_TAIL_IK_MODE::FOLLOW_BODY:
        yawDegrees_ = 0.0f;
        pitchDegrees_ = 0.0f;
        rollDegrees_ = 0.0f;
        break;

    case DRAGON_TAIL_IK_MODE::SWEEP_LEFT:
        yawDegrees_ = -currentCommand.sweepAngleDegrees * swingRate_;
        pitchDegrees_ = currentCommand.liftAngleDegrees * swingRate_;
        break;

    case DRAGON_TAIL_IK_MODE::SWEEP_RIGHT:
        yawDegrees_ = currentCommand.sweepAngleDegrees * swingRate_;
        pitchDegrees_ = currentCommand.liftAngleDegrees * swingRate_;
        break;

    case DRAGON_TAIL_IK_MODE::SPIN_SWEEP:
        yawDegrees_ = currentCommand.sweepAngleDegrees * std::sin(commandRate_ * 6.283185307f);
        pitchDegrees_ = currentCommand.liftAngleDegrees * swingRate_;
        rollDegrees_ = currentCommand.expandAngleDegrees * swingRate_;
        break;

    case DRAGON_TAIL_IK_MODE::AIM_TO_TARGET:
        // TODO: Aim the tail tip toward a target behind the body.
        // Sweep attacks are enough for the first command pass.
        break;

    default:
        break;
    }

    const size_t boneCount_ = chain_->nodeIndices.size();
    for (size_t boneIndex_ = 0; boneIndex_ < boneCount_; ++boneIndex_)
    {
        const int nodeIndex_ = chain_->nodeIndices[boneIndex_];
        auto& node_ = nodes_.at(nodeIndex_);

        const float boneRate_ = boneCount_ <= 1
            ? 1.0f
            : static_cast<float>(boneIndex_) / static_cast<float>(boneCount_ - 1);

        const float tipBias_ = boneRate_ * boneRate_;
        const float weight_ =
            currentCommand.rootWeight +
            (currentCommand.tipWeight - currentCommand.rootWeight) * tipBias_;

        const float yawRadians_ = yawDegrees_ * DEG_TO_RAD * weight_ * blendRate_;
        const float pitchRadians_ = pitchDegrees_ * DEG_TO_RAD * weight_ * blendRate_;
        const float rollRadians_ = rollDegrees_ * DEG_TO_RAD * weight_ * blendRate_;

        const DirectX::XMVECTOR localRotation_ = DirectX::XMLoadFloat4(&node_.rotation);

        const DirectX::XMVECTOR yawRotation_ = DirectX::XMQuaternionRotationAxis(
            DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
            yawRadians_);

        const DirectX::XMVECTOR pitchRotation_ = DirectX::XMQuaternionRotationAxis(
            DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),
            pitchRadians_);

        const DirectX::XMVECTOR rollRotation_ = DirectX::XMQuaternionRotationAxis(
            DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
            rollRadians_);

        const DirectX::XMVECTOR commandRotation_ = DirectX::XMQuaternionMultiply(
            rollRotation_,
            DirectX::XMQuaternionMultiply(pitchRotation_, yawRotation_));

        const DirectX::XMVECTOR resultRotation_ = DirectX::XMQuaternionNormalize(
            DirectX::XMQuaternionMultiply(localRotation_, commandRotation_));

        DirectX::XMStoreFloat4(&node_.rotation, resultRotation_);
        model->UpdateNodeTransformOnly(nodeIndex_);
    }

    model->UpdateNodeTransformFrom(chain_->nodeIndices.front());
    return true;
}

bool DragonTailCommandIKComponent::GetTailTipWorldPosition(DirectX::XMFLOAT3& position_) const
{
    if (!model || !ikMap) return false;

    const auto* chain_ = ikMap->FindChain(chainName);
    if (!chain_ || chain_->nodeIndices.empty()) return false;

    const int tipNodeIndex_ = chain_->nodeIndices.back();
    const auto& nodes_ = model->GetNodes();

    if (tipNodeIndex_ < 0 || tipNodeIndex_ >= static_cast<int>(nodes_.size())) return false;

    position_ = GetTranslation(nodes_.at(tipNodeIndex_).worldTransform);
    return true;
}

bool DragonTailCommandIKComponent::GetTailTipWorldForward(DirectX::XMFLOAT3& forward_) const
{
    if (!model || !ikMap) return false;

    const auto* chain_ = ikMap->FindChain(chainName);
    if (!chain_ || chain_->nodeIndices.empty()) return false;

    const int tipNodeIndex_ = chain_->nodeIndices.back();
    const auto& nodes_ = model->GetNodes();

    if (tipNodeIndex_ < 0 || tipNodeIndex_ >= static_cast<int>(nodes_.size())) return false;

    const DirectX::XMMATRIX worldMatrix_ = DirectX::XMLoadFloat4x4(&nodes_.at(tipNodeIndex_).worldTransform);
    DirectX::XMStoreFloat3(
        &forward_,
        DirectX::XMVector3Normalize(
            DirectX::XMVector3TransformNormal(
                DirectX::XMLoadFloat3(&localForwardAxis),
                worldMatrix_)));

    return true;
}

void DragonTailCommandIKComponent::Gui()
{
    ImGui::TextDisabled("AI command driven tail IK.");

    char chainNameBuffer_[64];
    strncpy_s(chainNameBuffer_, chainName.c_str(), sizeof(chainNameBuffer_));
    if (ImGui::InputText("Chain Name", chainNameBuffer_, sizeof(chainNameBuffer_)))
    {
        chainName = chainNameBuffer_;
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

    ImGui::DragFloat("Sweep Action Duration", &actionDuration, 0.01f, 0.0f, 30.0f);
    ImGui::DragFloat("Spin Action Duration", &spinActionDuration, 0.01f, 0.0f, 30.0f);

    if (ImGui::Button("Register Actions"))
    {
        RegisterActions();
    }

    if (ImGui::Button("Sweep Left"))
    {
        StartSweepLeft(actionDuration);
    }

    ImGui::SameLine();

    if (ImGui::Button("Sweep Right"))
    {
        StartSweepRight(actionDuration);
    }

    ImGui::SameLine();

    if (ImGui::Button("Spin Sweep"))
    {
        StartSpinSweep(spinActionDuration);
    }

    ImGui::Text("Attack Active: %s", attackActive ? "true" : "false");
}

void DragonTailCommandIKComponent::Serialize(nlohmann::json& json_) const
{
    json_["ChainName"] = chainName;
    json_["LocalForwardAxis"] = {
        localForwardAxis.x,
        localForwardAxis.y,
        localForwardAxis.z
    };
    json_["ActionCategory"] = actionCategory;
    json_["RegisterGlobalActions"] = registerGlobalActions;
    json_["ActionDuration"] = actionDuration;
    json_["SpinActionDuration"] = spinActionDuration;
}

void DragonTailCommandIKComponent::Deserialize(const nlohmann::json& json_)
{
    chainName = json_.value("ChainName", std::string("DragonTail"));

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
    actionDuration = json_.value("ActionDuration", 0.8f);
    spinActionDuration = json_.value("SpinActionDuration", 1.6f);

    RegisterActions();
}

REGISTER_COMPONENT(DragonTailCommandIKComponent, "DragonTailCommandIKComponent")
