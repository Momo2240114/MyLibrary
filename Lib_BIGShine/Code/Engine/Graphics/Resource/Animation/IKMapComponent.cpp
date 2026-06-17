#include "IKMapComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/GlTFModel.h"

#include <algorithm>
#include <cmath>
#include <map>

using namespace DirectX;

//--- テスト用 Static 風パラメータ-- -
DirectX::XMFLOAT3 IKMapComponent::windDirection = { 1.0f, 0.0f, 0.0f };
float             IKMapComponent::windStrength = 0.0f;
bool              IKMapComponent::windEnabled = false;

namespace
{
XMFLOAT3 GetTranslation(const XMFLOAT4X4& matrix_)
{
    return { matrix_._41, matrix_._42, matrix_._43 };
}

XMFLOAT3 Add(const XMFLOAT3& a_, const XMFLOAT3& b_)
{
    return { a_.x + b_.x, a_.y + b_.y, a_.z + b_.z };
}

XMFLOAT3 Subtract(const XMFLOAT3& a_, const XMFLOAT3& b_)
{
    return { a_.x - b_.x, a_.y - b_.y, a_.z - b_.z };
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
    if (length_ <= 0.00001f) return { 0.0f, 0.0f, 0.0f };
    return Scale(value_, 1.0f / length_);
}

XMFLOAT3 ApplyRootMotion(
    const XMFLOAT3& position_, const XMFLOAT4X4& previousRootTransform_,
    const XMFLOAT4X4& currentRootTransform_, float rotationFollow_)
{
    const XMFLOAT3 previousRootPosition_ = GetTranslation(previousRootTransform_);
    const XMFLOAT3 currentRootPosition_ = GetTranslation(currentRootTransform_);
    const XMMATRIX previousRootMatrix_ = XMLoadFloat4x4(&previousRootTransform_);
    const XMMATRIX currentRootMatrix_ = XMLoadFloat4x4(&currentRootTransform_);
    const XMMATRIX rootMotionDelta_ = XMMatrixInverse(nullptr, previousRootMatrix_) * currentRootMatrix_;

    XMVECTOR scale_;
    XMVECTOR rotation_;
    XMVECTOR translation_;
    if (!XMMatrixDecompose(&scale_, &rotation_, &translation_, rootMotionDelta_))
    {
        return Add(position_, Subtract(currentRootPosition_, previousRootPosition_));
    }

    const XMVECTOR followedRotation_ = XMQuaternionSlerp(
        XMQuaternionIdentity(),
        XMQuaternionNormalize(rotation_),
        std::clamp(rotationFollow_, 0.0f, 1.0f));
    const XMMATRIX followedRotationMatrix_ = XMMatrixRotationQuaternion(followedRotation_);
    const XMFLOAT3 localOffset_ = Subtract(position_, previousRootPosition_);
    XMFLOAT3 rotatedOffset_;
    XMStoreFloat3(
        &rotatedOffset_,
        XMVector3TransformNormal(XMLoadFloat3(&localOffset_), followedRotationMatrix_));
    return Add(currentRootPosition_, rotatedOffset_);
}

float GetStiffnessScale(const IKMapComponent::IKChain& chain_, size_t pointIndex_)
{
    if (chain_.simulatedPositions.size() <= 1) return 1.0f;

    const float normalizedPosition_ = static_cast<float>(pointIndex_)
        / static_cast<float>(chain_.simulatedPositions.size() - 1);
    const float smoothPosition_ = normalizedPosition_ * normalizedPosition_ * (3.0f - 2.0f * normalizedPosition_);
    const float tipScale_ = std::clamp(chain_.tipStiffnessScale, 0.0f, 1.0f);
    return 1.0f + (tipScale_ - 1.0f) * smoothPosition_;
}

void SolveDistanceConstraint(
    std::vector<XMFLOAT3>& positions_, size_t indexA_, size_t indexB_,
    float targetLength_, float stiffness_)
{
    XMFLOAT3 difference_ = Subtract(positions_[indexB_], positions_[indexA_]);
    const float distance_ = Length(difference_);
    if (distance_ <= 0.00001f) return;

    const XMFLOAT3 correction_ = Scale(
        difference_, ((distance_ - targetLength_) / distance_) * stiffness_);
    if (indexA_ == 0)
    {
        positions_[indexB_] = Subtract(positions_[indexB_], correction_);
        return;
    }

    positions_[indexA_] = Add(positions_[indexA_], Scale(correction_, 0.5f));
    positions_[indexB_] = Subtract(positions_[indexB_], Scale(correction_, 0.5f));
}

}

const std::string& IKMapComponent::GetName() const
{
    static std::string name = "IKMapComponent";
    return name;
}

void IKMapComponent::Start()
{
    auto owner_ = GetGameObject();
    model = owner_ ? owner_->GetComponent<GltfModelCommponennt>().get() : nullptr;
    ResetSimulation();
}

void IKMapComponent::LateUpdate(float deltaTime_)
{
    if (!model)
    {
        auto owner_ = GetGameObject();
        model = owner_ ? owner_->GetComponent<GltfModelCommponennt>().get() : nullptr;
    }
    if (!model) return;

    const float safeDeltaTime_ = std::clamp(deltaTime_, 0.0f, 1.0f / 20.0f);
    for (auto& chain_ : chains)
    {
        if (!chain_.enabled || chain_.type != IK_TYPE::SPRING_BONE) continue;
        if (!ResolveChain(chain_)) continue;
        SimulateSpringChain(chain_, safeDeltaTime_);
        ApplySpringChain(chain_);
    }

    bool hasClothChains_ = false;
    for (auto& chain_ : chains)
    {
        if (!chain_.enabled || chain_.type != IK_TYPE::CLOTH_BONE) continue;
        if (!ResolveChain(chain_)) continue;
        SimulateSpringChain(chain_, safeDeltaTime_);
        hasClothChains_ = true;
    }
    if (hasClothChains_)
    {
        SolveClothConstraints();
        for (auto& chain_ : chains)
        {
            if (!chain_.enabled || chain_.type != IK_TYPE::CLOTH_BONE || !chain_.initialized) continue;
            ApplySpringChain(chain_);
        }
    }
    DrawBodyCollisionShapes();
    DrawHairCollisionShapes();

    if (simulationWarmupRequested)
    {
        simulationWarmupRequested = false;

        for (auto& chain_ : chains)
        {
            chain_.initialized = false;
        }

        clothConstraints.clear();
        clothConstraintsInitialized = false;
    }
}

bool IKMapComponent::ResolveChain(IKChain& chain_)
{
    if (!model || chain_.rootNodeName.empty() || chain_.tipNodeName.empty()) return false;
    if (!chain_.nodeIndices.empty()) return chain_.nodeIndices.size() >= 2;

    const int rootIndex_ = model->GetNodeIndex(chain_.rootNodeName.c_str());
    const int tipIndex_ = model->GetNodeIndex(chain_.tipNodeName.c_str());
    if (rootIndex_ < 0 || tipIndex_ < 0) return false;

    std::vector<int> reverseIndices_;
    int currentIndex_ = tipIndex_;
    const auto& nodes_ = model->GetNodes();
    while (currentIndex_ >= 0)
    {
        reverseIndices_.push_back(currentIndex_);
        if (currentIndex_ == rootIndex_) break;
        currentIndex_ = nodes_.at(currentIndex_).parentIndex;
    }
    if (reverseIndices_.empty() || reverseIndices_.back() != rootIndex_) return false;

    chain_.nodeIndices.assign(reverseIndices_.rbegin(), reverseIndices_.rend());
    chain_.initialized = false;
    return chain_.nodeIndices.size() >= 2;
}

void IKMapComponent::SimulateSpringChain(IKChain& chain_, float deltaTime_)
{
    const auto& nodes_ = model->GetNodes();
    if (!chain_.initialized
        || chain_.simulatedPositions.size() != chain_.nodeIndices.size()
        || chain_.segmentLengths.size() + 1 != chain_.nodeIndices.size())
    {
        chain_.simulatedPositions.clear();
        chain_.previousPositions.clear();
        chain_.segmentLengths.clear();
        chain_.bendLengths.clear();
        for (const int nodeIndex_ : chain_.nodeIndices)
        {
            XMFLOAT3 position_ = GetTranslation(nodes_.at(nodeIndex_).worldTransform);

            if (!chain_.simulatedPositions.empty())
            {
                position_.y += chain_.startLift;
            }

            chain_.simulatedPositions.push_back(position_);
            chain_.previousPositions.push_back(position_);
        }
        for (size_t pointIndex_ = 1; pointIndex_ < chain_.simulatedPositions.size(); ++pointIndex_)
        {
            chain_.segmentLengths.push_back(Length(Subtract(
                chain_.simulatedPositions[pointIndex_], chain_.simulatedPositions[pointIndex_ - 1])));
        }
        for (size_t pointIndex_ = 2; pointIndex_ < chain_.simulatedPositions.size(); ++pointIndex_)
        {
            chain_.bendLengths.push_back(Length(Subtract(
                chain_.simulatedPositions[pointIndex_], chain_.simulatedPositions[pointIndex_ - 2])));
        }
        chain_.previousRootTransform = nodes_.at(chain_.nodeIndices.front()).worldTransform;

        if (chain_.collideWithBodyColliders)
        {
            const int warmupCount_ = std::clamp(chain_.startCollisionWarmup, 0, 16);

            for (int warmupIndex_ = 0; warmupIndex_ < warmupCount_; ++warmupIndex_)
            {
                for (size_t pointIndex_ = 1; pointIndex_ < chain_.simulatedPositions.size(); ++pointIndex_)
                {
                    ResolveBodyCollision(chain_, pointIndex_);
                }

                for (size_t pointIndex_ = 1; pointIndex_ < chain_.simulatedPositions.size(); ++pointIndex_)
                {
                    SolveDistanceConstraint(
                        chain_.simulatedPositions,
                        pointIndex_ - 1,
                        pointIndex_,
                        chain_.segmentLengths[pointIndex_ - 1],
                        1.0f);
                }
            }

            chain_.previousPositions = chain_.simulatedPositions;
        }

        chain_.initialized = true;
        return;
    }

    const XMFLOAT4X4 currentRootTransform_ = nodes_.at(chain_.nodeIndices.front()).worldTransform;
    for (size_t pointIndex_ = 0; pointIndex_ < chain_.simulatedPositions.size(); ++pointIndex_)
    {
        chain_.simulatedPositions[pointIndex_] = ApplyRootMotion(
            chain_.simulatedPositions[pointIndex_],
            chain_.previousRootTransform,
            currentRootTransform_,
            chain_.rootRotationFollow);
        chain_.previousPositions[pointIndex_] = ApplyRootMotion(
            chain_.previousPositions[pointIndex_],
            chain_.previousRootTransform,
            currentRootTransform_,
            chain_.rootRotationFollow);
    }
    chain_.previousRootTransform = currentRootTransform_;

    const XMFLOAT3 animatedRoot_ = GetTranslation(currentRootTransform_);
    const int substeps_ = std::clamp(chain_.solverSubsteps, 1, 8);
    const int iterations_ = std::clamp(chain_.solverIterations, 1, 16);
    const float substepDeltaTime_ = deltaTime_ / static_cast<float>(substeps_);
    const float substepDamping_ = std::pow(std::clamp(chain_.damping, 0.0f, 0.999f), 1.0f / substeps_);

    for (int substepIndex_ = 0; substepIndex_ < substeps_; ++substepIndex_)
    {
        chain_.simulatedPositions.front() = animatedRoot_;
        chain_.previousPositions.front() = animatedRoot_;
        for (size_t pointIndex_ = 1; pointIndex_ < chain_.simulatedPositions.size(); ++pointIndex_)
        {
            const XMFLOAT3 current_ = chain_.simulatedPositions[pointIndex_];
            const XMFLOAT3 velocity_ = Scale(Subtract(current_, chain_.previousPositions[pointIndex_]), substepDamping_);
            const XMFLOAT3 animatedPosition_ = GetTranslation(nodes_.at(chain_.nodeIndices[pointIndex_]).worldTransform);
            const float stiffnessScale_ = GetStiffnessScale(chain_, pointIndex_);
            const float pointStiffness_ = std::clamp(chain_.stiffness * stiffnessScale_, 0.0f, 1.0f);
            const float substepStiffness_ = 1.0f - std::pow(1.0f - pointStiffness_, 1.0f / substeps_);
            const XMFLOAT3 animationPull_ = Scale(Subtract(animatedPosition_, current_), substepStiffness_);
            const XMFLOAT3 gravity_ = {
                0.0f, -chain_.gravity * substepDeltaTime_ * substepDeltaTime_, 0.0f
            };

            // テスト用風力（root は固定なのでスキップ）
            XMFLOAT3 wind_ = { 0.0f, 0.0f, 0.0f };
            if (windEnabled && windStrength > 0.0f)
            {
                const float windScale_ = windStrength * substepDeltaTime_ * substepDeltaTime_;
                wind_ = Scale(windDirection, windScale_);
            }

            chain_.previousPositions[pointIndex_] = current_;
            chain_.simulatedPositions[pointIndex_] = Add(Add(Add(Add(current_, velocity_), animationPull_), gravity_), wind_);
        }

        for (int iterationIndex_ = 0; iterationIndex_ < iterations_; ++iterationIndex_)
        {
            chain_.simulatedPositions.front() = animatedRoot_;
            for (size_t pointIndex_ = 1; pointIndex_ < chain_.simulatedPositions.size(); ++pointIndex_)
            {
                SolveDistanceConstraint(
                    chain_.simulatedPositions, pointIndex_ - 1, pointIndex_,
                    chain_.segmentLengths[pointIndex_ - 1], 1.0f);
            }
            for (size_t pointIndex_ = 2; pointIndex_ < chain_.simulatedPositions.size(); ++pointIndex_)
            {
                SolveDistanceConstraint(
                    chain_.simulatedPositions, pointIndex_ - 2, pointIndex_,
                    chain_.bendLengths[pointIndex_ - 2],
                    std::clamp(chain_.bendStiffness * GetStiffnessScale(chain_, pointIndex_), 0.0f, 1.0f));
            }
            if (chain_.collideWithBodyColliders)
            {
                for (size_t pointIndex_ = 1; pointIndex_ < chain_.simulatedPositions.size(); ++pointIndex_)
                {
                    ResolveBodyCollision(chain_, pointIndex_);
                }
            }
        }
    }
}

void IKMapComponent::BuildClothConstraints()
{
    clothConstraints.clear();

    std::map<int, std::vector<size_t>> groupChainIndices_;
    for (size_t chainIndex_ = 0; chainIndex_ < chains.size(); ++chainIndex_)
    {
        const auto& chain_ = chains[chainIndex_];
        if (!chain_.enabled || chain_.type != IK_TYPE::CLOTH_BONE || !chain_.initialized) continue;
        groupChainIndices_[chain_.clothGroup].push_back(chainIndex_);
    }

    for (auto& [groupIndex_, chainIndices_] : groupChainIndices_)
    {
        (void)groupIndex_;
        std::stable_sort(
            chainIndices_.begin(), chainIndices_.end(),
            [this](size_t chainIndexA_, size_t chainIndexB_)
            {
                return chains[chainIndexA_].clothOrder < chains[chainIndexB_].clothOrder;
            });

        for (size_t pairIndex_ = 1; pairIndex_ < chainIndices_.size(); ++pairIndex_)
        {
            const size_t chainIndexA_ = chainIndices_[pairIndex_ - 1];
            const size_t chainIndexB_ = chainIndices_[pairIndex_];
            const auto& positionsA_ = chains[chainIndexA_].simulatedPositions;
            const auto& positionsB_ = chains[chainIndexB_].simulatedPositions;
            const size_t pointCount_ = (std::min)(positionsA_.size(), positionsB_.size());
            if (pointCount_ < 2) continue;

            // Match normalized layers so chains with different bone counts still connect at their tips.
            for (size_t layerIndex_ = 1; layerIndex_ < pointCount_; ++layerIndex_)
            {
                const float normalizedLayer_ = static_cast<float>(layerIndex_)
                    / static_cast<float>(pointCount_ - 1);
                const size_t pointIndexA_ = static_cast<size_t>(std::round(
                    normalizedLayer_ * static_cast<float>(positionsA_.size() - 1)));
                const size_t pointIndexB_ = static_cast<size_t>(std::round(
                    normalizedLayer_ * static_cast<float>(positionsB_.size() - 1)));

                ClothConstraint lateralConstraint_;
                lateralConstraint_.chainIndexA = chainIndexA_;
                lateralConstraint_.pointIndexA = pointIndexA_;
                lateralConstraint_.chainIndexB = chainIndexB_;
                lateralConstraint_.pointIndexB = pointIndexB_;
                lateralConstraint_.targetLength = Length(Subtract(positionsB_[pointIndexB_], positionsA_[pointIndexA_]));
                clothConstraints.push_back(lateralConstraint_);

                if (layerIndex_ + 1 >= pointCount_) continue;

                const float nextNormalizedLayer_ = static_cast<float>(layerIndex_ + 1)
                    / static_cast<float>(pointCount_ - 1);
                const size_t nextPointIndexA_ = static_cast<size_t>(std::round(
                    nextNormalizedLayer_ * static_cast<float>(positionsA_.size() - 1)));
                const size_t nextPointIndexB_ = static_cast<size_t>(std::round(
                    nextNormalizedLayer_ * static_cast<float>(positionsB_.size() - 1)));

                ClothConstraint diagonalConstraintA_ = lateralConstraint_;
                diagonalConstraintA_.pointIndexB = nextPointIndexB_;
                diagonalConstraintA_.targetLength = Length(Subtract(
                    positionsB_[diagonalConstraintA_.pointIndexB],
                    positionsA_[diagonalConstraintA_.pointIndexA]));
                diagonalConstraintA_.diagonal = true;
                clothConstraints.push_back(diagonalConstraintA_);

                ClothConstraint diagonalConstraintB_ = lateralConstraint_;
                diagonalConstraintB_.pointIndexA = nextPointIndexA_;
                diagonalConstraintB_.targetLength = Length(Subtract(
                    positionsB_[diagonalConstraintB_.pointIndexB],
                    positionsA_[diagonalConstraintB_.pointIndexA]));
                diagonalConstraintB_.diagonal = true;
                clothConstraints.push_back(diagonalConstraintB_);
            }
        }
    }

    clothConstraintsInitialized = true;
}

void IKMapComponent::SolveClothConstraints()
{
    if (!clothConstraintsInitialized)
    {
        BuildClothConstraints();
    }

    int maximumIterations_ = 1;
    for (const auto& chain_ : chains)
    {
        if (!chain_.enabled || chain_.type != IK_TYPE::CLOTH_BONE) continue;
        maximumIterations_ = (std::max)(maximumIterations_, std::clamp(chain_.solverIterations, 1, 16));
    }

    for (int iterationIndex_ = 0; iterationIndex_ < maximumIterations_; ++iterationIndex_)
    {
        for (const auto& constraint_ : clothConstraints)
        {
            SolveClothConstraint(constraint_);
        }
    }
}

void IKMapComponent::SolveClothConstraint(const ClothConstraint& constraint_)
{
    if (constraint_.chainIndexA >= chains.size() || constraint_.chainIndexB >= chains.size()) return;

    auto& chainA_ = chains[constraint_.chainIndexA];
    auto& chainB_ = chains[constraint_.chainIndexB];
    if (!chainA_.enabled || !chainB_.enabled
        || chainA_.type != IK_TYPE::CLOTH_BONE || chainB_.type != IK_TYPE::CLOTH_BONE) return;
    if (constraint_.pointIndexA >= chainA_.simulatedPositions.size()
        || constraint_.pointIndexB >= chainB_.simulatedPositions.size()) return;

    auto& positionA_ = chainA_.simulatedPositions[constraint_.pointIndexA];
    auto& positionB_ = chainB_.simulatedPositions[constraint_.pointIndexB];
    const XMFLOAT3 difference_ = Subtract(positionB_, positionA_);
    const float distance_ = Length(difference_);
    if (distance_ <= 0.00001f) return;

    const float stiffnessScaleA_ = GetStiffnessScale(chainA_, constraint_.pointIndexA);
    const float stiffnessScaleB_ = GetStiffnessScale(chainB_, constraint_.pointIndexB);
    const float chainStiffnessA_ = (constraint_.diagonal ? chainA_.diagonalStiffness : chainA_.lateralStiffness)
        * stiffnessScaleA_;
    const float chainStiffnessB_ = (constraint_.diagonal ? chainB_.diagonalStiffness : chainB_.lateralStiffness)
        * stiffnessScaleB_;
    const float stiffness_ = std::clamp((chainStiffnessA_ + chainStiffnessB_) * 0.5f, 0.0f, 1.0f);
    const XMFLOAT3 correction_ = Scale(
        difference_, ((distance_ - constraint_.targetLength) / distance_) * stiffness_ * 0.5f);
    positionA_ = Add(positionA_, correction_);
    positionB_ = Subtract(positionB_, correction_);

    if (chainA_.collideWithBodyColliders) ResolveBodyCollision(chainA_, constraint_.pointIndexA);
    if (chainB_.collideWithBodyColliders) ResolveBodyCollision(chainB_, constraint_.pointIndexB);
}

void IKMapComponent::ResolveBodyCollision(IKChain& chain_, size_t pointIndex_)
{
    auto& position_ = chain_.simulatedPositions[pointIndex_];
    for (auto& sphere_ : bodyCollisionSpheres)
    {
        XMFLOAT3 center_;
        if (!sphere_.enabled || !GetBodyCollisionSphereCenter(sphere_, center_)) continue;

        const XMFLOAT3 difference_ = Subtract(position_, center_);
        const float distance_ = Length(difference_);
        const float minimumDistance_ = sphere_.radius + chain_.collisionRadius;
        if (distance_ >= minimumDistance_) continue;

        XMFLOAT3 direction_ = Normalize(difference_);
        if (distance_ <= 0.00001f) direction_ = { 0.0f, 1.0f, 0.0f };
        position_ = Add(center_, Scale(direction_, minimumDistance_));
        chain_.previousPositions[pointIndex_] = position_;
    }

    for (size_t sphereIndex_ = 0; sphereIndex_ + 1 < bodyCollisionSpheres.size(); ++sphereIndex_)
    {
        auto& sphereA_ = bodyCollisionSpheres[sphereIndex_];
        auto& sphereB_ = bodyCollisionSpheres[sphereIndex_ + 1];
        if (!sphereA_.connectToNext || !sphereA_.enabled || !sphereB_.enabled) continue;

        XMFLOAT3 centerA_;
        XMFLOAT3 centerB_;
        if (!GetBodyCollisionSphereCenter(sphereA_, centerA_)
            || !GetBodyCollisionSphereCenter(sphereB_, centerB_)) continue;

        const XMFLOAT3 segment_ = Subtract(centerB_, centerA_);
        const float segmentLengthSquared_ =
            segment_.x * segment_.x + segment_.y * segment_.y + segment_.z * segment_.z;
        if (segmentLengthSquared_ <= 0.00001f) continue;

        const XMFLOAT3 fromA_ = Subtract(position_, centerA_);
        const float interpolation_ = std::clamp(
            (fromA_.x * segment_.x + fromA_.y * segment_.y + fromA_.z * segment_.z)
            / segmentLengthSquared_,
            0.0f,
            1.0f);
        const XMFLOAT3 closestPoint_ = Add(centerA_, Scale(segment_, interpolation_));
        const float interpolatedRadius_ =
            sphereA_.radius + (sphereB_.radius - sphereA_.radius) * interpolation_;
        const float minimumDistance_ = interpolatedRadius_ + chain_.collisionRadius;
        const XMFLOAT3 difference_ = Subtract(position_, closestPoint_);
        const float distance_ = Length(difference_);
        if (distance_ >= minimumDistance_) continue;

        XMFLOAT3 direction_ = Normalize(difference_);
        if (distance_ <= 0.00001f) direction_ = { 0.0f, 0.0f, 1.0f };
        position_ = Add(closestPoint_, Scale(direction_, minimumDistance_));
        chain_.previousPositions[pointIndex_] = position_;
    }
}

bool IKMapComponent::GetBodyCollisionSphereCenter(BodyCollisionSphere& sphere_, XMFLOAT3& center_)
{
    if (!model || sphere_.nodeName.empty()) return false;
    if (sphere_.nodeIndex < 0)
    {
        sphere_.nodeIndex = model->GetNodeIndex(sphere_.nodeName.c_str());
    }
    if (sphere_.nodeIndex < 0) return false;

    const auto& nodes_ = model->GetNodes();
    const XMMATRIX nodeWorld_ = XMLoadFloat4x4(&nodes_.at(sphere_.nodeIndex).worldTransform);
    XMStoreFloat3(
        &center_,
        XMVector3TransformCoord(XMLoadFloat3(&sphere_.offset), nodeWorld_));
    return true;
}

void IKMapComponent::DrawBodyCollisionShapes()
{
    if (!showBodyCollisionShapes) return;

    auto& renderer_ = GraphicsDeviceManager::Instance().GetShapeRendererSystem();
    for (auto& sphere_ : bodyCollisionSpheres)
    {
        XMFLOAT3 center_;
        if (!GetBodyCollisionSphereCenter(sphere_, center_)) continue;

        XMFLOAT4 color_ = bodyCollisionShapeColor;
        if (!sphere_.enabled) color_.w *= 0.25f;
        renderer_.DrawSphere(center_, sphere_.radius, color_);
    }

    constexpr int connectionSamples_ = 6;
    for (size_t sphereIndex_ = 0; sphereIndex_ + 1 < bodyCollisionSpheres.size(); ++sphereIndex_)
    {
        auto& sphereA_ = bodyCollisionSpheres[sphereIndex_];
        auto& sphereB_ = bodyCollisionSpheres[sphereIndex_ + 1];
        if (!sphereA_.connectToNext || !sphereA_.enabled || !sphereB_.enabled) continue;

        XMFLOAT3 centerA_;
        XMFLOAT3 centerB_;
        if (!GetBodyCollisionSphereCenter(sphereA_, centerA_)
            || !GetBodyCollisionSphereCenter(sphereB_, centerB_)) continue;

        for (int sampleIndex_ = 1; sampleIndex_ < connectionSamples_; ++sampleIndex_)
        {
            const float interpolation_ =
                static_cast<float>(sampleIndex_) / static_cast<float>(connectionSamples_);
            const XMFLOAT3 center_ = Add(centerA_, Scale(Subtract(centerB_, centerA_), interpolation_));
            const float radius_ = sphereA_.radius + (sphereB_.radius - sphereA_.radius) * interpolation_;
            renderer_.DrawSphere(center_, radius_, bodyCollisionShapeColor);
        }
    }
}

void IKMapComponent::DrawHairCollisionShapes()
{
    if (!showHairCollisionShapes) return;
    if (!model) return;

    auto& renderer_ = GraphicsDeviceManager::Instance().GetShapeRendererSystem();

    for (const auto& chain_ : chains)
    {
        if (!chain_.enabled
            || (chain_.type != IK_TYPE::SPRING_BONE && chain_.type != IK_TYPE::CLOTH_BONE)) continue;
        if (!chain_.initialized) continue;
        if (chain_.simulatedPositions.empty()) continue;

        const float radius_ = (std::max)(chain_.collisionRadius, 0.005f);

        for (size_t pointIndex_ = 1; pointIndex_ < chain_.simulatedPositions.size(); ++pointIndex_)
        {
            renderer_.DrawSphere(
                chain_.simulatedPositions[pointIndex_],
                radius_,
                hairCollisionShapeColor);
        }
    }
}
void IKMapComponent::ApplySpringChain(IKChain& chain_)
{
    auto& nodes_ = model->GetNodes();
    for (size_t nodeIndex_ = 0; nodeIndex_ + 1 < chain_.nodeIndices.size(); ++nodeIndex_)
    {
        auto& node_ = nodes_.at(chain_.nodeIndices[nodeIndex_]);
        const auto& child_ = nodes_.at(chain_.nodeIndices[nodeIndex_ + 1]);

        const XMFLOAT3 currentPosition_ = GetTranslation(node_.worldTransform);
        const XMFLOAT3 childPosition_ = GetTranslation(child_.worldTransform);
        const XMFLOAT3 currentDirection_ = Normalize(Subtract(childPosition_, currentPosition_));
        const XMFLOAT3 targetDirection_ = Normalize(Subtract(chain_.simulatedPositions[nodeIndex_ + 1], currentPosition_));
        if (Length(currentDirection_) <= 0.00001f || Length(targetDirection_) <= 0.00001f) continue;

        const XMVECTOR currentDirectionVector_ = XMLoadFloat3(&currentDirection_);
        const XMVECTOR targetDirectionVector_ = XMLoadFloat3(&targetDirection_);
        const XMVECTOR axis_ = XMVector3Cross(currentDirectionVector_, targetDirectionVector_);
        const float axisLength_ = XMVectorGetX(XMVector3Length(axis_));
        const float dot_ = std::clamp(XMVectorGetX(XMVector3Dot(currentDirectionVector_, targetDirectionVector_)), -1.0f, 1.0f);
        if (axisLength_ <= 0.00001f) continue;

        const float angle_ = std::acos(dot_) * std::clamp(chain_.influence, 0.0f, 1.0f);
        const XMMATRIX deltaWorldRotation_ = XMMatrixRotationAxis(axis_, angle_);
        XMMATRIX nodeWorldRotation_ = XMLoadFloat4x4(&node_.worldTransform);
        nodeWorldRotation_.r[3] = XMVectorSet(0, 0, 0, 1);

        XMMATRIX parentWorldRotation_ = XMMatrixIdentity();
        if (node_.parent)
        {
            parentWorldRotation_ = XMLoadFloat4x4(&node_.parent->worldTransform);
            parentWorldRotation_.r[3] = XMVectorSet(0, 0, 0, 1);
        }
        else
        {
            auto owner_ = GetGameObject();
            if (!owner_ || !owner_->transform) continue;

            parentWorldRotation_ = owner_->transform->GetWorldMatrix();
            parentWorldRotation_.r[3] = XMVectorSet(0, 0, 0, 1);
        }

        const XMMATRIX desiredWorldRotation_ = nodeWorldRotation_ * deltaWorldRotation_;
        const XMMATRIX desiredLocalRotation_ = desiredWorldRotation_ * XMMatrixInverse(nullptr, parentWorldRotation_);
        XMVECTOR scale_;
        XMVECTOR rotation_;
        XMVECTOR translation_;
        if (XMMatrixDecompose(&scale_, &rotation_, &translation_, desiredLocalRotation_))
        {
            XMStoreFloat4(&node_.rotation, XMQuaternionNormalize(rotation_));

            model->UpdateNodeTransformFrom(chain_.nodeIndices.front());
        }
    }


}

void IKMapComponent::Gui()
{
    if (!model)
    {
        auto owner_ = GetGameObject();
        model = owner_ ? owner_->GetComponent<GltfModelCommponennt>().get() : nullptr;
    }
    if (!model)
    {
        ImGui::TextDisabled("Requires GltfModel on the same GameObject.");
        return;
    }

    // ---- テスト用風設定 ----
    ImGui::SeparatorText("Wind (Test)");
    ImGui::Checkbox("Enable Wind", &windEnabled);
    if (windEnabled)
    {
        ImGui::DragFloat("Wind Strength", &windStrength, 0.1f, 0.0f, 200.0f, "%.2f");
        ImGui::TextDisabled("Direction XYZ (normalized automatically)");
        if (ImGui::DragFloat3("Wind Direction", &windDirection.x, 0.01f, -1.0f, 1.0f))
        {
            // 入力後に正規化
            const float len_ = std::sqrt(
                windDirection.x * windDirection.x +
                windDirection.y * windDirection.y +
                windDirection.z * windDirection.z);
            if (len_ > 0.0001f)
            {
                windDirection.x /= len_;
                windDirection.y /= len_;
                windDirection.z /= len_;
            }
        }
        // プリセットボタン
        ImGui::TextDisabled("Direction Presets:");
        if (ImGui::Button("+X"))  windDirection = { 1.0f, 0.0f, 0.0f };
        ImGui::SameLine();
        if (ImGui::Button("-X"))  windDirection = { -1.0f, 0.0f, 0.0f };
        ImGui::SameLine();
        if (ImGui::Button("+Z"))  windDirection = { 0.0f, 0.0f, 1.0f };
        ImGui::SameLine();
        if (ImGui::Button("-Z"))  windDirection = { 0.0f, 0.0f, -1.0f };
        ImGui::SameLine();
        if (ImGui::Button("Up"))  windDirection = { 0.0f, 1.0f, 0.0f };
        if (ImGui::Button("Stop Wind")) { windStrength = 0.0f; }
    }
    ImGui::Separator();


    ImGui::TextDisabled("Spring Bone and Cloth Bone run after animation in LateUpdate.");
    if (ImGui::Button("Add Hair Chain"))
    {
        chains.emplace_back();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Simulation"))
    {
        ResetSimulation();
    }

    const auto& nodes_ = model->GetNodes();
    ImGui::SeparatorText("Body Collision Spheres");
    ImGui::TextDisabled("Lightweight approximation used to keep hair outside the body.");
    ImGui::Checkbox("Show Body Collision Shapes", &showBodyCollisionShapes);
    if (showBodyCollisionShapes)
    {
        ImGui::ColorEdit4("Body Collision Shape Color", &bodyCollisionShapeColor.x);
    }
    ImGui::Checkbox("Show Hair Collision Shapes", &showHairCollisionShapes);
    if (showHairCollisionShapes)
    {
        ImGui::ColorEdit4("Hair Collision Shape Color", &hairCollisionShapeColor.x);
    }
    if (ImGui::Button("Add Body Collision Sphere"))
    {
        bodyCollisionSpheres.emplace_back();
    }
    int removeSphereIndex_ = -1;
    for (size_t sphereIndex_ = 0; sphereIndex_ < bodyCollisionSpheres.size(); ++sphereIndex_)
    {
        auto& sphere_ = bodyCollisionSpheres[sphereIndex_];
        ImGui::PushID(static_cast<int>(sphereIndex_) + 10000);
        if (ImGui::TreeNode("Body Collision Sphere"))
        {
            ImGui::Checkbox("Enabled", &sphere_.enabled);
            char sphereNameBuffer_[64];
            strncpy_s(sphereNameBuffer_, sphere_.name.c_str(), sizeof(sphereNameBuffer_));
            if (ImGui::InputText("Name", sphereNameBuffer_, sizeof(sphereNameBuffer_))) sphere_.name = sphereNameBuffer_;

            const char* nodePreview_ = sphere_.nodeName.empty() ? "Select Node" : sphere_.nodeName.c_str();
            if (ImGui::BeginCombo("Node", nodePreview_))
            {
                for (const auto& node_ : nodes_)
                {
                    if (ImGui::Selectable(node_.name.c_str(), sphere_.nodeName == node_.name))
                    {
                        sphere_.nodeName = node_.name;
                        sphere_.nodeIndex = -1;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::DragFloat3("Offset", &sphere_.offset.x, 0.001f);
            ImGui::DragFloat("Radius", &sphere_.radius, 0.001f, 0.0f, 10.0f);
            if (sphereIndex_ + 1 < bodyCollisionSpheres.size())
            {
                ImGui::Checkbox("Connect To Next", &sphere_.connectToNext);
                ImGui::TextDisabled("Fills the gap to the next sphere with a tapered capsule.");
            }
            if (ImGui::Button("Remove Sphere")) removeSphereIndex_ = static_cast<int>(sphereIndex_);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (removeSphereIndex_ >= 0)
    {
        bodyCollisionSpheres.erase(bodyCollisionSpheres.begin() + removeSphereIndex_);
    }

    ImGui::SeparatorText("Spring / Cloth Bone Chains");
    int removeIndex_ = -1;
    for (size_t chainIndex_ = 0; chainIndex_ < chains.size(); ++chainIndex_)
    {
        auto& chain_ = chains[chainIndex_];
        ImGui::PushID(static_cast<int>(chainIndex_));
        const char* chainTreeLabel_ = chain_.type == IK_TYPE::CLOTH_BONE ? "Cloth Bone Chain" : "Spring Bone Chain";
        if (ImGui::TreeNode(chainTreeLabel_))
        {
            ImGui::Checkbox("Enabled", &chain_.enabled);
            char nameBuffer_[64];
            strncpy_s(nameBuffer_, chain_.name.c_str(), sizeof(nameBuffer_));
            if (ImGui::InputText("Name", nameBuffer_, sizeof(nameBuffer_))) chain_.name = nameBuffer_;

            int typeIndex_ = chain_.type == IK_TYPE::CLOTH_BONE ? 1 : 0;
            const char* typeNames_[] = { "Spring Bone", "Cloth Bone" };
            if (ImGui::Combo("Simulation Type", &typeIndex_, typeNames_, IM_ARRAYSIZE(typeNames_)))
            {
                chain_.type = typeIndex_ == 1 ? IK_TYPE::CLOTH_BONE : IK_TYPE::SPRING_BONE;
                ResetSimulation();
            }
            if (chain_.type == IK_TYPE::CLOTH_BONE)
            {
                if (ImGui::DragInt("Cloth Group", &chain_.clothGroup, 0.1f))
                {
                    ResetSimulation();
                }
                if (ImGui::DragInt("Cloth Order", &chain_.clothOrder, 0.1f))
                {
                    ResetSimulation();
                }
                ImGui::SliderFloat("Lateral Stiffness", &chain_.lateralStiffness, 0.0f, 1.0f);
                ImGui::SliderFloat("Diagonal Stiffness", &chain_.diagonalStiffness, 0.0f, 1.0f);
                ImGui::TextDisabled("Chains in the same group connect in ascending Cloth Order.");
            }

            const char* rootPreview_ = chain_.rootNodeName.empty() ? "Select Root" : chain_.rootNodeName.c_str();
            if (ImGui::BeginCombo("Root Node", rootPreview_))
            {
                for (const auto& node_ : nodes_)
                {
                    if (ImGui::Selectable(node_.name.c_str(), chain_.rootNodeName == node_.name))
                    {
                        chain_.rootNodeName = node_.name;
                        chain_.nodeIndices.clear();
                        chain_.initialized = false;
                    }
                }
                ImGui::EndCombo();
            }

            const char* tipPreview_ = chain_.tipNodeName.empty() ? "Select Tip" : chain_.tipNodeName.c_str();
            if (ImGui::BeginCombo("Tip Node", tipPreview_))
            {
                for (const auto& node_ : nodes_)
                {
                    if (ImGui::Selectable(node_.name.c_str(), chain_.tipNodeName == node_.name))
                    {
                        chain_.tipNodeName = node_.name;
                        chain_.nodeIndices.clear();
                        chain_.initialized = false;
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::DragFloat("Gravity", &chain_.gravity, 0.1f, 0.0f, 100.0f);
            if (ImGui::Button("Apply Rope Preset"))
            {
                chain_.damping = 0.96f;
                chain_.stiffness = 0.01f;
                chain_.bendStiffness = 0.02f;
                chain_.solverSubsteps = 2;
                chain_.solverIterations = 6;
                chain_.initialized = false;
            }

            ImGui::DragFloat("Start Lift", &chain_.startLift, 0.001f, -1.0f, 1.0f);
            ImGui::DragInt("Start Collision Warmup", &chain_.startCollisionWarmup, 0.1f, 0, 16);
            ImGui::SliderFloat("Damping", &chain_.damping, 0.0f, 0.999f);
            ImGui::SliderFloat("Animation Stiffness", &chain_.stiffness, 0.0f, 1.0f);
            ImGui::SliderFloat("Bend Stiffness", &chain_.bendStiffness, 0.0f, 1.0f);
            ImGui::SliderFloat("Tip Stiffness Scale", &chain_.tipStiffnessScale, 0.0f, 1.0f);
            ImGui::TextDisabled("Root uses full stiffness; the tip uses this stiffness scale.");
            ImGui::SliderFloat("Root Rotation Follow", &chain_.rootRotationFollow, 0.0f, 1.0f);
            ImGui::TextDisabled("Lower values preserve more turning inertia.");
            ImGui::DragInt("Solver Substeps", &chain_.solverSubsteps, 0.1f, 1, 8);
            ImGui::DragInt("Solver Iterations", &chain_.solverIterations, 0.1f, 1, 16);
            ImGui::TextDisabled("More iterations make the chain behave like a tighter rope.");
            ImGui::SliderFloat("Influence", &chain_.influence, 0.0f, 1.0f);
            ImGui::Checkbox("Collide With Body Spheres", &chain_.collideWithBodyColliders);
            if (chain_.collideWithBodyColliders)
            {
                ImGui::DragFloat("Collision Radius", &chain_.collisionRadius, 0.001f, 0.0f, 1.0f);
                ImGui::TextDisabled("Keeps spring points outside registered body spheres.");
            }
            ImGui::Text("Resolved Bones: %d", static_cast<int>(chain_.nodeIndices.size()));
            if (ImGui::Button("Remove Chain")) removeIndex_ = static_cast<int>(chainIndex_);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (removeIndex_ >= 0) chains.erase(chains.begin() + removeIndex_);
}

void IKMapComponent::Serialize(nlohmann::json& j_) const
{


    j_["ShowBodyCollisionShapes"] = showBodyCollisionShapes;
    j_["BodyCollisionShapeColor"] = {
        bodyCollisionShapeColor.x,
        bodyCollisionShapeColor.y,
        bodyCollisionShapeColor.z,
        bodyCollisionShapeColor.w
    };
    j_["BodyCollisionSpheres"] = nlohmann::json::array();
    for (const auto& sphere_ : bodyCollisionSpheres)
    {
        j_["BodyCollisionSpheres"].push_back({
            { "Name", sphere_.name },
            { "Node", sphere_.nodeName },
            { "Offset", { sphere_.offset.x, sphere_.offset.y, sphere_.offset.z } },
            { "Radius", sphere_.radius },
            { "ConnectToNext", sphere_.connectToNext },
            { "Enabled", sphere_.enabled }
        });
    }

    j_["Chains"] = nlohmann::json::array();
    for (const auto& chain_ : chains)
    {
        j_["Chains"].push_back({
            { "StartLift", chain_.startLift },
            { "StartCollisionWarmup", chain_.startCollisionWarmup },
            { "Name", chain_.name },
            { "Type", static_cast<int>(chain_.type) },
            { "RootNode", chain_.rootNodeName },
            { "TipNode", chain_.tipNodeName },
            { "Gravity", chain_.gravity },
            { "Damping", chain_.damping },
            { "Stiffness", chain_.stiffness },
            { "BendStiffness", chain_.bendStiffness },
            { "LateralStiffness", chain_.lateralStiffness },
            { "DiagonalStiffness", chain_.diagonalStiffness },
            { "TipStiffnessScale", chain_.tipStiffnessScale },
            { "RootRotationFollow", chain_.rootRotationFollow },
            { "SolverSubsteps", chain_.solverSubsteps },
            { "SolverIterations", chain_.solverIterations },
            { "ClothGroup", chain_.clothGroup },
            { "ClothOrder", chain_.clothOrder },
            { "Influence", chain_.influence },
            { "CollisionRadius", chain_.collisionRadius },
            { "CollideWithBodyColliders", chain_.collideWithBodyColliders },
            { "Enabled", chain_.enabled }
        });
    }
}

void IKMapComponent::Deserialize(const nlohmann::json& j_)
{
    chains.clear();
    bodyCollisionSpheres.clear();
    showBodyCollisionShapes = j_.value("ShowBodyCollisionShapes", true);
    if (j_.contains("BodyCollisionShapeColor"))
    {
        bodyCollisionShapeColor = {
            j_["BodyCollisionShapeColor"][0],
            j_["BodyCollisionShapeColor"][1],
            j_["BodyCollisionShapeColor"][2],
            j_["BodyCollisionShapeColor"][3]
        };
    }
    if (j_.contains("BodyCollisionSpheres"))
    {
        for (const auto& sphereJson_ : j_["BodyCollisionSpheres"])
        {
            BodyCollisionSphere sphere_;
            sphere_.name = sphereJson_.value("Name", std::string("Head"));
            sphere_.nodeName = sphereJson_.value("Node", std::string());
            if (sphereJson_.contains("Offset"))
            {
                sphere_.offset = {
                    sphereJson_["Offset"][0],
                    sphereJson_["Offset"][1],
                    sphereJson_["Offset"][2]
                };
            }
            sphere_.radius = sphereJson_.value("Radius", 0.15f);
            sphere_.connectToNext = sphereJson_.value("ConnectToNext", false);
            sphere_.enabled = sphereJson_.value("Enabled", true);
            bodyCollisionSpheres.push_back(sphere_);
        }
    }
    if (!j_.contains("Chains")) return;
    for (const auto& chainJson_ : j_["Chains"])
    {
        IKChain chain_;
        chain_.name = chainJson_.value("Name", std::string("Hair"));
        chain_.type = static_cast<IK_TYPE>(chainJson_.value("Type", static_cast<int>(IK_TYPE::SPRING_BONE)));
        chain_.rootNodeName = chainJson_.value("RootNode", std::string());
        chain_.tipNodeName = chainJson_.value("TipNode", std::string());
        chain_.gravity = chainJson_.value("Gravity", 9.8f);
        chain_.damping = chainJson_.value("Damping", 0.88f);
        chain_.stiffness = chainJson_.value("Stiffness", 0.02f);
        chain_.bendStiffness = chainJson_.value("BendStiffness", 0.02f);
        chain_.lateralStiffness = chainJson_.value("LateralStiffness", 0.35f);
        chain_.diagonalStiffness = chainJson_.value("DiagonalStiffness", 0.15f);
        chain_.tipStiffnessScale = chainJson_.value("TipStiffnessScale", 0.2f);
        chain_.rootRotationFollow = chainJson_.value("RootRotationFollow", 0.7f);
        chain_.solverSubsteps = chainJson_.value("SolverSubsteps", 2);
        chain_.solverIterations = chainJson_.value("SolverIterations", 4);
        chain_.clothGroup = chainJson_.value("ClothGroup", 0);
        chain_.clothOrder = chainJson_.value("ClothOrder", 0);
        chain_.influence = chainJson_.value("Influence", 1.0f);
        chain_.collisionRadius = chainJson_.value("CollisionRadius", 0.02f);
        chain_.collideWithBodyColliders = chainJson_.value(
            "CollideWithBodyColliders", chainJson_.value("CollideWithOwnMesh", true));
        chain_.enabled = chainJson_.value("Enabled", true);
        chain_.startLift = chainJson_.value("StartLift", 0.08f);
        chain_.startCollisionWarmup = chainJson_.value("StartCollisionWarmup", 4);
        chains.push_back(chain_);
    }
}

IKMapComponent::IKChain& IKMapComponent::AddSpringChain(
    const std::string& name_, const std::string& rootNodeName_, const std::string& tipNodeName_)
{
    IKChain chain_;
    chain_.name = name_;
    chain_.type = IK_TYPE::SPRING_BONE;
    chain_.rootNodeName = rootNodeName_;
    chain_.tipNodeName = tipNodeName_;
    chains.push_back(chain_);
    return chains.back();
}

void IKMapComponent::ResetSimulation()
{
    clothConstraints.clear();
    clothConstraintsInitialized = false;
    for (auto& sphere_ : bodyCollisionSpheres)
    {
        sphere_.nodeIndex = -1;
    }
    for (auto& chain_ : chains)
    {
        chain_.nodeIndices.clear();
        chain_.segmentLengths.clear();
        chain_.bendLengths.clear();
        chain_.simulatedPositions.clear();
        chain_.previousPositions.clear();
        chain_.initialized = false;
    }
}
