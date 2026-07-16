#include "PrecompiledHeader.h"

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
    constexpr float EPSILON = 0.00001f; // 極小値判定用の許容値。

    XMFLOAT3 GetTranslation(const XMFLOAT4X4& matrix_) // 行列から平行移動成分を取得する。
    {
        return { matrix_._41, matrix_._42, matrix_._43 };
    }

    XMFLOAT3 Add(const XMFLOAT3& lhs_, const XMFLOAT3& rhs_) // 2つのベクトルを加算する。
    {
        return { lhs_.x + rhs_.x, lhs_.y + rhs_.y, lhs_.z + rhs_.z };
    }

    XMFLOAT3 Subtract(const XMFLOAT3& lhs_, const XMFLOAT3& rhs_) // 2つのベクトルを減算する。
    {
        return { lhs_.x - rhs_.x, lhs_.y - rhs_.y, lhs_.z - rhs_.z };
    }

    XMFLOAT3 Scale(const XMFLOAT3& value_, float scale_) // ベクトルを指定倍率にする。
    {
        return { value_.x * scale_, value_.y * scale_, value_.z * scale_ };
    }

    float Length(const XMFLOAT3& value_) // ベクトルの長さを計算する。
    {
        return std::sqrt(value_.x * value_.x + value_.y * value_.y + value_.z * value_.z);
    }

    XMFLOAT3 Normalize(const XMFLOAT3& value_) // ベクトルを正規化する。
    {
        const float length_ = Length(value_);
        return length_ > EPSILON ? Scale(value_, 1.0f / length_) : XMFLOAT3{};
    }

    XMFLOAT3 Lerp(const XMFLOAT3& from_, const XMFLOAT3& to_, float amount_) // 2点間を線形補間する。
    {
        return Add(from_, Scale(Subtract(to_, from_), std::clamp(amount_, 0.0f, 1.0f)));
    }

    XMFLOAT3 ClampFootPositionAboveGround(
        const XMFLOAT3& footPosition_,
        const XMFLOAT3& groundPosition_,
        bool grounded_)
    {
        if (!grounded_) return footPosition_;

        XMFLOAT3 result_ = footPosition_;

        // QueryGround側で footHeightOffset はすでに加算済みなので、
        // ここでは地面目標位置より下に行かないようにする。
        if (result_.y < groundPosition_.y)
        {
            result_.y = groundPosition_.y;
        }

        return result_;
    }

    // 2点間をボーン形状で描画する。
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

        // Z軸向きの描画形状を2点間の方向へ回転させる。
        if (dot_ < 1.0f - EPSILON)
        {
            XMVECTOR axis_ = XMVector3Cross(forward_, direction_);
            if (XMVectorGetX(XMVector3LengthSq(axis_)) <= EPSILON)
            {
                axis_ = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            }
            transform_ = XMMatrixRotationAxis(XMVector3Normalize(axis_), std::acos(dot_));
        }

        // 描画開始位置を設定する。
        transform_.r[3] = XMVectorSet(
            startPosition_.x,
            startPosition_.y,
            startPosition_.z,
            1.0f);

        XMFLOAT4X4 transformStorage_;
        XMStoreFloat4x4(&transformStorage_, transform_);
        renderer_.DrawBone(transformStorage_, length_, color_);
    }

    // 足IKの地面Rayから自分自身のColliderを除外するFilter。
    class FootQueryFilter : public PxQueryFilterCallback
    {
    public:
        explicit FootQueryFilter(const std::vector<const void*>& ignoredUserData_)
            : ignoredUserData(ignoredUserData_)
        {
        }

        // Rayに当ててよいShapeかを判定する。
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

            // Owner自身のColliderは足IKの接地判定から除外する。
            if (std::find(ignoredUserData.begin(), ignoredUserData.end(), actor_->userData)
                != ignoredUserData.end())
            {
                return PxQueryHitType::eNONE;
            }

            return PxQueryHitType::eBLOCK;
        }

        // 有効なHitをBlockとして扱う。
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
        const std::vector<const void*>& ignoredUserData; // 除外するuserData一覧。
    };
}

// Component名を返す。
const std::string& FootIKComponent::GetName() const
{
    static const std::string name_ = "FootIKComponent";
    return name_;
}

// 必要Componentを取得し、左右のFootチェーンを作成する。
void FootIKComponent::Start()
{
    if (!ResolveDependencies()) return;
    const std::shared_ptr<IKMapComponent> ikMap_ = ikMap.lock();
    if (!ikMap_) return;
    ikMap_->AddFootChain("FootL");
    ikMap_->AddFootChain("FootR");
}

bool FootIKComponent::IsLoadReady() const
{
    const auto owner_ = const_cast<FootIKComponent*>(this)->GetGameObject();
    const auto model_ = owner_ ? owner_->GetComponent<GltfModelCommponennt>() : nullptr;
    if (!model_) return true;

    model_->UpdateLoadState();
    return !model_->IsModelLoading();
}

// OwnerからIKMapComponentとモデルComponentを取得する。
bool FootIKComponent::ResolveDependencies()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return false;

    if (ikMap.expired()) ikMap = owner_->GetComponent<IKMapComponent>();
    if (model.expired()) model = owner_->GetComponent<GltfModelCommponennt>();

    return !ikMap.expired() && !model.expired();
}

// アニメーション後の足位置に対して骨盤補正とFootIKを適用する。
void FootIKComponent::LateUpdate(float deltaTime_)
{
    BS_PROFILE_SCOPE("FootIKComponent::LateUpdate");

    if (!enabled || !ResolveDependencies()) return;
    const std::shared_ptr<GltfModelCommponennt> modelReady_ = model.lock();
    if (modelReady_) modelReady_->UpdateLoadState();
    if (!modelReady_ || !modelReady_->IsModelReady()) return;

    const std::shared_ptr<IKMapComponent> ikMap_ = ikMap.lock();
    if (!ikMap_) return;
    std::vector<IKMapComponent::IKChain>& chains_ = ikMap_->GetChains();
    if (footStates.size() != chains_.size()) footStates.resize(chains_.size());

    std::vector<IKMapComponent::IKChain*> footChains_;

    // 有効なFootチェーンを集める。
    for (size_t chainIndex_ = 0; chainIndex_ < chains_.size(); ++chainIndex_)
    {
        IKMapComponent::IKChain& chain_ = chains_[chainIndex_];
        if (!chain_.enabled || chain_.type != IKMapComponent::IK_TYPE::FOOT) continue;
        if (ResolveChain(chain_)) footChains_.push_back(&chain_);
    }

    // 足が届くように先に骨盤高さを補正する。
    ApplyPelvisAdjustment(footChains_, deltaTime_);

    // 各Footチェーンを地面に追従させる。
    for (size_t chainIndex_ = 0; chainIndex_ < chains_.size(); ++chainIndex_)
    {
        IKMapComponent::IKChain& chain_ = chains_[chainIndex_];
        if (!chain_.enabled || chain_.type != IKMapComponent::IK_TYPE::FOOT) continue;
        if (!chain_.nodeIndices.empty()) SolveChain(chain_, footStates[chainIndex_], deltaTime_);
    }
}

// 複数の足チェーンに共通する親Nodeを探す。
int FootIKComponent::FindCommonAncestor(
    const std::vector<IKMapComponent::IKChain*>& footChains_) const
{
    if (footChains_.size() < 2) return -1;

    const std::shared_ptr<GltfModelCommponennt> model_ = model.lock();
    if (!model_) return -1;
    const std::vector<GltfModelCommponennt::Node>& nodes_ = model_->GetNodes();
    std::vector<int> ancestors_;

    // 最初のFootチェーンのRootから親を辿る。
    for (int nodeIndex_ = footChains_.front()->nodeIndices.front(); nodeIndex_ >= 0;
        nodeIndex_ = nodes_.at(nodeIndex_).parentIndex)
    {
        ancestors_.push_back(nodeIndex_);
    }

    // すべてのFootチェーンが共有している親Nodeを探す。
    for (const int candidateIndex_ : ancestors_)
    {
        bool sharedByAll_ = true;

        for (size_t chainIndex_ = 1; chainIndex_ < footChains_.size(); ++chainIndex_)
        {
            bool found_ = false;

            for (int nodeIndex_ = footChains_[chainIndex_]->nodeIndices.front(); nodeIndex_ >= 0;
                nodeIndex_ = nodes_.at(nodeIndex_).parentIndex)
            {
                if (nodeIndex_ == candidateIndex_)
                {
                    found_ = true;
                    break;
                }
            }

            if (!found_)
            {
                sharedByAll_ = false;
                break;
            }
        }

        if (sharedByAll_) return candidateIndex_;
    }

    return -1;
}

// 足が地面に届くように必要な骨盤下降量を計算する。
void FootIKComponent::ApplyPelvisAdjustment(
    const std::vector<IKMapComponent::IKChain*>& footChains_,
    float deltaTime_)
{
    if (footChains_.empty()) return;
    const std::shared_ptr<GltfModelCommponennt> model_ = model.lock();
    if (!model_) return;

    const int pelvisNodeIndex_ = FindCommonAncestor(footChains_);
    if (pelvisNodeIndex_ < 0) return;

    float requiredDrop_ = 0.0f;
    const std::vector<GltfModelCommponennt::Node>& nodes_ = model_->GetNodes();

    if (adjustPelvisHeight)
    {
        for (const IKMapComponent::IKChain* chain_ : footChains_)
        {
            if (!chain_ || chain_->nodeIndices.size() < 2) continue;

            const XMFLOAT3 rootPosition_ = GetTranslation(
                nodes_.at(chain_->nodeIndices.front()).worldTransform);
            const XMFLOAT3 footPosition_ = GetTranslation(
                nodes_.at(chain_->nodeIndices.back()).worldTransform);

            XMFLOAT3 hitPosition_{};
            XMFLOAT3 hitNormal_{};
            if (!QueryGround(*chain_, footPosition_, hitPosition_, hitNormal_)) continue;

            float legLength_ = 0.0f;

            // チェーン全体の足の長さを計算する。
            for (size_t pointIndex_ = 1; pointIndex_ < chain_->nodeIndices.size(); ++pointIndex_)
            {
                const XMFLOAT3 previousPosition_ = GetTranslation(
                    nodes_.at(chain_->nodeIndices[pointIndex_ - 1]).worldTransform);
                const XMFLOAT3 currentPosition_ = GetTranslation(
                    nodes_.at(chain_->nodeIndices[pointIndex_]).worldTransform);

                legLength_ += Length(Subtract(currentPosition_, previousPosition_));
            }

            // 水平方向の距離を引いた残りを垂直方向の到達距離として扱う。
            const float horizontalX_ = hitPosition_.x - rootPosition_.x;
            const float horizontalZ_ = hitPosition_.z - rootPosition_.z;
            const float horizontalDistanceSquared_ =
                horizontalX_ * horizontalX_ + horizontalZ_ * horizontalZ_;

            const float usableLength_ = (std::max)(legLength_ - footReachTolerance, 0.0f);
            const float verticalReach_ = std::sqrt((std::max)(
                usableLength_ * usableLength_ - horizontalDistanceSquared_,
                0.0f));

            const float reachableRootHeight_ = hitPosition_.y + verticalReach_;
            requiredDrop_ = (std::max)(requiredDrop_, rootPosition_.y - reachableRootHeight_);
        }
    }

    // 骨盤下降量を最大値内に制限する。
    requiredDrop_ = std::clamp(requiredDrop_, 0.0f, (std::max)(maxPelvisDrop, 0.0f));

    const float safeDeltaTime_ = std::clamp(deltaTime_, 0.0f, 0.1f);
    const float blend_ = 1.0f - std::exp(
        -(std::max)(pelvisPositionSpeed, 0.0f) * safeDeltaTime_);

    // 骨盤下降量を滑らかに補間する。
    currentPelvisDrop += (requiredDrop_ - currentPelvisDrop) * blend_;

    if (std::abs(requiredDrop_ - currentPelvisDrop) <= 0.0001f)
    {
        currentPelvisDrop = requiredDrop_;
    }

    ApplyPelvisOffset(pelvisNodeIndex_, currentPelvisDrop);
}

// 指定した骨盤NodeへWorld下方向の補正をLocal位置として反映する。
void FootIKComponent::ApplyPelvisOffset(int pelvisNodeIndex_, float worldDrop_)
{
    const std::shared_ptr<GltfModelCommponennt> model_ = model.lock();
    if (!model_) return;
    std::vector<GltfModelCommponennt::Node>& nodes_ = model_->GetNodes();
    GltfModelCommponennt::Node& pelvisNode_ = nodes_.at(pelvisNodeIndex_);

    // 前回の補正が残っている場合は、補正前位置を維持する。
    const bool previousOffsetStillApplied_ = pelvisPositionApplied
        && adjustedPelvisNodeIndex == pelvisNodeIndex_
        && Length(Subtract(pelvisNode_.position, pelvisAppliedPosition)) <= 0.0001f;

    if (!previousOffsetStillApplied_)
    {
        pelvisBasePosition = pelvisNode_.position;
    }

    XMMATRIX parentWorld_ = XMMatrixIdentity();

    if (pelvisNode_.parent)
    {
        parentWorld_ = XMLoadFloat4x4(&pelvisNode_.parent->worldTransform);
    }
    else if (const std::shared_ptr<GameObject> owner_ = GetGameObject())
    {
        parentWorld_ = owner_->transform->GetWorldMatrix();
    }

    parentWorld_.r[3] = XMVectorSet(0, 0, 0, 1);

    // World下方向の移動量を親Local空間へ変換する。
    const XMVECTOR localOffset_ = XMVector3TransformNormal(
        XMVectorSet(0.0f, -worldDrop_, 0.0f, 0.0f),
        XMMatrixInverse(nullptr, parentWorld_));

    XMFLOAT3 localOffsetStorage_{};
    XMStoreFloat3(&localOffsetStorage_, localOffset_);

    pelvisNode_.position = Add(pelvisBasePosition, localOffsetStorage_);
    pelvisAppliedPosition = pelvisNode_.position;
    adjustedPelvisNodeIndex = pelvisNodeIndex_;
    pelvisPositionApplied = true;

    model_->UpdateNodeTransformFrom(pelvisNodeIndex_);
}

// RootからTipまでのFootチェーンNodeIndex列を解決する。
bool FootIKComponent::ResolveChain(IKMapComponent::IKChain& chain_)
{
    if (chain_.rootNodeName.empty() || chain_.tipNodeName.empty()) return false;
    if (!chain_.nodeIndices.empty()) return chain_.nodeIndices.size() >= 2;

    const std::shared_ptr<GltfModelCommponennt> model_ = model.lock();
    if (!model_) return false;
    const int rootIndex_ = model_->GetNodeIndex(chain_.rootNodeName.c_str());
    const int tipIndex_ = model_->GetNodeIndex(chain_.tipNodeName.c_str());
    if (rootIndex_ < 0 || tipIndex_ < 0) return false;

    const std::vector<GltfModelCommponennt::Node>& nodes_ = model_->GetNodes();
    std::vector<int> reverseIndices_;

    // Tipから親を辿ってRootまでのNodeを集める。
    for (int currentIndex_ = tipIndex_; currentIndex_ >= 0;
        currentIndex_ = nodes_.at(currentIndex_).parentIndex)
    {
        reverseIndices_.push_back(currentIndex_);
        if (currentIndex_ == rootIndex_) break;
    }

    if (reverseIndices_.empty() || reverseIndices_.back() != rootIndex_) return false;

    // RootからTipの順番で格納する。
    chain_.nodeIndices.assign(reverseIndices_.rbegin(), reverseIndices_.rend());
    return chain_.nodeIndices.size() >= 2;
}
// 足位置から下方向へRayを飛ばして地面を取得する。
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

    // Owner自身のColliderをRaycast対象から除外する。
    for (const std::shared_ptr<ColliderComponent>& collider_ : owner_->GetComponents<ColliderComponent>())
    {
        if (collider_) ignoredUserData_.push_back(collider_.get());
    }

    FootQueryFilter queryFilter_(ignoredUserData_);
    PxQueryFlags queryFlags_ = PxQueryFlag::eSTATIC | PxQueryFlag::ePREFILTER;

    // 設定が有効なら動的Colliderも地面判定に含める。
    if (chain_.footIncludeDynamicColliders) queryFlags_ |= PxQueryFlag::eDYNAMIC;

    const float safeStepUp_ = (std::max)(chain_.footMaxStepUp, 0.0f);
    const float safeStepDown_ = (std::max)(chain_.footMaxStepDown, 0.0f);
    const float safeDistance_ = (std::max)(safeStepUp_ + safeStepDown_, EPSILON);

    PxRaycastBuffer rayHit_;

    // 足位置より少し上から下方向へRayを飛ばす。
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

    // 接地点を法線方向へ少し浮かせて足裏高さを補正する。
    hitPosition_ = Add(
        { block_.position.x, block_.position.y, block_.position.z },
        Scale(hitNormal_, chain_.footHeightOffset));

    return true;
}

// Footチェーンの足先を地面目標位置へ解く。
void FootIKComponent::SolveChain(
    IKMapComponent::IKChain& chain_,
    FootRuntimeState& state_,
    float deltaTime_)
{
    const std::shared_ptr<GltfModelCommponennt> model_ = model.lock();
    if (!model_) return;
    const std::vector<GltfModelCommponennt::Node>& nodes_ = model_->GetNodes();

    std::vector<XMFLOAT3> animatedPositions_;

    // 現在アニメーション姿勢の各ボーンWorld位置を取得する。
    for (const int nodeIndex_ : chain_.nodeIndices)
    {
        animatedPositions_.push_back(GetTranslation(nodes_.at(nodeIndex_).worldTransform));
    }

    XMFLOAT3 hitPosition_{};
    XMFLOAT3 hitNormal_{ 0.0f, 1.0f, 0.0f };

    const bool grounded_ = QueryGround(chain_, animatedPositions_.back(), hitPosition_, hitNormal_);

    // 初回は接地しているなら地面位置、未接地なら現在の足位置を目標にする。
    // animatedPositions_.back() をそのまま使うと、腰補正後に足が地面へ埋まった状態で初期化される可能性がある。
    if (!state_.initialized)
    {
        state_.targetPosition = grounded_ ? hitPosition_ : animatedPositions_.back();
        state_.groundNormal = hitNormal_;
        state_.initialized = true;
    }

    const float safeDeltaTime_ = std::clamp(deltaTime_, 0.0f, 0.1f);
    const float blend_ = 1.0f - std::exp(
        -(std::max)(chain_.footPositionSpeed, 0.0f) * safeDeltaTime_);

    // 接地している時は地面位置へ、未接地時はアニメーション位置へ戻す。
    state_.targetPosition = Lerp(
        state_.targetPosition,
        grounded_ ? hitPosition_ : animatedPositions_.back(),
        blend_);

    // 高い方の足が、前フレームの低いターゲットへ引っ張られて地面へ埋まらないようにする。
    state_.targetPosition = ClampFootPositionAboveGround(
        state_.targetPosition,
        hitPosition_,
        grounded_);

    state_.groundNormal = Lerp(state_.groundNormal, hitNormal_, blend_);
    state_.grounded = grounded_;

    std::vector<XMFLOAT3> solvedPositions_ = animatedPositions_;
    std::vector<float> lengths_;
    float totalLength_ = 0.0f;

    // 各ボーン間の長さを保存する。
    for (size_t pointIndex_ = 1; pointIndex_ < solvedPositions_.size(); ++pointIndex_)
    {
        const float length_ = Length(Subtract(solvedPositions_[pointIndex_], solvedPositions_[pointIndex_ - 1]));
        lengths_.push_back(length_);
        totalLength_ += length_;
    }

    const XMFLOAT3 rootPosition_ = solvedPositions_.front();

    XMFLOAT3 targetPosition_ = Lerp(
        animatedPositions_.back(),
        state_.targetPosition,
        chain_.influence);

    // influence が 1.0 未満の時、animatedFoot 側に引っ張られて地面下へ戻る可能性があるため、
    // 最終IK目標も接地点より下に行かないようにする。
    targetPosition_ = ClampFootPositionAboveGround(
        targetPosition_,
        hitPosition_,
        grounded_);

    // 足が届かない場合はRootからTarget方向へ最大まで伸ばす。
    if (Length(Subtract(targetPosition_, rootPosition_)) >= totalLength_ - EPSILON)
    {
        const XMFLOAT3 direction_ = Normalize(Subtract(targetPosition_, rootPosition_));

        for (size_t pointIndex_ = 1; pointIndex_ < solvedPositions_.size(); ++pointIndex_)
        {
            solvedPositions_[pointIndex_] = Add(
                solvedPositions_[pointIndex_ - 1],
                Scale(direction_, lengths_[pointIndex_ - 1]));
        }
    }
    else
    {
        // FABRIK方式でTipからRoot、RootからTipの順に位置を収束させる。
        for (int iteration_ = 0;
            iteration_ < std::clamp(chain_.footSolverIterations, 1, 32);
            ++iteration_)
        {
            solvedPositions_.back() = targetPosition_;

            // Tip側からRoot側へ戻す。
            for (size_t pointIndex_ = solvedPositions_.size() - 1; pointIndex_ > 0; --pointIndex_)
            {
                const XMFLOAT3 direction_ = Normalize(Subtract(solvedPositions_[pointIndex_ - 1], solvedPositions_[pointIndex_]));
                solvedPositions_[pointIndex_ - 1] = Add(solvedPositions_[pointIndex_], Scale(direction_, lengths_[pointIndex_ - 1]));
            }

            solvedPositions_.front() = rootPosition_;

            // Root側からTip側へ伸ばす。
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

    // 接地中のみ足裏を地面法線に合わせる。
    if (state_.grounded && chain_.footAlignToGround)
    {
        AlignFootToGround(chain_, Normalize(state_.groundNormal), safeDeltaTime_);
    }

    DrawDebugShapes(chain_, animatedPositions_.back(), state_, solvedPositions_);
}

// 解いた各点の方向に合うようボーン回転を更新する。
void FootIKComponent::ApplySolvedPose(
    const IKMapComponent::IKChain& chain_,
    const std::vector<XMFLOAT3>& solvedPositions_)
{
    const std::shared_ptr<GltfModelCommponennt> model_ = model.lock();
    if (!model_) return;
    std::vector<GltfModelCommponennt::Node>& nodes_ = model_->GetNodes();
    bool chainChanged_ = false;

    for (size_t pointIndex_ = 0; pointIndex_ + 1 < chain_.nodeIndices.size(); ++pointIndex_)
    {
        model_->UpdateNodeTransformOnly(chain_.nodeIndices[pointIndex_]);
        model_->UpdateNodeTransformOnly(chain_.nodeIndices[pointIndex_ + 1]);

        GltfModelCommponennt::Node& node_ = nodes_.at(chain_.nodeIndices[pointIndex_]);
        const GltfModelCommponennt::Node& child_ = nodes_.at(chain_.nodeIndices[pointIndex_ + 1]);

        const XMFLOAT3 currentDirection_ = Normalize(
            Subtract(GetTranslation(child_.worldTransform), GetTranslation(node_.worldTransform)));
        const XMFLOAT3 targetDirection_ = Normalize(
            Subtract(solvedPositions_[pointIndex_ + 1], solvedPositions_[pointIndex_]));

        const XMVECTOR currentVector_ = XMLoadFloat3(&currentDirection_);
        const XMVECTOR targetVector_ = XMLoadFloat3(&targetDirection_);

        // 現在方向から目標方向へ回す軸を求める。
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

        // World回転結果を親基準のLocal回転へ戻す。
        const XMMATRIX desiredWorldRotation_ = worldRotation_ * XMMatrixRotationAxis(axis_, std::acos(dot_));
        const XMMATRIX desiredLocalRotation_ = desiredWorldRotation_ * XMMatrixInverse(nullptr, parentWorldRotation_);

        XMVECTOR scale_;
        XMVECTOR rotation_;
        XMVECTOR translation_;

        // 分解したLocal回転をNodeへ反映する。
        if (XMMatrixDecompose(&scale_, &rotation_, &translation_, desiredLocalRotation_))
        {
            XMStoreFloat4(&node_.rotation, XMQuaternionNormalize(rotation_));
            model_->UpdateNodeTransformOnly(chain_.nodeIndices[pointIndex_]);
            chainChanged_ = true;
        }
    }

    // チェーン変更後にRootからWorldTransformを更新する。
    if (chainChanged_)
    {
        model_->UpdateNodeTransformFrom(chain_.nodeIndices.front());
    }
}

void FootIKComponent::AlignFootToGround(
    const IKMapComponent::IKChain& chain_,
    const XMFLOAT3& groundNormal_,
    float deltaTime_)
{
    const std::shared_ptr<GltfModelCommponennt> model_ = model.lock();
    if (!model_) return;
    std::vector<GltfModelCommponennt::Node>& nodes_ = model_->GetNodes();
    GltfModelCommponennt::Node& footNode_ = nodes_.at(chain_.nodeIndices.back());
    XMMATRIX footWorldRotation_ = XMLoadFloat4x4(&footNode_.worldTransform);
    footWorldRotation_.r[3] = XMVectorSet(0, 0, 0, 1);
    const XMVECTOR currentUp_ = XMVector3Normalize(footWorldRotation_.r[1]);
    const XMVECTOR desiredUp_ = XMLoadFloat3(&groundNormal_);
    XMVECTOR axis_ = XMVector3Cross(currentUp_, desiredUp_);
    if (XMVectorGetX(XMVector3LengthSq(axis_)) <= EPSILON) return;
    axis_ = XMVector3Normalize(axis_);
    const float dot_ = std::clamp(
        XMVectorGetX(XMVector3Dot(currentUp_, desiredUp_)), -1.0f, 1.0f);
    const float blend_ = 1.0f - std::exp(
        -(std::max)(chain_.footRotationSpeed, 0.0f) * deltaTime_);
    XMMATRIX parentWorldRotation_ = XMLoadFloat4x4(&footNode_.parent->worldTransform);
    parentWorldRotation_.r[3] = XMVectorSet(0, 0, 0, 1);
    const XMMATRIX desiredWorldRotation_ = footWorldRotation_
        * XMMatrixRotationAxis(axis_, std::acos(dot_) * blend_ * chain_.influence);
    const XMMATRIX desiredLocalRotation_ = desiredWorldRotation_
        * XMMatrixInverse(nullptr, parentWorldRotation_);
    XMVECTOR scale_;
    XMVECTOR rotation_;
    XMVECTOR translation_;
    if (XMMatrixDecompose(&scale_, &rotation_, &translation_, desiredLocalRotation_))
    {
        XMStoreFloat4(&footNode_.rotation, XMQuaternionNormalize(rotation_));
        model_->UpdateNodeTransformFrom(chain_.nodeIndices.back());
    }
}

void FootIKComponent::ApplyPoleConstraint(
    const IKMapComponent::IKChain& chain_,
    const std::vector<XMFLOAT3>& animatedPositions_,
    std::vector<XMFLOAT3>& solvedPositions_) const
{
    const std::shared_ptr<GltfModelCommponennt> model_ = model.lock();
    if (!model_) return;
    const float bendWeight_ = std::clamp(chain_.footBendWeight, 0.0f, 1.0f);
    if (bendWeight_ <= EPSILON || solvedPositions_.size() < 3) return;
    XMFLOAT3 configuredPolePosition_{};
    bool hasConfiguredPole_ = false;
    if (!chain_.footPoleNodeName.empty())
    {
        const int poleNodeIndex_ = model_->GetNodeIndex(chain_.footPoleNodeName.c_str());
        if (poleNodeIndex_ >= 0)
        {
            configuredPolePosition_ = GetTranslation(
                model_->GetNodes().at(poleNodeIndex_).worldTransform);
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
            ? configuredPolePosition_ : animatedPositions_[pointIndex_];
        const XMFLOAT3 currentOffset_ = Subtract(
            solvedPositions_[pointIndex_], previousPosition_);
        const XMFLOAT3 poleOffset_ = Subtract(polePosition_, previousPosition_);
        const XMVECTOR axisVector_ = XMLoadFloat3(&axisDirection_);
        const XMVECTOR currentVector_ = XMLoadFloat3(&currentOffset_);
        const XMVECTOR poleVector_ = XMLoadFloat3(&poleOffset_);
        const XMVECTOR currentProjected_ = XMVectorSubtract(currentVector_,
            XMVectorScale(axisVector_, XMVectorGetX(XMVector3Dot(currentVector_, axisVector_))));
        const XMVECTOR poleProjected_ = XMVectorSubtract(poleVector_,
            XMVectorScale(axisVector_, XMVectorGetX(XMVector3Dot(poleVector_, axisVector_))));
        if (XMVectorGetX(XMVector3LengthSq(currentProjected_)) <= EPSILON
            || XMVectorGetX(XMVector3LengthSq(poleProjected_)) <= EPSILON) continue;
        const XMVECTOR currentDirection_ = XMVector3Normalize(currentProjected_);
        const XMVECTOR poleDirection_ = XMVector3Normalize(poleProjected_);
        const float cosine_ = std::clamp(
            XMVectorGetX(XMVector3Dot(currentDirection_, poleDirection_)), -1.0f, 1.0f);
        const float sine_ = XMVectorGetX(XMVector3Dot(
            axisVector_, XMVector3Cross(currentDirection_, poleDirection_)));
        const XMVECTOR rotatedVector_ = XMVector3Rotate(currentVector_,
            XMQuaternionRotationAxis(axisVector_, std::atan2(sine_, cosine_) * bendWeight_));
        XMStoreFloat3(&solvedPositions_[pointIndex_],
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
    const std::shared_ptr<GltfModelCommponennt> model_ = model.lock();
    if (!model_) return;
    ShapeRenderSystem& renderer_ =
        GraphicsDeviceManager::Instance().GetShapeRendererSystem();
    if (!chain_.footPoleNodeName.empty())
    {
        const int poleNodeIndex_ = model_->GetNodeIndex(chain_.footPoleNodeName.c_str());
        if (poleNodeIndex_ >= 0)
        {
            DrawBoneBetween(renderer_, solvedPositions_.front(),
                GetTranslation(model_->GetNodes().at(poleNodeIndex_).worldTransform), debugRayColor);
        }
    }
    for (size_t pointIndex_ = 0; pointIndex_ + 1 < solvedPositions_.size(); ++pointIndex_)
    {
        DrawBoneBetween(renderer_, solvedPositions_[pointIndex_],
            solvedPositions_[pointIndex_ + 1], debugChainColor);
    }
    const XMFLOAT3 rayStart_ = Add(animatedFootPosition_,
        { 0.0f, (std::max)(chain_.footMaxStepUp, 0.0f), 0.0f });
    const XMFLOAT3 rayEnd_ = Add(animatedFootPosition_,
        { 0.0f, -(std::max)(chain_.footMaxStepDown, 0.0f), 0.0f });
    DrawBoneBetween(renderer_, rayStart_, rayEnd_, debugRayColor);
    if (!state_.grounded) return;
    const XMFLOAT3 normalEnd_ = Add(state_.targetPosition,
        Scale(Normalize(state_.groundNormal), (std::max)(debugNormalLength, 0.001f)));
    DrawBoneBetween(renderer_, state_.targetPosition, normalEnd_, debugGroundColor);
}

void FootIKComponent::Gui()
{
    ImGui::Checkbox("Enabled", &enabled);
    ImGui::SeparatorText("Pelvis Height Adjustment");
    ImGui::Checkbox("Adjust Pelvis Height", &adjustPelvisHeight);
    ImGui::DragFloat("Max Pelvis Drop", &maxPelvisDrop, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("Pelvis Position Speed", &pelvisPositionSpeed, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("Foot Reach Tolerance", &footReachTolerance, 0.001f, 0.0f, 0.25f);
    ImGui::Text("Current Pelvis Drop: %.3f", currentPelvisDrop);
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
    const std::shared_ptr<IKMapComponent> ikMap_ = ikMap.lock();
    const std::shared_ptr<GltfModelCommponennt> model_ = model.lock();
    if (!ikMap_ || !model_) return;
    if (adjustedPelvisNodeIndex >= 0
        && adjustedPelvisNodeIndex < static_cast<int>(model_->GetNodes().size()))
    {
        ImGui::Text("Auto Pelvis Node: %s",
            model_->GetNodes()[adjustedPelvisNodeIndex].name.c_str());
    }
    ImGui::SeparatorText("IKMap Chains");
    for (const IKMapComponent::IKChain& chain_ : ikMap_->GetChains())
    {
        if (chain_.type != IKMapComponent::IK_TYPE::FOOT) continue;
        ImGui::Text("%s: %s -> %s", chain_.name.c_str(),
            chain_.rootNodeName.empty() ? "Select Root" : chain_.rootNodeName.c_str(),
            chain_.tipNodeName.empty() ? "Select Tip" : chain_.tipNodeName.c_str());
    }
}

void FootIKComponent::Serialize(nlohmann::json& json_) const
{
    json_["Enabled"] = enabled;
    json_["AdjustPelvisHeight"] = adjustPelvisHeight;
    json_["MaxPelvisDrop"] = maxPelvisDrop;
    json_["PelvisPositionSpeed"] = pelvisPositionSpeed;
    json_["FootReachTolerance"] = footReachTolerance;
    json_["ShowDebugShapes"] = showDebugShapes;
    json_["DebugNormalLength"] = debugNormalLength;
    json_["DebugChainColor"] = { debugChainColor.x, debugChainColor.y,
        debugChainColor.z, debugChainColor.w };
    json_["DebugRayColor"] = { debugRayColor.x, debugRayColor.y,
        debugRayColor.z, debugRayColor.w };
    json_["DebugGroundColor"] = { debugGroundColor.x, debugGroundColor.y,
        debugGroundColor.z, debugGroundColor.w };
}

void FootIKComponent::Deserialize(const nlohmann::json& json_)
{
    enabled = json_.value("Enabled", true);
    adjustPelvisHeight = json_.value("AdjustPelvisHeight", true);
    maxPelvisDrop = (std::max)(json_.value("MaxPelvisDrop", 0.5f), 0.0f);
    pelvisPositionSpeed = (std::max)(json_.value("PelvisPositionSpeed", 12.0f), 0.0f);
    footReachTolerance = (std::max)(json_.value("FootReachTolerance", 0.01f), 0.0f);
    showDebugShapes = json_.value("ShowDebugShapes", true);
    debugNormalLength = json_.value("DebugNormalLength", 0.2f);
    if (json_.contains("DebugChainColor")) debugChainColor = {
        json_["DebugChainColor"][0], json_["DebugChainColor"][1],
        json_["DebugChainColor"][2], json_["DebugChainColor"][3] };
    if (json_.contains("DebugRayColor")) debugRayColor = {
        json_["DebugRayColor"][0], json_["DebugRayColor"][1],
        json_["DebugRayColor"][2], json_["DebugRayColor"][3] };
    if (json_.contains("DebugGroundColor")) debugGroundColor = {
        json_["DebugGroundColor"][0], json_["DebugGroundColor"][1],
        json_["DebugGroundColor"][2], json_["DebugGroundColor"][3] };
    footStates.clear();
    adjustedPelvisNodeIndex = -1;
    currentPelvisDrop = 0.0f;
    pelvisPositionApplied = false;
}

REGISTER_COMPONENT(FootIKComponent, "FootIKComponent")
