#include "FootIKComponent.h"

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Physics/ColliderComponent.h"
#include "Engine/Physics/PhysXSystem.h"

#include <PxPhysicsAPI.h>
#include <algorithm>
#include <cmath>
#include <imgui.h>

using namespace DirectX;
using namespace physx;

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

    class FootQueryFilter : public PxQueryFilterCallback
    {
    public:
        explicit FootQueryFilter(const std::vector<const void*>& ignoredUserData_)
            : ignoredUserData(ignoredUserData_)
        {
        }

        PxQueryHitType::Enum preFilter(
            const PxFilterData& filterData_,
            const PxShape* shape_,
            const PxRigidActor* actor_,
            PxHitFlags& queryFlags_) override
        {
            (void)filterData_;
            (void)queryFlags_;
            if (!shape_ || !actor_) return PxQueryHitType::eNONE;
            if (!shape_->getFlags().isSet(PxShapeFlag::eSCENE_QUERY_SHAPE)) return PxQueryHitType::eNONE;
            if (shape_->getFlags().isSet(PxShapeFlag::eTRIGGER_SHAPE)) return PxQueryHitType::eNONE;
            if (std::find(ignoredUserData.begin(), ignoredUserData.end(), actor_->userData)
                != ignoredUserData.end())
            {
                return PxQueryHitType::eNONE;
            }
            return PxQueryHitType::eBLOCK;
        }

        PxQueryHitType::Enum postFilter(
            const PxFilterData& filterData_,
            const PxQueryHit& hit_,
            const PxShape* shape_,
            const PxRigidActor* actor_) override
        {
            (void)filterData_;
            (void)hit_;
            (void)shape_;
            (void)actor_;
            return PxQueryHitType::eBLOCK;
        }

    private:
        const std::vector<const void*>& ignoredUserData;
    };
}

const std::string& FootIKComponent::GetName() const
{
    static const std::string name_ = "FootIKComponent";
    return name_;
}

void FootIKComponent::Start()
{
    if (!ResolveDependencies()) return;
    ikMap->AddFootChain("FootL");
    ikMap->AddFootChain("FootR");
}

bool FootIKComponent::ResolveDependencies()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return false;
    if (!ikMap) ikMap = owner_->GetComponent<IKMapComponent>().get();
    if (!model) model = owner_->GetComponent<GltfModelCommponennt>().get();
    return ikMap && model;
}

void FootIKComponent::LateUpdate(float deltaTime_)
{
    BS_PROFILE_SCOPE("FootIKComponent::LateUpdate");
    if (!enabled || !ResolveDependencies()) return;

    std::vector<IKMapComponent::IKChain>& chains_ = ikMap->GetChains();
    if (footStates.size() != chains_.size()) footStates.resize(chains_.size());
    for (size_t chainIndex_ = 0; chainIndex_ < chains_.size(); ++chainIndex_)
    {
        IKMapComponent::IKChain& chain_ = chains_[chainIndex_];
        if (!chain_.enabled || chain_.type != IKMapComponent::IK_TYPE::FOOT) continue;
        if (ResolveChain(chain_)) SolveChain(chain_, footStates[chainIndex_], deltaTime_);
    }
}

bool FootIKComponent::ResolveChain(IKMapComponent::IKChain& chain_)
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

bool FootIKComponent::QueryGround(
    const IKMapComponent::IKChain& chain_,
    const XMFLOAT3& footPosition_,
    XMFLOAT3& hitPosition_,
    XMFLOAT3& hitNormal_) const
{
    PxScene* scene_ = PhysXSystem::GetScene();
    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!scene_ || !owner_) return false;

    std::vector<const void*> ignoredUserData_;
    for (const std::shared_ptr<ColliderComponent>& collider_ : owner_->GetComponents<ColliderComponent>())
    {
        if (collider_) ignoredUserData_.push_back(collider_.get());
    }
    FootQueryFilter queryFilter_(ignoredUserData_);
    PxQueryFlags queryFlags_ = PxQueryFlag::eSTATIC | PxQueryFlag::ePREFILTER;
    if (chain_.footIncludeDynamicColliders) queryFlags_ |= PxQueryFlag::eDYNAMIC;

    const float safeStepUp_ = (std::max)(chain_.footMaxStepUp, 0.0f);
    const float safeStepDown_ = (std::max)(chain_.footMaxStepDown, 0.0f);
    const float safeDistance_ = (std::max)(safeStepUp_ + safeStepDown_, EPSILON);
    PxRaycastBuffer rayHit_;
    const bool hasHit_ = scene_->raycast(
        PxVec3(footPosition_.x, footPosition_.y + safeStepUp_, footPosition_.z),
        PxVec3(0.0f, -1.0f, 0.0f),
        safeDistance_,
        rayHit_,
        PxHitFlag::eDEFAULT | PxHitFlag::eMESH_BOTH_SIDES,
        PxQueryFilterData(PxFilterData(), queryFlags_),
        &queryFilter_);
    if (!hasHit_ || !rayHit_.hasBlock) return false;

    const PxRaycastHit& block_ = rayHit_.block;
    hitNormal_ = Normalize({ block_.normal.x, block_.normal.y, block_.normal.z });
    hitPosition_ = Add(
        { block_.position.x, block_.position.y, block_.position.z },
        Scale(hitNormal_, chain_.footHeightOffset));
    return true;
}

void FootIKComponent::SolveChain(
    IKMapComponent::IKChain& chain_,
    FootRuntimeState& state_,
    float deltaTime_)
{
    const std::vector<GltfModelCommponennt::Node>& nodes_ = model->GetNodes();
    std::vector<XMFLOAT3> animatedPositions_;
    for (const int nodeIndex_ : chain_.nodeIndices)
    {
        animatedPositions_.push_back(GetTranslation(nodes_.at(nodeIndex_).worldTransform));
    }

    XMFLOAT3 hitPosition_{};
    XMFLOAT3 hitNormal_{ 0.0f, 1.0f, 0.0f };
    const bool grounded_ = QueryGround(chain_, animatedPositions_.back(), hitPosition_, hitNormal_);
    if (!state_.initialized)
    {
        state_.targetPosition = animatedPositions_.back();
        state_.groundNormal = hitNormal_;
        state_.initialized = true;
    }
    const float safeDeltaTime_ = std::clamp(deltaTime_, 0.0f, 0.1f);
    const float blend_ = 1.0f - std::exp(
        -(std::max)(chain_.footPositionSpeed, 0.0f) * safeDeltaTime_);
    state_.targetPosition = Lerp(
        state_.targetPosition,
        grounded_ ? hitPosition_ : animatedPositions_.back(),
        blend_);
    state_.groundNormal = Lerp(state_.groundNormal, hitNormal_, blend_);
    state_.grounded = grounded_;

    std::vector<XMFLOAT3> solvedPositions_ = animatedPositions_;
    std::vector<float> lengths_;
    float totalLength_ = 0.0f;
    for (size_t pointIndex_ = 1; pointIndex_ < solvedPositions_.size(); ++pointIndex_)
    {
        const float length_ = Length(Subtract(solvedPositions_[pointIndex_], solvedPositions_[pointIndex_ - 1]));
        lengths_.push_back(length_);
        totalLength_ += length_;
    }

    const XMFLOAT3 rootPosition_ = solvedPositions_.front();
    const XMFLOAT3 targetPosition_ = Lerp(animatedPositions_.back(), state_.targetPosition, chain_.influence);
    if (Length(Subtract(targetPosition_, rootPosition_)) >= totalLength_ - EPSILON)
    {
        const XMFLOAT3 direction_ = Normalize(Subtract(targetPosition_, rootPosition_));
        for (size_t pointIndex_ = 1; pointIndex_ < solvedPositions_.size(); ++pointIndex_)
        {
            solvedPositions_[pointIndex_] = Add(solvedPositions_[pointIndex_ - 1], Scale(direction_, lengths_[pointIndex_ - 1]));
        }
    }
    else
    {
        for (int iteration_ = 0;
            iteration_ < std::clamp(chain_.footSolverIterations, 1, 32);
            ++iteration_)
        {
            solvedPositions_.back() = targetPosition_;
            for (size_t pointIndex_ = solvedPositions_.size() - 1; pointIndex_ > 0; --pointIndex_)
            {
                const XMFLOAT3 direction_ = Normalize(Subtract(solvedPositions_[pointIndex_ - 1], solvedPositions_[pointIndex_]));
                solvedPositions_[pointIndex_ - 1] = Add(solvedPositions_[pointIndex_], Scale(direction_, lengths_[pointIndex_ - 1]));
            }
            solvedPositions_.front() = rootPosition_;
            for (size_t pointIndex_ = 1; pointIndex_ < solvedPositions_.size(); ++pointIndex_)
            {
                const XMFLOAT3 direction_ = Normalize(Subtract(solvedPositions_[pointIndex_], solvedPositions_[pointIndex_ - 1]));
                solvedPositions_[pointIndex_] = Add(solvedPositions_[pointIndex_ - 1], Scale(direction_, lengths_[pointIndex_ - 1]));
            }
            if (Length(Subtract(solvedPositions_.back(), targetPosition_)) <= 0.001f) break;
        }
    }

    ApplyPoleConstraint(chain_, animatedPositions_, solvedPositions_);
    ApplySolvedPose(chain_, solvedPositions_);
    if (state_.grounded && chain_.footAlignToGround)
    {
        AlignFootToGround(chain_, Normalize(state_.groundNormal), safeDeltaTime_);
    }
    DrawDebugShapes(chain_, animatedPositions_.back(), state_, solvedPositions_);
}

void FootIKComponent::ApplySolvedPose(
    const IKMapComponent::IKChain& chain_,
    const std::vector<XMFLOAT3>& solvedPositions_)
{
    std::vector<GltfModelCommponennt::Node>& nodes_ = model->GetNodes();
    for (size_t pointIndex_ = 0; pointIndex_ + 1 < chain_.nodeIndices.size(); ++pointIndex_)
    {
        GltfModelCommponennt::Node& node_ = nodes_.at(chain_.nodeIndices[pointIndex_]);
        const GltfModelCommponennt::Node& child_ = nodes_.at(chain_.nodeIndices[pointIndex_ + 1]);
        const XMFLOAT3 currentDirection_ = Normalize(Subtract(GetTranslation(child_.worldTransform), GetTranslation(node_.worldTransform)));
        const XMFLOAT3 targetDirection_ = Normalize(Subtract(solvedPositions_[pointIndex_ + 1], solvedPositions_[pointIndex_]));
        const XMVECTOR currentVector_ = XMLoadFloat3(&currentDirection_);
        const XMVECTOR targetVector_ = XMLoadFloat3(&targetDirection_);
        XMVECTOR axis_ = XMVector3Cross(currentVector_, targetVector_);
        if (XMVectorGetX(XMVector3LengthSq(axis_)) <= EPSILON) continue;

        axis_ = XMVector3Normalize(axis_);
        XMMATRIX worldRotation_ = XMLoadFloat4x4(&node_.worldTransform);
        worldRotation_.r[3] = XMVectorSet(0, 0, 0, 1);
        XMMATRIX parentWorldRotation_ = XMMatrixIdentity();
        if (node_.parent)
        {
            parentWorldRotation_ = XMLoadFloat4x4(&node_.parent->worldTransform);
            parentWorldRotation_.r[3] = XMVectorSet(0, 0, 0, 1);
        }
        else if (const std::shared_ptr<GameObject> owner_ = GetGameObject())
        {
            parentWorldRotation_ = owner_->transform->GetWorldMatrix();
            parentWorldRotation_.r[3] = XMVectorSet(0, 0, 0, 1);
        }

        const float dot_ = std::clamp(XMVectorGetX(XMVector3Dot(currentVector_, targetVector_)), -1.0f, 1.0f);
        const XMMATRIX desiredWorldRotation_ = worldRotation_ * XMMatrixRotationAxis(axis_, std::acos(dot_));
        const XMMATRIX desiredLocalRotation_ = desiredWorldRotation_ * XMMatrixInverse(nullptr, parentWorldRotation_);
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

void FootIKComponent::AlignFootToGround(
    const IKMapComponent::IKChain& chain_,
    const XMFLOAT3& groundNormal_,
    float deltaTime_)
{
    std::vector<GltfModelCommponennt::Node>& nodes_ = model->GetNodes();
    GltfModelCommponennt::Node& footNode_ = nodes_.at(chain_.nodeIndices.back());
    XMMATRIX footWorldRotation_ = XMLoadFloat4x4(&footNode_.worldTransform);
    footWorldRotation_.r[3] = XMVectorSet(0, 0, 0, 1);
    const XMVECTOR currentUp_ = XMVector3Normalize(footWorldRotation_.r[1]);
    const XMVECTOR desiredUp_ = XMLoadFloat3(&groundNormal_);
    XMVECTOR axis_ = XMVector3Cross(currentUp_, desiredUp_);
    if (XMVectorGetX(XMVector3LengthSq(axis_)) <= EPSILON) return;

    axis_ = XMVector3Normalize(axis_);
    const float dot_ = std::clamp(XMVectorGetX(XMVector3Dot(currentUp_, desiredUp_)), -1.0f, 1.0f);
    const float blend_ = 1.0f - std::exp(
        -(std::max)(chain_.footRotationSpeed, 0.0f) * deltaTime_);
    XMMATRIX parentWorldRotation_ = XMLoadFloat4x4(&footNode_.parent->worldTransform);
    parentWorldRotation_.r[3] = XMVectorSet(0, 0, 0, 1);
    const XMMATRIX desiredWorldRotation_ = footWorldRotation_
        * XMMatrixRotationAxis(axis_, std::acos(dot_) * blend_ * chain_.influence);
    const XMMATRIX desiredLocalRotation_ = desiredWorldRotation_ * XMMatrixInverse(nullptr, parentWorldRotation_);
    XMVECTOR scale_;
    XMVECTOR rotation_;
    XMVECTOR translation_;
    if (XMMatrixDecompose(&scale_, &rotation_, &translation_, desiredLocalRotation_))
    {
        XMStoreFloat4(&footNode_.rotation, XMQuaternionNormalize(rotation_));
        model->UpdateNodeTransformFrom(chain_.nodeIndices.back());
    }
}

void FootIKComponent::ApplyPoleConstraint(
    const IKMapComponent::IKChain& chain_,
    const std::vector<XMFLOAT3>& animatedPositions_,
    std::vector<XMFLOAT3>& solvedPositions_) const
{
    const float bendWeight_ = std::clamp(chain_.footBendWeight, 0.0f, 1.0f);
    if (bendWeight_ <= EPSILON || solvedPositions_.size() < 3) return;

    XMFLOAT3 configuredPolePosition_{};
    bool hasConfiguredPole_ = false;
    if (!chain_.footPoleNodeName.empty())
    {
        const int poleNodeIndex_ = model->GetNodeIndex(chain_.footPoleNodeName.c_str());
        if (poleNodeIndex_ >= 0)
        {
            configuredPolePosition_ = GetTranslation(model->GetNodes().at(poleNodeIndex_).worldTransform);
            hasConfiguredPole_ = true;
        }
    }

    for (size_t pointIndex_ = 1; pointIndex_ + 1 < solvedPositions_.size(); ++pointIndex_)
    {
        const XMFLOAT3 previousPosition_ = solvedPositions_[pointIndex_ - 1];
        const XMFLOAT3 nextPosition_ = solvedPositions_[pointIndex_ + 1];
        const XMFLOAT3 axisDirection_ = Normalize(Subtract(nextPosition_, previousPosition_));
        if (Length(axisDirection_) <= EPSILON) continue;

        const XMFLOAT3 polePosition_ = hasConfiguredPole_
            ? configuredPolePosition_
            : animatedPositions_[pointIndex_];
        const XMFLOAT3 currentOffset_ = Subtract(
            solvedPositions_[pointIndex_], previousPosition_);
        const XMFLOAT3 poleOffset_ = Subtract(polePosition_, previousPosition_);
        const XMVECTOR axisVector_ = XMLoadFloat3(&axisDirection_);
        const XMVECTOR currentVector_ = XMLoadFloat3(&currentOffset_);
        const XMVECTOR poleVector_ = XMLoadFloat3(&poleOffset_);
        const XMVECTOR currentProjected_ = XMVectorSubtract(
            currentVector_,
            XMVectorScale(axisVector_, XMVectorGetX(XMVector3Dot(currentVector_, axisVector_))));
        const XMVECTOR poleProjected_ = XMVectorSubtract(
            poleVector_,
            XMVectorScale(axisVector_, XMVectorGetX(XMVector3Dot(poleVector_, axisVector_))));
        if (XMVectorGetX(XMVector3LengthSq(currentProjected_)) <= EPSILON
            || XMVectorGetX(XMVector3LengthSq(poleProjected_)) <= EPSILON)
        {
            continue;
        }

        const XMVECTOR currentDirection_ = XMVector3Normalize(currentProjected_);
        const XMVECTOR poleDirection_ = XMVector3Normalize(poleProjected_);
        const float cosine_ = std::clamp(
            XMVectorGetX(XMVector3Dot(currentDirection_, poleDirection_)),
            -1.0f,
            1.0f);
        const float sine_ = XMVectorGetX(XMVector3Dot(
            axisVector_,
            XMVector3Cross(currentDirection_, poleDirection_)));
        const float angle_ = std::atan2(sine_, cosine_) * bendWeight_;
        const XMVECTOR rotatedVector_ = XMVector3Rotate(
            currentVector_,
            XMQuaternionRotationAxis(axisVector_, angle_));
        XMStoreFloat3(
            &solvedPositions_[pointIndex_],
            XMVectorAdd(XMLoadFloat3(&previousPosition_), rotatedVector_));
    }
}

void FootIKComponent::DrawDebugShapes(
    const IKMapComponent::IKChain& chain_,
    const XMFLOAT3& animatedFootPosition_,
    const FootRuntimeState& state_,
    const std::vector<XMFLOAT3>& solvedPositions_) const
{
    if (!showDebugShapes) return;
    ShapeRenderSystem& renderer_ = GraphicsDeviceManager::Instance().GetShapeRendererSystem();

    if (!chain_.footPoleNodeName.empty())
    {
        const int poleNodeIndex_ = model->GetNodeIndex(chain_.footPoleNodeName.c_str());
        if (poleNodeIndex_ >= 0)
        {
            const XMFLOAT3 polePosition_ = GetTranslation(
                model->GetNodes().at(poleNodeIndex_).worldTransform);
            DrawBoneBetween(
                renderer_,
                solvedPositions_.front(),
                polePosition_,
                debugRayColor);
        }
    }
    for (size_t pointIndex_ = 0; pointIndex_ + 1 < solvedPositions_.size(); ++pointIndex_)
    {
        DrawBoneBetween(
            renderer_,
            solvedPositions_[pointIndex_],
            solvedPositions_[pointIndex_ + 1],
            debugChainColor);
    }

    const XMFLOAT3 rayStart_ = Add(
        animatedFootPosition_,
        { 0.0f, (std::max)(chain_.footMaxStepUp, 0.0f), 0.0f });
    const XMFLOAT3 rayEnd_ = Add(
        animatedFootPosition_,
        { 0.0f, -(std::max)(chain_.footMaxStepDown, 0.0f), 0.0f });
    DrawBoneBetween(renderer_, rayStart_, rayEnd_, debugRayColor);

    if (!state_.grounded) return;
    const XMFLOAT3 normalEnd_ = Add(
        state_.targetPosition,
        Scale(Normalize(state_.groundNormal), (std::max)(debugNormalLength, 0.001f)));
    DrawBoneBetween(renderer_, state_.targetPosition, normalEnd_, debugGroundColor);
}

void FootIKComponent::Gui()
{
    ImGui::Checkbox("Enabled", &enabled);
    ImGui::Checkbox("Show Debug Shapes", &showDebugShapes);
    if (showDebugShapes)
    {
        ImGui::DragFloat("Debug Normal Length", &debugNormalLength, 0.01f, 0.001f, 5.0f);
        ImGui::ColorEdit4("Debug Chain Color", &debugChainColor.x);
        ImGui::ColorEdit4("Debug Ray Color", &debugRayColor.x);
        ImGui::ColorEdit4("Debug Ground Color", &debugGroundColor.x);
    }

    if (!ResolveDependencies())
    {
        ImGui::TextDisabled("Requires GltfModel and IKMapComponent on the same GameObject.");
        return;
    }
    ImGui::SeparatorText("IKMap Chains");
    ImGui::TextDisabled("FootL / FootR chains are created and edited in IKMapComponent.");
    std::vector<IKMapComponent::IKChain>& chains_ = ikMap->GetChains();
    int footChainCount_ = 0;
    for (size_t chainIndex_ = 0; chainIndex_ < chains_.size(); ++chainIndex_)
    {
        IKMapComponent::IKChain& chain_ = chains_[chainIndex_];
        if (chain_.type != IKMapComponent::IK_TYPE::FOOT) continue;
        ++footChainCount_;
        ImGui::PushID(static_cast<int>(chainIndex_));
        ImGui::Text("%s: %s -> %s",
            chain_.name.c_str(),
            chain_.rootNodeName.empty() ? "Select Root" : chain_.rootNodeName.c_str(),
            chain_.tipNodeName.empty() ? "Select Tip" : chain_.tipNodeName.c_str());
        ImGui::PopID();
    }
    if (footChainCount_ == 0)
    {
        ImGui::TextDisabled("No Foot IK chains. Use Add Foot L / Add Foot R in IKMapComponent.");
    }
}

void FootIKComponent::Serialize(nlohmann::json& json_) const
{
    json_["Enabled"] = enabled;
    json_["ShowDebugShapes"] = showDebugShapes;
    json_["DebugNormalLength"] = debugNormalLength;
    json_["DebugChainColor"] = { debugChainColor.x, debugChainColor.y, debugChainColor.z, debugChainColor.w };
    json_["DebugRayColor"] = { debugRayColor.x, debugRayColor.y, debugRayColor.z, debugRayColor.w };
    json_["DebugGroundColor"] = { debugGroundColor.x, debugGroundColor.y, debugGroundColor.z, debugGroundColor.w };
}

void FootIKComponent::Deserialize(const nlohmann::json& json_)
{
    enabled = json_.value("Enabled", true);
    showDebugShapes = json_.value("ShowDebugShapes", true);
    debugNormalLength = json_.value("DebugNormalLength", 0.2f);
    if (json_.contains("DebugChainColor"))
    {
        debugChainColor = { json_["DebugChainColor"][0], json_["DebugChainColor"][1], json_["DebugChainColor"][2], json_["DebugChainColor"][3] };
    }
    if (json_.contains("DebugRayColor"))
    {
        debugRayColor = { json_["DebugRayColor"][0], json_["DebugRayColor"][1], json_["DebugRayColor"][2], json_["DebugRayColor"][3] };
    }
    if (json_.contains("DebugGroundColor"))
    {
        debugGroundColor = { json_["DebugGroundColor"][0], json_["DebugGroundColor"][1], json_["DebugGroundColor"][2], json_["DebugGroundColor"][3] };
    }
    footStates.clear();
}

REGISTER_COMPONENT(FootIKComponent, "FootIKComponent")
