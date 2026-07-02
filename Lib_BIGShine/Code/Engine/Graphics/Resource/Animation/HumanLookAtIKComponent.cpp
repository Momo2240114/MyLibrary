#include "HumanLookAtIKComponent.h"

#include "AnimationControllerComponent.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/GlTFModel.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <imgui.h>

using namespace DirectX;

namespace
{
    constexpr float EPSILON = 0.00001f;

    XMFLOAT3 GetTranslation(const XMFLOAT4X4& matrix_)
    {
        return { matrix_._41, matrix_._42, matrix_._43 };
    }

    XMFLOAT3 Add(const XMFLOAT3& lhs_, const XMFLOAT3& rhs_)
    {
        return { lhs_.x + rhs_.x, lhs_.y + rhs_.y, lhs_.z + rhs_.z };
    }

    XMFLOAT3 Subtract(const XMFLOAT3& lhs_, const XMFLOAT3& rhs_)
    {
        return { lhs_.x - rhs_.x, lhs_.y - rhs_.y, lhs_.z - rhs_.z };
    }

    XMFLOAT3 Scale(const XMFLOAT3& value_, float scale_)
    {
        return { value_.x * scale_, value_.y * scale_, value_.z * scale_ };
    }

    float Length(const XMFLOAT3& value_)
    {
        return std::sqrt(value_.x * value_.x + value_.y * value_.y + value_.z * value_.z);
    }

    XMFLOAT3 Normalize(const XMFLOAT3& value_)
    {
        const float length_ = Length(value_);
        return length_ > EPSILON ? Scale(value_, 1.0f / length_) : XMFLOAT3{};
    }

    XMFLOAT3 Lerp(const XMFLOAT3& from_, const XMFLOAT3& to_, float amount_)
    {
        return Add(from_, Scale(Subtract(to_, from_), std::clamp(amount_, 0.0f, 1.0f)));
    }

    void DrawBoneBetween(
        ShapeRenderSystem& renderer_,
        const XMFLOAT3& startPosition_,
        const XMFLOAT3& endPosition_,
        const XMFLOAT4& color_)
    {
        const XMFLOAT3 difference_ = Subtract(endPosition_, startPosition_);
        const float length_ = Length(difference_);
        if (length_ <= EPSILON) return;

        const XMVECTOR forward_ = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        const XMVECTOR direction_ = XMVector3Normalize(XMLoadFloat3(&difference_));
        const float dot_ = std::clamp(
            XMVectorGetX(XMVector3Dot(forward_, direction_)),
            -1.0f,
            1.0f);
        XMMATRIX transform_ = XMMatrixIdentity();
        if (dot_ < 1.0f - EPSILON)
        {
            XMVECTOR axis_ = XMVector3Cross(forward_, direction_);
            if (XMVectorGetX(XMVector3LengthSq(axis_)) <= EPSILON)
            {
                axis_ = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            }
            transform_ = XMMatrixRotationAxis(XMVector3Normalize(axis_), std::acos(dot_));
        }
        transform_.r[3] = XMVectorSet(
            startPosition_.x,
            startPosition_.y,
            startPosition_.z,
            1.0f);
        XMFLOAT4X4 transformStorage_;
        XMStoreFloat4x4(&transformStorage_, transform_);
        renderer_.DrawBone(transformStorage_, length_, color_);
    }
}

const std::string& HumanLookAtIKComponent::GetName() const
{
    static const std::string name_ = "HumanLookAtIKComponent";
    return name_;
}

void HumanLookAtIKComponent::Start()
{
    if (!ResolveDependencies()) return;
    ikMap->AddLookAtChain("LookAt");
    ResolveTarget();
}

void HumanLookAtIKComponent::SetTargetName(const std::string& targetName_)
{
    targetName = targetName_;
    target.reset();
}

bool HumanLookAtIKComponent::ResolveDependencies()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return false;
    if (!ikMap) ikMap = owner_->GetComponent<IKMapComponent>().get();
    if (!model) model = owner_->GetComponent<GltfModelCommponennt>().get();
    if (!animationController)
    {
        animationController = owner_->GetComponent<AnimationControllerComponent>().get();
    }
    return ikMap && model;
}

std::shared_ptr<GameObject> HumanLookAtIKComponent::ResolveTarget()
{
    if (const std::shared_ptr<GameObject> currentTarget_ = target.lock()) return currentTarget_;
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->manager || targetName.empty()) return nullptr;
    const std::shared_ptr<GameObject> resolvedTarget_ = owner_->manager->Find(targetName);
    target = resolvedTarget_;
    return resolvedTarget_;
}

bool HumanLookAtIKComponent::IsAllowedAnimationState() const
{
    if (!idleOnly) return true;
    if (!animationController) return false;
    const AnimState* currentState_ = animationController->GetCurrentState();
    return currentState_ && currentState_->name == idleStateName;
}

void HumanLookAtIKComponent::LateUpdate(float deltaTime_)
{
    BS_PROFILE_SCOPE("HumanLookAtIKComponent::LateUpdate");
    if (!ResolveDependencies()) return;

    std::vector<IKMapComponent::IKChain>& chains_ = ikMap->GetChains();
    if (lookStates.size() != chains_.size()) lookStates.resize(chains_.size());

    const std::shared_ptr<GameObject> target_ = ResolveTarget();
    XMFLOAT3 targetPosition_{};
    if (target_ && target_->transform)
    {
        XMFLOAT4X4 targetWorld_;
        XMStoreFloat4x4(&targetWorld_, target_->transform->GetWorldMatrix());
        targetPosition_ = Add(GetTranslation(targetWorld_), targetOffset);
    }
    const bool shouldLook_ = lookEnabled && target_ && IsAllowedAnimationState();

    for (size_t chainIndex_ = 0; chainIndex_ < chains_.size(); ++chainIndex_)
    {
        IKMapComponent::IKChain& chain_ = chains_[chainIndex_];
        if (!chain_.enabled || chain_.type != IKMapComponent::IK_TYPE::LOOK_AT) continue;
        if (!ResolveChain(chain_)) continue;

        LookRuntimeState& state_ = lookStates[chainIndex_];
        const float safeDeltaTime_ = std::clamp(deltaTime_, 0.0f, 0.1f);
        const float blend_ = 1.0f - std::exp(
            -(std::max)(chain_.lookBlendSpeed, 0.0f) * safeDeltaTime_);
        const float desiredWeight_ = shouldLook_ ? 1.0f : 0.0f;
        state_.activeWeight += (desiredWeight_ - state_.activeWeight) * blend_;
        if (state_.activeWeight <= EPSILON || !target_) continue;

        ApplyLookChain(chain_, state_, targetPosition_, safeDeltaTime_);
        DrawDebugShapes(chain_, targetPosition_);
    }
}

bool HumanLookAtIKComponent::ResolveChain(IKMapComponent::IKChain& chain_)
{
    if (chain_.rootNodeName.empty() || chain_.tipNodeName.empty()) return false;
    if (!chain_.nodeIndices.empty()) return chain_.nodeIndices.size() >= 2;
    const int rootIndex_ = model->GetNodeIndex(chain_.rootNodeName.c_str());
    const int tipIndex_ = model->GetNodeIndex(chain_.tipNodeName.c_str());
    if (rootIndex_ < 0 || tipIndex_ < 0) return false;

    const std::vector<GltfModelCommponennt::Node>& nodes_ = model->GetNodes();
    std::vector<int> reverseIndices_;
    for (int currentIndex_ = tipIndex_; currentIndex_ >= 0;
        currentIndex_ = nodes_.at(currentIndex_).parentIndex)
    {
        reverseIndices_.push_back(currentIndex_);
        if (currentIndex_ == rootIndex_) break;
    }
    if (reverseIndices_.empty() || reverseIndices_.back() != rootIndex_) return false;
    chain_.nodeIndices.assign(reverseIndices_.rbegin(), reverseIndices_.rend());
    return chain_.nodeIndices.size() >= 2;
}

void HumanLookAtIKComponent::ApplyLookChain(
    IKMapComponent::IKChain& chain_,
    LookRuntimeState& state_,
    const XMFLOAT3& targetPosition_,
    float deltaTime_)
{
    (void)deltaTime_;
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->transform) return;

    const XMMATRIX ownerWorld_ = owner_->transform->GetWorldMatrix();
    const XMVECTOR ownerRight_ = XMVector3Normalize(ownerWorld_.r[0]);
    const XMVECTOR ownerUp_ = XMVector3Normalize(ownerWorld_.r[1]);
    const XMVECTOR ownerForward_ = XMVector3Normalize(ownerWorld_.r[2]);
    std::vector<GltfModelCommponennt::Node>& nodes_ = model->GetNodes();

    // モデルごとにボーン軸が異なるため、現在のキャラクター正面を各ボーンの
    // ローカル空間へ戻し、そのボーン固有の正面軸として初回だけ保持する。
    if (state_.localForwardAxes.size() != chain_.nodeIndices.size())
    {
        state_.localForwardAxes.clear();
        state_.localForwardAxes.reserve(chain_.nodeIndices.size());
        for (const int nodeIndex_ : chain_.nodeIndices)
        {
            const XMMATRIX nodeWorld_ = XMLoadFloat4x4(&nodes_.at(nodeIndex_).worldTransform);
            const XMVECTOR localForward_ = XMVector3Normalize(
                XMVector3TransformNormal(ownerForward_, XMMatrixInverse(nullptr, nodeWorld_)));
            XMFLOAT3 localForwardStorage_;
            XMStoreFloat3(&localForwardStorage_, localForward_);
            state_.localForwardAxes.push_back(localForwardStorage_);
        }
    }

    for (size_t pointIndex_ = 0; pointIndex_ < chain_.nodeIndices.size(); ++pointIndex_)
    {
        GltfModelCommponennt::Node& node_ = nodes_.at(chain_.nodeIndices[pointIndex_]);
        const XMFLOAT3 nodePosition_ = GetTranslation(node_.worldTransform);
        const XMFLOAT3 targetDifference_ = Normalize(Subtract(targetPosition_, nodePosition_));
        if (Length(targetDifference_) <= EPSILON) continue;

        const XMVECTOR targetDirection_ = XMLoadFloat3(&targetDifference_);
        const float forwardAmount_ = XMVectorGetX(XMVector3Dot(targetDirection_, ownerForward_));
        const float rightAmount_ = XMVectorGetX(XMVector3Dot(targetDirection_, ownerRight_));
        const float upAmount_ = std::clamp(
            XMVectorGetX(XMVector3Dot(targetDirection_, ownerUp_)),
            -1.0f,
            1.0f);
        float yaw_ = std::atan2(rightAmount_, forwardAmount_);
        float pitch_ = std::asin(upAmount_);
        yaw_ = std::clamp(
            yaw_,
            -XMConvertToRadians((std::max)(chain_.lookMaxYawDegrees, 0.0f)),
            XMConvertToRadians((std::max)(chain_.lookMaxYawDegrees, 0.0f)));
        pitch_ = std::clamp(
            pitch_,
            -XMConvertToRadians((std::max)(chain_.lookMaxPitchDegrees, 0.0f)),
            XMConvertToRadians((std::max)(chain_.lookMaxPitchDegrees, 0.0f)));

        const float normalizedPoint_ = chain_.nodeIndices.size() <= 1
            ? 1.0f
            : static_cast<float>(pointIndex_)
                / static_cast<float>(chain_.nodeIndices.size() - 1);
        const float shieldBias_ = XMConvertToRadians(chain_.lookShieldShoulderYawDegrees)
            * (1.0f - normalizedPoint_);
        const float biasedYaw_ = yaw_ + shieldBias_;
        const float cosPitch_ = std::cos(pitch_);
        const XMVECTOR desiredDirection_ = XMVector3Normalize(
            XMVectorAdd(
                XMVectorAdd(
                    XMVectorScale(ownerForward_, std::cos(biasedYaw_) * cosPitch_),
                    XMVectorScale(ownerRight_, std::sin(biasedYaw_) * cosPitch_)),
                XMVectorScale(ownerUp_, std::sin(pitch_))));

        XMFLOAT3 localForwardAxis_ = Normalize(state_.localForwardAxes[pointIndex_]);
        if (Length(localForwardAxis_) <= EPSILON)
        {
            localForwardAxis_ = Normalize(chain_.lookForwardAxis);
        }
        if (Length(localForwardAxis_) <= EPSILON) localForwardAxis_ = { 0.0f, 0.0f, 1.0f };
        const XMMATRIX nodeWorld_ = XMLoadFloat4x4(&node_.worldTransform);
        const XMVECTOR currentForward_ = XMVector3Normalize(
            XMVector3TransformNormal(XMLoadFloat3(&localForwardAxis_), nodeWorld_));
        XMVECTOR axis_ = XMVector3Cross(currentForward_, desiredDirection_);
        if (XMVectorGetX(XMVector3LengthSq(axis_)) <= EPSILON) continue;
        axis_ = XMVector3Normalize(axis_);

        const float dot_ = std::clamp(
            XMVectorGetX(XMVector3Dot(currentForward_, desiredDirection_)),
            -1.0f,
            1.0f);
        float chainWeight_ = chain_.lookRootWeight
            + (chain_.lookTipWeight - chain_.lookRootWeight) * normalizedPoint_;
        const size_t remainingNodeCount_ = chain_.nodeIndices.size() - pointIndex_;
        if (remainingNodeCount_ == 2)
        {
            // 首は胴体より強く追従させ、視線方向へ十分に曲げる。
            chainWeight_ = (std::max)(chainWeight_, 0.85f);
        }
        else if (remainingNodeCount_ == 1)
        {
            // 頭は親までで残った角度をすべて解消してターゲットへ正対させる。
            chainWeight_ = 1.0f;
        }
        const float angle_ = std::acos(dot_)
            * std::clamp(chainWeight_, 0.0f, 1.0f)
            * std::clamp(chain_.influence, 0.0f, 1.0f)
            * state_.activeWeight;

        XMMATRIX nodeWorldRotation_ = nodeWorld_;
        nodeWorldRotation_.r[3] = XMVectorSet(0, 0, 0, 1);
        XMMATRIX parentWorldRotation_ = ownerWorld_;
        if (node_.parent)
        {
            parentWorldRotation_ = XMLoadFloat4x4(&node_.parent->worldTransform);
        }
        parentWorldRotation_.r[3] = XMVectorSet(0, 0, 0, 1);
        const XMMATRIX desiredWorldRotation_ = nodeWorldRotation_
            * XMMatrixRotationAxis(axis_, angle_);
        const XMMATRIX desiredLocalRotation_ = desiredWorldRotation_
            * XMMatrixInverse(nullptr, parentWorldRotation_);
        XMVECTOR scale_;
        XMVECTOR rotation_;
        XMVECTOR translation_;
        if (XMMatrixDecompose(&scale_, &rotation_, &translation_, desiredLocalRotation_))
        {
            XMStoreFloat4(&node_.rotation, XMQuaternionNormalize(rotation_));
            model->UpdateNodeTransformFrom(chain_.nodeIndices[pointIndex_]);
        }
    }
}

void HumanLookAtIKComponent::DrawDebugShapes(
    const IKMapComponent::IKChain& chain_,
    const XMFLOAT3& targetPosition_) const
{
    if (!showDebugShapes || chain_.nodeIndices.size() < 2) return;
    ShapeRenderSystem& renderer_ = GraphicsDeviceManager::Instance().GetShapeRendererSystem();
    const std::vector<GltfModelCommponennt::Node>& nodes_ = model->GetNodes();
    for (size_t pointIndex_ = 0; pointIndex_ + 1 < chain_.nodeIndices.size(); ++pointIndex_)
    {
        DrawBoneBetween(
            renderer_,
            GetTranslation(nodes_.at(chain_.nodeIndices[pointIndex_]).worldTransform),
            GetTranslation(nodes_.at(chain_.nodeIndices[pointIndex_ + 1]).worldTransform),
            debugChainColor);
    }
    DrawBoneBetween(
        renderer_,
        GetTranslation(nodes_.at(chain_.nodeIndices.back()).worldTransform),
        targetPosition_,
        debugTargetColor);
}

void HumanLookAtIKComponent::Gui()
{
    ImGui::Checkbox("Look Enabled", &lookEnabled);
    ImGui::Checkbox("Idle Only", &idleOnly);
    ImGui::Checkbox("Show Debug Shapes", &showDebugShapes);
    char targetNameBuffer_[128]{};
    strncpy_s(targetNameBuffer_, targetName.c_str(), sizeof(targetNameBuffer_));
    if (ImGui::InputText("Target Object", targetNameBuffer_, sizeof(targetNameBuffer_)))
    {
        SetTargetName(targetNameBuffer_);
    }
    char idleStateBuffer_[128]{};
    strncpy_s(idleStateBuffer_, idleStateName.c_str(), sizeof(idleStateBuffer_));
    if (ImGui::InputText("Idle State", idleStateBuffer_, sizeof(idleStateBuffer_)))
    {
        idleStateName = idleStateBuffer_;
    }
    ImGui::DragFloat3("Target Offset", &targetOffset.x, 0.01f);
    if (showDebugShapes)
    {
        ImGui::ColorEdit4("Look Chain Color", &debugChainColor.x);
        ImGui::ColorEdit4("Look Target Color", &debugTargetColor.x);
    }
    ImGui::TextDisabled("Look chain bones and limits are configured in IKMapComponent.");
}

void HumanLookAtIKComponent::Serialize(nlohmann::json& json_) const
{
    json_["LookEnabled"] = lookEnabled;
    json_["IdleOnly"] = idleOnly;
    json_["IdleStateName"] = idleStateName;
    json_["TargetName"] = targetName;
    json_["TargetOffset"] = { targetOffset.x, targetOffset.y, targetOffset.z };
    json_["ShowDebugShapes"] = showDebugShapes;
    json_["DebugChainColor"] = { debugChainColor.x, debugChainColor.y, debugChainColor.z, debugChainColor.w };
    json_["DebugTargetColor"] = { debugTargetColor.x, debugTargetColor.y, debugTargetColor.z, debugTargetColor.w };
}

void HumanLookAtIKComponent::Deserialize(const nlohmann::json& json_)
{
    lookEnabled = json_.value("LookEnabled", true);
    idleOnly = json_.value("IdleOnly", true);
    idleStateName = json_.value("IdleStateName", std::string("SS_Idle"));
    targetName = json_.value("TargetName", std::string("TestTarget"));
    if (json_.contains("TargetOffset"))
    {
        targetOffset = {
            json_["TargetOffset"][0],
            json_["TargetOffset"][1],
            json_["TargetOffset"][2]
        };
    }
    showDebugShapes = json_.value("ShowDebugShapes", true);
    if (json_.contains("DebugChainColor"))
    {
        debugChainColor = {
            json_["DebugChainColor"][0],
            json_["DebugChainColor"][1],
            json_["DebugChainColor"][2],
            json_["DebugChainColor"][3]
        };
    }
    if (json_.contains("DebugTargetColor"))
    {
        debugTargetColor = {
            json_["DebugTargetColor"][0],
            json_["DebugTargetColor"][1],
            json_["DebugTargetColor"][2],
            json_["DebugTargetColor"][3]
        };
    }
    target.reset();
    lookStates.clear();
}

REGISTER_COMPONENT(HumanLookAtIKComponent, "HumanLookAtIKComponent")
