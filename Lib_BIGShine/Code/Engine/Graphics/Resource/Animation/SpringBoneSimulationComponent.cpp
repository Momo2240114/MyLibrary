#include "PrecompiledHeader.h"

#include "SpringBoneSimulationComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/GlTFModel.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <imgui.h>

#include "Engine/Debug/TracyProfiler.h"

using namespace DirectX;

//--- テスト用 Static 風パラメータ ---
DirectX::XMFLOAT3 SpringBoneSimulationComponent::windDirection = { 1.0f, 0.0f, 0.0f };
float             SpringBoneSimulationComponent::windStrength = 0.0f;
bool              SpringBoneSimulationComponent::windEnabled = false;

namespace
{
    // スプリング/クロスの点列計算、制約解決、デバッグ描画で使うファイル内ヘルパー。
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

    void DrawBoneBetween(
        ShapeRenderSystem& renderer_,
        const XMFLOAT3& startPosition_,
        const XMFLOAT3& endPosition_,
        const XMFLOAT4& color_)
    {
        const XMFLOAT3 difference_ = Subtract(endPosition_, startPosition_);
        const float length_ = Length(difference_);
        if (length_ <= 0.00001f) return;

        const XMVECTOR forward_ = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        const XMVECTOR direction_ = XMVector3Normalize(XMLoadFloat3(&difference_));
        const float dot_ = std::clamp(
            XMVectorGetX(XMVector3Dot(forward_, direction_)),
            -1.0f,
            1.0f);
        XMMATRIX transform_ = XMMatrixIdentity();
        if (dot_ < 0.99999f)
        {
            XMVECTOR axis_ = XMVector3Cross(forward_, direction_);
            if (XMVectorGetX(XMVector3LengthSq(axis_)) <= 0.00001f)
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


    float GetStiffnessScale(const IKMapComponent::IKChain& chain_, size_t pointIndex_)
    {
        // 根元側はアニメーション追従を強く、先端側はtipStiffnessScaleで揺れを残す。
        if (chain_.simulatedPositions.size() <= 1) return 1.0f;

        const float normalizedPosition_ = static_cast<float>(pointIndex_)
            / static_cast<float>(chain_.simulatedPositions.size() - 1);
        const float smoothPosition_ = normalizedPosition_ * normalizedPosition_ * (3.0f - 2.0f * normalizedPosition_);
        const float tipScale_ = std::clamp(chain_.tipStiffnessScale, 0.0f, 1.0f);
        return 1.0f + (tipScale_ - 1.0f) * smoothPosition_;
    }

    float GetNormalizedPointPosition(const IKMapComponent::IKChain& chain_, size_t pointIndex_)
    {
        if (chain_.simulatedPositions.size() <= 1) return 0.0f;
        return static_cast<float>(pointIndex_)
            / static_cast<float>(chain_.simulatedPositions.size() - 1);
    }

    float SmoothStep(float value_)
    {
        const float clampedValue_ = std::clamp(value_, 0.0f, 1.0f);
        return clampedValue_ * clampedValue_ * (3.0f - 2.0f * clampedValue_);
    }

    float GetMiddleFlexWeight(float normalizedPoint_)
    {
        // 髪束の中央付近だけ少し柔らかくするため、0.5付近で最大になる重みを返す。
        const float centeredPoint_ = std::abs(normalizedPoint_ - 0.5f) * 2.0f;
        return 1.0f - SmoothStep(centeredPoint_);
    }

    XMFLOAT3 ClampLength(const XMFLOAT3& value_, float maxLength_)
    {
        const float length_ = Length(value_);
        if (length_ <= maxLength_ || length_ <= 0.00001f) return value_;
        return Scale(value_, maxLength_ / length_);
    }

    void SolveDistanceConstraint(
        std::vector<XMFLOAT3>& positions_, size_t indexA_, size_t indexB_,
        float targetLength_, float stiffness_)
    {
        // 点同士の距離を元のボーン長へ戻し、髪や布が伸び縮みしすぎないようにする。
        XMFLOAT3 difference_ = Subtract(positions_[indexB_], positions_[indexA_]);
        const float distance_ = Length(difference_);
        if (distance_ <= 0.00001f) return;

        const XMFLOAT3 correction_ = Scale(
            difference_, ((distance_ - targetLength_) / distance_) * stiffness_);
        if (indexB_ <= 1) return;
        if (indexA_ <= 1)
        {
            positions_[indexB_] = Subtract(positions_[indexB_], correction_);
            return;
        }

        positions_[indexA_] = Add(positions_[indexA_], Scale(correction_, 0.5f));
        positions_[indexB_] = Subtract(positions_[indexB_], Scale(correction_, 0.5f));
    }

}

const std::string& SpringBoneSimulationComponent::GetName() const
{
    static std::string name = "SpringBoneSimulationComponent";
    return name;
}

void SpringBoneSimulationComponent::Start()
{
    // IKMapとモデルを解決し、前回のシミュレーション履歴が残らないよう開始時にリセットする。
    ResolveDependencies();
    ResetSimulation();
}

bool SpringBoneSimulationComponent::IsLoadReady() const
{
    const auto owner_ = const_cast<SpringBoneSimulationComponent*>(this)->GetGameObject();
    const auto model_ = owner_ ? owner_->GetComponent<GltfModelCommponennt>() : nullptr;
    if (!model_) return true;

    model_->UpdateLoadState();
    return !model_->IsModelLoading();
}

bool SpringBoneSimulationComponent::ResolveDependencies()
{
    // IKMapに定義されたチェーンを、このコンポーネントが毎フレーム解くための参照を取得する。
    auto owner_ = GetGameObject();
    if (!owner_) return false;

    if (!ikMap)
    {
        ikMap = owner_->GetComponent<IKMapComponent>().get();
    }
    if (!model)
    {
        model = owner_->GetComponent<GltfModelCommponennt>().get();
    }

    return ikMap && model;
}

void SpringBoneSimulationComponent::LateUpdate(float deltaTime_)
{
    BS_PROFILE_SCOPE("SpringBoneSimulationComponent::LateUpdate");

    // アニメーション姿勢が更新された後に、Spring BoneとCloth Boneを解いてモデル姿勢へ反映する。
    if (!ResolveDependencies()) return;
    if (model) model->UpdateLoadState();
    if (!model || !model->IsModelReady()) return;

    const float safeDeltaTime_ = std::clamp(deltaTime_, 0.0f, 1.0f / 20.0f);

    {
        BS_PROFILE_SCOPE("SpringBone::SpringChains");

        for (auto& chain_ : ikMap->GetChains())
        {
            if (!chain_.enabled || chain_.type != IKMapComponent::IK_TYPE::SPRING_BONE) continue;
            if (!ResolveChain(chain_)) continue;

            // 通常の髪/尻尾チェーンは、点列シミュレーション後すぐにボーン回転へ戻す。
            {
                BS_PROFILE_SCOPE("SpringBone::SimulateSpringChain");
                SimulateSpringChain(chain_, safeDeltaTime_);
            }

            {
                BS_PROFILE_SCOPE("SpringBone::ApplySpringChain");
                ApplySpringChain(chain_);
            }
        }
    }

    bool hasClothChains_ = false;

    {
        BS_PROFILE_SCOPE("SpringBone::ClothChainsSimulate");

        for (auto& chain_ : ikMap->GetChains())
        {
            if (!chain_.enabled || chain_.type != IKMapComponent::IK_TYPE::CLOTH_BONE) continue;
            if (!ResolveChain(chain_)) continue;

            // Cloth Boneは複数チェーン間の横/斜め制約を後でまとめて解くため、ここでは点列だけ先に更新する。
            SimulateSpringChain(chain_, safeDeltaTime_);
            hasClothChains_ = true;
        }
    }

    if (hasClothChains_)
    {
        {
            BS_PROFILE_SCOPE("SpringBone::SolveClothConstraints");
            SolveClothConstraints();
        }

        {
            BS_PROFILE_SCOPE("SpringBone::ApplyClothChains");

            for (auto& chain_ : ikMap->GetChains())
            {
                if (!chain_.enabled || chain_.type != IKMapComponent::IK_TYPE::CLOTH_BONE || !chain_.initialized) continue;
                ApplySpringChain(chain_);
            }
        }
    }

    {
        BS_PROFILE_SCOPE("SpringBone::DebugDraw");
        DrawBodyCollisionShapes();
        DrawHairCollisionShapes();
    }

    if (ikMap->ConsumeSimulationWarmupRequest())
    {
        BS_PROFILE_SCOPE("SpringBone::WarmupReset");

        // IKMap側でパラメータやコライダーが変わったら、現在姿勢からシミュレーションを作り直す。
        for (auto& chain_ : ikMap->GetChains())
        {
            chain_.initialized = false;
        }

        clothConstraints.clear();
        clothConstraintsInitialized = false;
    }
}

bool SpringBoneSimulationComponent::ResolveChain(IKMapComponent::IKChain& chain_)
{
    // root/tip名を親方向へたどり、解決対象のボーンindex列をキャッシュする。
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

void SpringBoneSimulationComponent::SimulateSpringChain(IKMapComponent::IKChain& chain_, float deltaTime_)
{
    // 1本のSpring/ClothチェーンをVerlet風に更新し、距離制約と衝突で破綻を抑える。
    const auto& nodes_ = model->GetNodes();
    if (!chain_.initialized
        || chain_.simulatedPositions.size() != chain_.nodeIndices.size()
        || chain_.segmentLengths.size() + 1 != chain_.nodeIndices.size())
    {
        // 初期化時は現在ボーン位置から点列を作り、隣接距離と曲げ距離を以後の制約基準として保存する。
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
            // 開始直後に体へめり込まないよう、数回だけ衝突と距離制約を先に回す。
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
    std::vector<XMFLOAT3> currentWorldPositions_;
    currentWorldPositions_.reserve(chain_.nodeIndices.size());
    for (const int nodeIndex_ : chain_.nodeIndices)
    {
        currentWorldPositions_.push_back(GetTranslation(nodes_.at(nodeIndex_).worldTransform));
    }

    const XMFLOAT3 previousRootPosition_ = GetTranslation(chain_.previousRootTransform);
    const XMFLOAT3 currentRootPosition_ = GetTranslation(currentRootTransform_);
    const XMFLOAT3 rootTranslation_ = Subtract(currentRootPosition_, previousRootPosition_);
    float chainLength_ = 0.0f;
    for (const float segmentLength_ : chain_.segmentLengths)
    {
        chainLength_ += segmentLength_;
    }

    const float teleportDistance_ = (std::max)(0.5f, chainLength_ * 1.5f);
    if (Length(rootTranslation_) > teleportDistance_)
    {
        // 大きなワープ移動は速度として扱わず、シミュレーション点も同じ量だけ移動して暴れを防ぐ。
        for (size_t pointIndex_ = 0; pointIndex_ < chain_.simulatedPositions.size(); ++pointIndex_)
        {
            chain_.simulatedPositions[pointIndex_] = Add(
                chain_.simulatedPositions[pointIndex_], rootTranslation_);
            chain_.previousPositions[pointIndex_] = Add(
                chain_.previousPositions[pointIndex_], rootTranslation_);
        }
    }
    chain_.previousRootTransform = currentRootTransform_;

    const XMFLOAT3 animatedRoot_ = GetTranslation(currentRootTransform_);
    const int substeps_ = std::clamp(chain_.solverSubsteps, 1, 8);
    const int iterations_ = std::clamp(chain_.solverIterations, 1, 16);
    const float substepDeltaTime_ = deltaTime_ / static_cast<float>(substeps_);
    const float substepDamping_ = std::pow(std::clamp(chain_.damping, 0.0f, 0.999f), 1.0f / substeps_);

    for (int substepIndex_ = 0; substepIndex_ < substeps_; ++substepIndex_)
    {
        // 根元側はアニメーションへ固定し、先端側ほど慣性、風、リフトを効かせる。
        chain_.simulatedPositions.front() = animatedRoot_;
        chain_.previousPositions.front() = animatedRoot_;
        for (size_t pointIndex_ = chain_.simulatedPositions.size(); pointIndex_-- > 1;)
        {
            const XMFLOAT3 current_ = chain_.simulatedPositions[pointIndex_];
            const float normalizedPoint_ = GetNormalizedPointPosition(chain_, pointIndex_);
            if (pointIndex_ <= 1 || normalizedPoint_ <= 0.15f)
            {
                chain_.simulatedPositions[pointIndex_] = currentWorldPositions_[pointIndex_];
                chain_.previousPositions[pointIndex_] = currentWorldPositions_[pointIndex_];
                continue;
            }

            const float tipWeight_ = SmoothStep(normalizedPoint_);
            const float middleFlexWeight_ = GetMiddleFlexWeight(normalizedPoint_);
            const float pointDamping_ = std::clamp(
                substepDamping_ + 0.08f * tipWeight_ + 0.05f * middleFlexWeight_ - 0.04f * (1.0f - tipWeight_),
                0.0f,
                0.995f);
            const XMFLOAT3 rawVelocity_ = Scale(Subtract(current_, chain_.previousPositions[pointIndex_]), pointDamping_);
            const float baseSegmentLength_ = chain_.segmentLengths.empty()
                ? 0.1f
                : chain_.segmentLengths[(std::min)(pointIndex_ - 1, chain_.segmentLengths.size() - 1)];
            const XMFLOAT3 velocity_ = ClampLength(
                rawVelocity_,
                baseSegmentLength_ * (0.65f + 0.55f * tipWeight_ + 0.25f * middleFlexWeight_));
            const XMFLOAT3 targetWorldPosition_ = currentWorldPositions_[pointIndex_];
            const float stiffnessScale_ = GetStiffnessScale(chain_, pointIndex_);
            const float pointStiffness_ = std::clamp(chain_.stiffness * stiffnessScale_, 0.0f, 1.0f);
            const float substepStiffness_ = 1.0f - std::pow(1.0f - pointStiffness_, 1.0f / substeps_);
            const float followScale_ = 1.0f - tipWeight_;
            const XMFLOAT3 animationPull_ = Scale(
                Subtract(targetWorldPosition_, current_),
                substepStiffness_ * followScale_);
            const XMFLOAT3 gravity_ = { 0.0f, 0.0f, 0.0f };
            const float liftAcceleration_ = (std::max)(chain_.startLift, 0.0f) * 20.0f;
            const XMFLOAT3 lift_ = {
                0.0f,
                liftAcceleration_ * tipWeight_ * substepDeltaTime_ * substepDeltaTime_,
                0.0f
            };

            XMFLOAT3 wind_ = { 0.0f, 0.0f, 0.0f };
            if (windEnabled && windStrength > 0.0f)
            {
                const float windScale_ = windStrength * (0.25f + 0.75f * tipWeight_) * substepDeltaTime_ * substepDeltaTime_;
                wind_ = Scale(windDirection, windScale_);
            }

            chain_.previousPositions[pointIndex_] = current_;
            chain_.simulatedPositions[pointIndex_] = Add(
                Add(Add(Add(Add(current_, velocity_), animationPull_), gravity_), lift_),
                wind_);
        }

        for (int iterationIndex_ = 0; iterationIndex_ < iterations_; ++iterationIndex_)
        {
            // 距離制約、曲げ制約、体コリジョン、前方はみ出し制限を反復して安定させる。
            chain_.simulatedPositions.front() = animatedRoot_;
            for (size_t pointIndex_ = 1; pointIndex_ < chain_.simulatedPositions.size(); ++pointIndex_)
            {
                const float normalizedPoint_ = GetNormalizedPointPosition(chain_, pointIndex_);
                const float middleFlexWeight_ = GetMiddleFlexWeight(normalizedPoint_);
                const float segmentStiffness_ = chain_.type == IKMapComponent::IK_TYPE::CLOTH_BONE
                    ? 1.0f
                    : std::clamp(0.90f - 0.16f * SmoothStep(normalizedPoint_) - 0.16f * middleFlexWeight_, 0.58f, 1.0f);
                SolveDistanceConstraint(
                    chain_.simulatedPositions, pointIndex_ - 1, pointIndex_,
                    chain_.segmentLengths[pointIndex_ - 1], segmentStiffness_);
            }
            for (size_t pointIndex_ = 2; pointIndex_ < chain_.simulatedPositions.size(); ++pointIndex_)
            {
                SolveDistanceConstraint(
                    chain_.simulatedPositions, pointIndex_ - 2, pointIndex_,
                    chain_.bendLengths[pointIndex_ - 2],
                    std::clamp(
                        chain_.bendStiffness * GetStiffnessScale(chain_, pointIndex_)
                        * (1.0f - 0.35f * GetMiddleFlexWeight(GetNormalizedPointPosition(chain_, pointIndex_))),
                        0.0f,
                        1.0f));
            }
            if (chain_.collideWithBodyColliders)
            {
                for (size_t pointIndex_ = 1; pointIndex_ < chain_.simulatedPositions.size(); ++pointIndex_)
                {
                    ResolveBodyCollision(chain_, pointIndex_);
                }
            }

            if (chain_.type == IKMapComponent::IK_TYPE::SPRING_BONE
                || chain_.type == IKMapComponent::IK_TYPE::CLOTH_BONE)
            {
                ConstrainHairBehindBody(chain_);
            }
        }
    }

    for (size_t pointIndex_ = 0; pointIndex_ < chain_.simulatedPositions.size(); ++pointIndex_)
    {
        if (pointIndex_ > 1 && GetNormalizedPointPosition(chain_, pointIndex_) > 0.35f) break;
        chain_.simulatedPositions[pointIndex_] = currentWorldPositions_[pointIndex_];
        chain_.previousPositions[pointIndex_] = currentWorldPositions_[pointIndex_];
    }

}

void SpringBoneSimulationComponent::BuildClothConstraints()
{
    // 同じCloth GroupのチェーンをOrder順に並べ、隣接チェーン同士を横/斜めにつなぐ制約を作る。
    clothConstraints.clear();

    std::map<int, std::vector<size_t>> groupChainIndices_;
    for (size_t chainIndex_ = 0; chainIndex_ < ikMap->GetChains().size(); ++chainIndex_)
    {
        const auto& chain_ = ikMap->GetChains()[chainIndex_];
        if (!chain_.enabled || chain_.type != IKMapComponent::IK_TYPE::CLOTH_BONE || !chain_.initialized) continue;
        groupChainIndices_[chain_.clothGroup].push_back(chainIndex_);
    }

    for (auto& [groupIndex_, chainIndices_] : groupChainIndices_)
    {
        (void)groupIndex_;
        std::stable_sort(
            chainIndices_.begin(), chainIndices_.end(),
            [this](size_t chainIndexA_, size_t chainIndexB_)
            {
                return ikMap->GetChains()[chainIndexA_].clothOrder < ikMap->GetChains()[chainIndexB_].clothOrder;
            });

        for (size_t pairIndex_ = 1; pairIndex_ < chainIndices_.size(); ++pairIndex_)
        {
            const size_t chainIndexA_ = chainIndices_[pairIndex_ - 1];
            const size_t chainIndexB_ = chainIndices_[pairIndex_];
            const auto& positionsA_ = ikMap->GetChains()[chainIndexA_].simulatedPositions;
            const auto& positionsB_ = ikMap->GetChains()[chainIndexB_].simulatedPositions;
            const size_t pointCount_ = (std::min)(positionsA_.size(), positionsB_.size());
            if (pointCount_ < 2) continue;

            // ボーン数が違うチェーン同士でも先端まで自然につながるよう、正規化した段で対応点を取る。
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

void SpringBoneSimulationComponent::SolveClothConstraints()
{
    // 布制約は全Clothチェーンの最大solverIterationsに合わせて繰り返し、束全体の形を保つ。
    if (!clothConstraintsInitialized)
    {
        BuildClothConstraints();
    }

    int maximumIterations_ = 1;
    for (const auto& chain_ : ikMap->GetChains())
    {
        if (!chain_.enabled || chain_.type != IKMapComponent::IK_TYPE::CLOTH_BONE) continue;
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

void SpringBoneSimulationComponent::SolveClothConstraint(const ClothConstraint& constraint_)
{
    // 横/斜め制約ごとの剛性を使い、2本のチェーンの対応点を目標距離へ戻す。
    if (constraint_.chainIndexA >= ikMap->GetChains().size() || constraint_.chainIndexB >= ikMap->GetChains().size()) return;

    auto& chainA_ = ikMap->GetChains()[constraint_.chainIndexA];
    auto& chainB_ = ikMap->GetChains()[constraint_.chainIndexB];
    if (!chainA_.enabled || !chainB_.enabled
        || chainA_.type != IKMapComponent::IK_TYPE::CLOTH_BONE || chainB_.type != IKMapComponent::IK_TYPE::CLOTH_BONE) return;
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

void SpringBoneSimulationComponent::ResolveBodyCollision(IKMapComponent::IKChain& chain_, size_t pointIndex_)
{
    // 先端側のシミュレーション点だけを体コライダー外へ押し戻し、根元のアニメーション追従は保つ。
    if (pointIndex_ <= 1 || GetNormalizedPointPosition(chain_, pointIndex_) <= 0.35f) return;

    auto& position_ = chain_.simulatedPositions[pointIndex_];
    const auto& nodes_ = model->GetNodes();
    const XMFLOAT3 animatedPosition_ =
        GetTranslation(nodes_.at(chain_.nodeIndices[pointIndex_]).worldTransform);

    for (auto& sphere_ : ikMap->GetBodyCollisionSpheres())
    {
        XMFLOAT3 center_;
        if (!sphere_.enabled || !GetBodyCollisionSphereCenter(sphere_, center_)) continue;

        const XMFLOAT3 difference_ = Subtract(position_, center_);
        const float distance_ = Length(difference_);
        const float minimumDistance_ = sphere_.radius + chain_.collisionRadius;
        if (distance_ >= minimumDistance_) continue;

        XMFLOAT3 direction_ = Normalize(difference_);
        if (distance_ <= 0.00001f)
        {
            direction_ = Normalize(Subtract(animatedPosition_, center_));
            if (Length(direction_) <= 0.00001f)
            {
                direction_ = Normalize(Subtract(chain_.previousPositions[pointIndex_], center_));
            }
            if (Length(direction_) <= 0.00001f) direction_ = { 0.0f, 0.0f, 1.0f };
        }
        position_ = Add(center_, Scale(direction_, minimumDistance_));
        chain_.previousPositions[pointIndex_] = position_;
    }

    for (size_t sphereIndex_ = 0; sphereIndex_ + 1 < ikMap->GetBodyCollisionSpheres().size(); ++sphereIndex_)
    {
        auto& sphereA_ = ikMap->GetBodyCollisionSpheres()[sphereIndex_];
        auto& sphereB_ = ikMap->GetBodyCollisionSpheres()[sphereIndex_ + 1];
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
        if (distance_ <= 0.00001f)
        {
            direction_ = Normalize(Subtract(animatedPosition_, closestPoint_));
            if (Length(direction_) <= 0.00001f)
            {
                direction_ = Normalize(Subtract(chain_.previousPositions[pointIndex_], closestPoint_));
            }
            if (Length(direction_) <= 0.00001f) direction_ = { 0.0f, 0.0f, 1.0f };
        }
        position_ = Add(closestPoint_, Scale(direction_, minimumDistance_));
        chain_.previousPositions[pointIndex_] = position_;
    }
}

bool SpringBoneSimulationComponent::GetBodyCollisionSphereCenter(IKMapComponent::BodyCollisionSphere& sphere_, XMFLOAT3& center_)
{
    // IKMapのコライダー設定はノード名とローカルオフセットで持つため、初回だけノードindexを解決して使う。
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

void SpringBoneSimulationComponent::DrawBodyCollisionShapes()
{
    // 調整用に体コライダーを可視化する。連結指定がある球同士は途中のサンプル球も描く。
    if (!ikMap || !showBodyCollisionShapes) return;

    auto& renderer_ = GraphicsDeviceManager::Instance().GetShapeRendererSystem();
    for (auto& sphere_ : ikMap->GetBodyCollisionSpheres())
    {
        XMFLOAT3 center_;
        if (!GetBodyCollisionSphereCenter(sphere_, center_)) continue;

        XMFLOAT4 color_ = bodyCollisionShapeColor;
        if (!sphere_.enabled) color_.w *= 0.25f;
        renderer_.DrawSphere(center_, sphere_.radius, color_);
    }

    constexpr int connectionSamples_ = 6;
    for (size_t sphereIndex_ = 0; sphereIndex_ + 1 < ikMap->GetBodyCollisionSpheres().size(); ++sphereIndex_)
    {
        auto& sphereA_ = ikMap->GetBodyCollisionSpheres()[sphereIndex_];
        auto& sphereB_ = ikMap->GetBodyCollisionSpheres()[sphereIndex_ + 1];
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

void SpringBoneSimulationComponent::DrawHairCollisionShapes()
{
    // シミュレーション済み点列を線として描き、髪/布がどの形に解けているか確認する。
    if (!showHairCollisionShapes) return;
    if (!model) return;

    auto& renderer_ = GraphicsDeviceManager::Instance().GetShapeRendererSystem();

    for (const auto& chain_ : ikMap->GetChains())
    {
        if (!chain_.enabled
            || (chain_.type != IKMapComponent::IK_TYPE::SPRING_BONE && chain_.type != IKMapComponent::IK_TYPE::CLOTH_BONE)) continue;
        if (!chain_.initialized) continue;
        if (chain_.simulatedPositions.empty()) continue;

        for (size_t pointIndex_ = 1; pointIndex_ < chain_.simulatedPositions.size(); ++pointIndex_)
        {
            DrawBoneBetween(
                renderer_,
                chain_.simulatedPositions[pointIndex_ - 1],
                chain_.simulatedPositions[pointIndex_],
                hairCollisionShapeColor);
        }
    }
}

void SpringBoneSimulationComponent::ConstrainHairBehindBody(IKMapComponent::IKChain& chain_)
{
    // 髪や布がキャラクター前方へ出すぎないよう、根元から見た前方向距離を制限する。
    if (!model || chain_.simulatedPositions.size() < 2) return;

    auto owner_ = GetGameObject();
    if (!owner_ || !owner_->transform) return;

    const XMMATRIX ownerWorld_ = owner_->transform->GetWorldMatrix();

    XMFLOAT3 ownerForward_;
    XMStoreFloat3(&ownerForward_, XMVector3Normalize(ownerWorld_.r[2]));

    const XMFLOAT3 rootPosition_ = chain_.simulatedPositions.front();

    // 前側にどれくらい出ることを許可するか
    const float allowedForwardDistance_ = 0.02f;
    const float correctionRate_ = 1.0f;

    // 根元付近はアニメーション追従なので制限しすぎない
    for (size_t pointIndex_ = 2; pointIndex_ < chain_.simulatedPositions.size(); ++pointIndex_)
    {
        const float normalizedPoint_ = GetNormalizedPointPosition(chain_, pointIndex_);
        if (normalizedPoint_ <= 0.35f) continue;

        XMFLOAT3& position_ = chain_.simulatedPositions[pointIndex_];

        const XMFLOAT3 rootToPoint_ = Subtract(position_, rootPosition_);
        const float forwardDistance_ =
            rootToPoint_.x * ownerForward_.x +
            rootToPoint_.y * ownerForward_.y +
            rootToPoint_.z * ownerForward_.z;

        if (forwardDistance_ <= allowedForwardDistance_) continue;

        const float pushBackDistance_ = forwardDistance_ - allowedForwardDistance_;
        position_ = Subtract(position_, Scale(ownerForward_, pushBackDistance_ * correctionRate_));

        chain_.previousPositions[pointIndex_] = position_;
    }
}

void SpringBoneSimulationComponent::ApplySpringChain(IKMapComponent::IKChain& chain_)
{
    BS_PROFILE_SCOPE("SpringBone::ApplySpringChain");

    // 解けた点列の方向へ各ボーンを回転し、ワールド方向の差分をローカル回転へ戻してモデルに反映する。
    auto& nodes_ = model->GetNodes();
    std::vector<XMFLOAT3> targetDirections_;
    targetDirections_.reserve(chain_.simulatedPositions.size() - 1);
    for (size_t pointIndex_ = 0; pointIndex_ + 1 < chain_.simulatedPositions.size(); ++pointIndex_)
    {
        targetDirections_.push_back(Normalize(Subtract(
            chain_.simulatedPositions[pointIndex_ + 1],
            chain_.simulatedPositions[pointIndex_])));
    }

    bool chainChanged_ = false;
    for (size_t nodeIndex_ = 0; nodeIndex_ + 1 < chain_.nodeIndices.size(); ++nodeIndex_)
    {
        model->UpdateNodeTransformOnly(chain_.nodeIndices[nodeIndex_]);
        model->UpdateNodeTransformOnly(chain_.nodeIndices[nodeIndex_ + 1]);
        auto& node_ = nodes_.at(chain_.nodeIndices[nodeIndex_]);
        const auto& child_ = nodes_.at(chain_.nodeIndices[nodeIndex_ + 1]);

        const XMFLOAT3 currentPosition_ = GetTranslation(node_.worldTransform);
        const XMFLOAT3 childPosition_ = GetTranslation(child_.worldTransform);
        const XMFLOAT3 currentDirection_ = Normalize(Subtract(childPosition_, currentPosition_));
        const XMFLOAT3 targetDirection_ = targetDirections_[nodeIndex_];
        if (Length(currentDirection_) <= 0.00001f || Length(targetDirection_) <= 0.00001f) continue;

        const XMVECTOR currentDirectionVector_ = XMLoadFloat3(&currentDirection_);
        const XMVECTOR targetDirectionVector_ = XMLoadFloat3(&targetDirection_);
        const XMVECTOR axis_ = XMVector3Cross(currentDirectionVector_, targetDirectionVector_);
        const float axisLength_ = XMVectorGetX(XMVector3Length(axis_));
        const float dot_ = std::clamp(XMVectorGetX(XMVector3Dot(currentDirectionVector_, targetDirectionVector_)), -1.0f, 1.0f);
        if (axisLength_ <= 0.00001f) continue;

        const float normalizedNodePosition_ = static_cast<float>(nodeIndex_)
            / static_cast<float>(chain_.nodeIndices.size() - 1);
        const float simulationWeight_ = SmoothStep(normalizedNodePosition_);
        if (simulationWeight_ <= 0.00001f) continue;

        const float angle_ = std::acos(dot_)
            * std::clamp(chain_.influence, 0.0f, 1.0f)
            * simulationWeight_;
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
            {
                BS_PROFILE_SCOPE("SpringBone::UpdateNodeTransformFrom");
                model->UpdateNodeTransformOnly(chain_.nodeIndices[nodeIndex_]);
            }
            chainChanged_ = true;
        }
    }

    if (chainChanged_)
    {
        BS_PROFILE_SCOPE("SpringBone::FinalizeChainTransform");
        model->UpdateNodeTransformFrom(chain_.nodeIndices.front());
    }
}


void SpringBoneSimulationComponent::Gui()
{
    // このコンポーネントでは実行時シミュレーションの表示と風テストだけを扱い、チェーン設定はIKMap側に置く。
    ImGui::SeparatorText("Spring Bone Simulation");
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

    ImGui::SeparatorText("Wind (Test)");
    ImGui::Checkbox("Enable Wind", &windEnabled);
    if (windEnabled)
    {
        ImGui::DragFloat("Wind Strength", &windStrength, 0.1f, 0.0f, 200.0f, "%.2f");
        if (ImGui::DragFloat3("Wind Direction", &windDirection.x, 0.01f, -1.0f, 1.0f))
        {
            const float length_ = std::sqrt(
                windDirection.x * windDirection.x +
                windDirection.y * windDirection.y +
                windDirection.z * windDirection.z);
            if (length_ > 0.0001f)
            {
                windDirection.x /= length_;
                windDirection.y /= length_;
                windDirection.z /= length_;
            }
        }
        if (ImGui::Button("Stop Wind")) windStrength = 0.0f;
    }

    if (ImGui::Button("Reset Simulation"))
    {
        ResetSimulation();
    }
}

void SpringBoneSimulationComponent::Serialize(nlohmann::json& json_) const
{
    // シミュレーションそのものではなく、デバッグ表示設定だけを保存する。
    json_["ShowBodyCollisionShapes"] = showBodyCollisionShapes;
    json_["ShowHairCollisionShapes"] = showHairCollisionShapes;
    json_["BodyCollisionShapeColor"] = {
        bodyCollisionShapeColor.x,
        bodyCollisionShapeColor.y,
        bodyCollisionShapeColor.z,
        bodyCollisionShapeColor.w
    };
    json_["HairCollisionShapeColor"] = {
        hairCollisionShapeColor.x,
        hairCollisionShapeColor.y,
        hairCollisionShapeColor.z,
        hairCollisionShapeColor.w
    };
}

void SpringBoneSimulationComponent::Deserialize(const nlohmann::json& json_)
{
    // 表示設定を復元する。チェーン定義と物理パラメータはIKMapComponent側で読み込まれる。
    showBodyCollisionShapes = json_.value("ShowBodyCollisionShapes", true);
    showHairCollisionShapes = json_.value("ShowHairCollisionShapes", false);
    if (json_.contains("BodyCollisionShapeColor"))
    {
        bodyCollisionShapeColor = {
            json_["BodyCollisionShapeColor"][0],
            json_["BodyCollisionShapeColor"][1],
            json_["BodyCollisionShapeColor"][2],
            json_["BodyCollisionShapeColor"][3]
        };
    }
    if (json_.contains("HairCollisionShapeColor"))
    {
        hairCollisionShapeColor = {
            json_["HairCollisionShapeColor"][0],
            json_["HairCollisionShapeColor"][1],
            json_["HairCollisionShapeColor"][2],
            json_["HairCollisionShapeColor"][3]
        };
    }
}

void SpringBoneSimulationComponent::ResetSimulation()
{
    // 布制約、コライダーindex、各チェーンの点列履歴を破棄し、次回更新で現在姿勢から初期化する。
    clothConstraints.clear();
    clothConstraintsInitialized = false;
    if (!ResolveDependencies()) return;

    for (auto& sphere_ : ikMap->GetBodyCollisionSpheres())
    {
        sphere_.nodeIndex = -1;
    }
    for (auto& chain_ : ikMap->GetChains())
    {
        chain_.nodeIndices.clear();
        chain_.segmentLengths.clear();
        chain_.bendLengths.clear();
        chain_.simulatedPositions.clear();
        chain_.previousPositions.clear();
        chain_.initialized = false;
    }
}

REGISTER_COMPONENT(SpringBoneSimulationComponent, "SpringBoneSimulationComponent")
