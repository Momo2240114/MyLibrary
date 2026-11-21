#include "PrecompiledHeader.h"

#include "ShieldBlockIKComponent.h"

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Physics/CollisionStepSystem.h"
#include "Engine/Physics/HitBox.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>

using namespace DirectX;

namespace
{
    constexpr float EPSILON = 0.00001f;

    // 盾IKでは攻撃位置、腕ボーン、デバッグ線を同じXMFLOAT3で扱うため、局所的なベクトル補助を置く。
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

    XMFLOAT3 GetCapsuleCenter(const Capsule& capsule_)
    {
        XMFLOAT3 start_{};
        XMFLOAT3 end_{};
        XMStoreFloat3(&start_, capsule_.p0);
        XMStoreFloat3(&end_, capsule_.p1);
        return Scale(Add(start_, end_), 0.5f);
    }

    void DrawBoneBetween(
        ShapeRenderSystem& renderer_,
        const XMFLOAT3& startPosition_,
        const XMFLOAT3& endPosition_,
        const XMFLOAT4& color_)
    {
        const XMFLOAT3 difference_ = Subtract(endPosition_, startPosition_);
        const float length_ = Length(difference_);
        if (length_ <= EPSILON)
        {
            return;
        }

        const XMVECTOR forward_ = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        const XMVECTOR direction_ = XMVector3Normalize(XMLoadFloat3(&difference_));
        const float dot_ = std::clamp(XMVectorGetX(XMVector3Dot(forward_, direction_)), -1.0f, 1.0f);

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

        transform_.r[3] = XMVectorSet(startPosition_.x, startPosition_.y, startPosition_.z, 1.0f);

        XMFLOAT4X4 transformStorage_;
        XMStoreFloat4x4(&transformStorage_, transform_);
        renderer_.DrawBone(transformStorage_, length_, color_);
    }
}

const std::string& ShieldBlockIKComponent::GetName() const
{
    static const std::string name_ = "ShieldBlockIKComponent";
    return name_;
}

void ShieldBlockIKComponent::Start()
{
    // IKMap側に盾ガード用チェーンを登録しておき、実際のボーン指定はIKMapのGUIで調整できるようにする。
    if (!ResolveDependencies())
    {
        return;
    }

    const std::shared_ptr<IKMapComponent> ikMap_ = ikMap.lock();
    if (!ikMap_)
    {
        return;
    }

    ikMap_->AddShieldBlockChain("ShieldBlock");
}

bool ShieldBlockIKComponent::IsLoadReady() const
{
    const auto owner_ = const_cast<ShieldBlockIKComponent*>(this)->GetGameObject();
    const auto model_ = owner_ ? owner_->GetComponent<GltfModelCommponennt>() : nullptr;
    if (!model_)
    {
        return true;
    }

    model_->UpdateLoadState();
    return !model_->IsModelLoading();
}

bool ShieldBlockIKComponent::ResolveDependencies()
{
    // 同じGameObject上のIKMapとモデルを取得し、ロード完了後のLateUpdateで使える状態にする。
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return false;
    }

    if (ikMap.expired())
    {
        ikMap = owner_->GetComponent<IKMapComponent>();
    }
    if (model.expired())
    {
        model = owner_->GetComponent<GltfModelCommponennt>();
    }

    return !ikMap.expired() && !model.expired();
}

void ShieldBlockIKComponent::LateUpdate(float deltaTime_)
{
    BS_PROFILE_SCOPE("ShieldBlockIKComponent::LateUpdate");

    // アクティブな攻撃HitBoxを探し、近い攻撃方向へ盾腕を2ボーンIKで向ける。
    if (!enabled || !ResolveDependencies())
    {
        return;
    }

    const std::shared_ptr<GltfModelCommponennt> model_ = model.lock();
    if (model_)
    {
        model_->UpdateLoadState();
    }
    if (!model_ || !model_->IsModelReady())
    {
        return;
    }

    const std::shared_ptr<IKMapComponent> ikMap_ = ikMap.lock();
    if (!ikMap_)
    {
        return;
    }

    std::vector<IKMapComponent::IKChain>& chains_ = ikMap_->GetChains();
    if (shieldStates.size() != chains_.size())
    {
        shieldStates.resize(chains_.size());
    }

    const float safeDeltaTime_ = std::clamp(deltaTime_, 0.0f, 0.1f);
    const std::vector<GltfModelCommponennt::Node>& nodes_ = model_->GetNodes();

    for (size_t chainIndex_ = 0; chainIndex_ < chains_.size(); ++chainIndex_)
    {
        IKMapComponent::IKChain& chain_ = chains_[chainIndex_];
        if (!chain_.enabled || chain_.type != IKMapComponent::IK_TYPE::SHIELD_BLOCK)
        {
            continue;
        }
        if (!ResolveChain(chain_) || chain_.nodeIndices.size() < 3)
        {
            continue;
        }

        std::vector<XMFLOAT3> animatedPositions_;
        animatedPositions_.reserve(3u);
        animatedPositions_.push_back(GetTranslation(nodes_.at(chain_.nodeIndices[0]).worldTransform));
        animatedPositions_.push_back(GetTranslation(nodes_.at(chain_.nodeIndices[1]).worldTransform));
        animatedPositions_.push_back(GetTranslation(nodes_.at(chain_.nodeIndices[2]).worldTransform));

        const float blend_ = 1.0f - std::exp(
            -(std::max)(chain_.shieldBlendSpeed, 0.0f) * safeDeltaTime_);
        XMFLOAT3 attackPosition_{};
        const bool hasAttack_ = FindNearestAttack(chain_, animatedPositions_[1], attackPosition_);

        // 攻撃が見つかった時だけ重みを上げ、攻撃が消えたら急に戻らずブレンドして解除する。
        ShieldRuntimeState& state_ = shieldStates[chainIndex_];
        const float desiredWeight_ = hasAttack_ ? 1.0f : 0.0f;
        state_.activeWeight += (desiredWeight_ - state_.activeWeight) * blend_;

        const float finalWeight_ = std::clamp(state_.activeWeight * chain_.influence, 0.0f, 1.0f);
        if (finalWeight_ <= EPSILON || !hasAttack_)
        {
            continue;
        }

        const XMFLOAT3 targetDirection_ = ClampDirection(chain_, animatedPositions_[0], attackPosition_);
        std::vector<XMFLOAT3> solvedPositions_ = animatedPositions_;
        if (!SolveTwoBone(chain_, animatedPositions_, targetDirection_, solvedPositions_))
        {
            continue;
        }

        ApplySolvedPose(chain_, solvedPositions_, finalWeight_);
        DrawDebugShapes(chain_, solvedPositions_, attackPosition_);
    }
}

bool ShieldBlockIKComponent::ResolveChain(IKMapComponent::IKChain& chain_)
{
    // rootからtipまで親をたどり、肩/肘/手首の最低3点を持つチェーンとして解決する。
    if (chain_.rootNodeName.empty() || chain_.tipNodeName.empty())
    {
        return false;
    }
    if (!chain_.nodeIndices.empty())
    {
        return chain_.nodeIndices.size() >= 3;
    }

    const std::shared_ptr<GltfModelCommponennt> model_ = model.lock();
    if (!model_)
    {
        return false;
    }

    const int rootIndex_ = model_->GetNodeIndex(chain_.rootNodeName.c_str());
    const int tipIndex_ = model_->GetNodeIndex(chain_.tipNodeName.c_str());
    if (rootIndex_ < 0 || tipIndex_ < 0)
    {
        return false;
    }

    const std::vector<GltfModelCommponennt::Node>& nodes_ = model_->GetNodes();
    std::vector<int> reverseIndices_;
    for (int currentIndex_ = tipIndex_; currentIndex_ >= 0;
        currentIndex_ = nodes_.at(currentIndex_).parentIndex)
    {
        reverseIndices_.push_back(currentIndex_);
        if (currentIndex_ == rootIndex_)
        {
            break;
        }
    }

    if (reverseIndices_.empty() || reverseIndices_.back() != rootIndex_)
    {
        return false;
    }

    chain_.nodeIndices.assign(reverseIndices_.rbegin(), reverseIndices_.rend());
    return chain_.nodeIndices.size() >= 3;
}

bool ShieldBlockIKComponent::FindNearestAttack(
    const IKMapComponent::IKChain& chain_,
    const XMFLOAT3& origin_,
    XMFLOAT3& attackPosition_) const
{
    // CollisionStepSystemから現在有効なHitBoxを集め、自分以外で最も近い攻撃中心を盾の向き先にする。
    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_ || !owner_->manager)
    {
        return false;
    }

    const float searchRadius_ = (std::max)(chain_.shieldSearchRadius, 0.0f);
    const float searchRadiusSq_ = searchRadius_ * searchRadius_;
    float nearestDistanceSq_ = searchRadiusSq_;
    bool found_ = false;

    std::vector<std::shared_ptr<HitBox>> hitBoxes_ =
        owner_->manager->GetCollisionStepSystem().CollectActiveHitBoxes();
    for (const std::shared_ptr<HitBox>& hitBox_ : hitBoxes_)
    {
        if (!hitBox_ || hitBox_->ownerObject == owner_.get())
        {
            continue;
        }

        const std::shared_ptr<GameObject> hitObject_ = hitBox_->GetGameObject();
        if (hitObject_.get() == owner_.get())
        {
            continue;
        }

        const XMFLOAT3 center_ = GetCapsuleCenter(hitBox_->GetCapsuleData());
        const XMFLOAT3 difference_ = Subtract(center_, origin_);
        const float distanceSq_ =
            difference_.x * difference_.x + difference_.y * difference_.y + difference_.z * difference_.z;
        if (distanceSq_ > nearestDistanceSq_)
        {
            continue;
        }

        nearestDistanceSq_ = distanceSq_;
        attackPosition_ = center_;
        found_ = true;
    }

    return found_;
}

XMFLOAT3 ShieldBlockIKComponent::ClampDirection(
    const IKMapComponent::IKChain& chain_,
    const XMFLOAT3& from_,
    const XMFLOAT3& to_) const
{
    // 攻撃方向をキャラクター基準のyaw/pitch上限内へ収め、腕が無理な方向へねじれないようにする。
    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_ || !owner_->transform)
    {
        return Normalize(Subtract(to_, from_));
    }

    const XMMATRIX ownerWorld_ = owner_->transform->GetWorldMatrix();
    const XMVECTOR ownerRight_ = XMVector3Normalize(ownerWorld_.r[0]);
    const XMVECTOR ownerUp_ = XMVector3Normalize(ownerWorld_.r[1]);
    const XMVECTOR ownerForward_ = XMVector3Normalize(ownerWorld_.r[2]);
    const XMFLOAT3 targetDifference_ = Subtract(to_, from_);
    if (Length(targetDifference_) <= EPSILON)
    {
        XMFLOAT3 forward_{};
        XMStoreFloat3(&forward_, ownerForward_);
        return forward_;
    }
    const XMVECTOR targetDirection_ = XMVector3Normalize(XMLoadFloat3(&targetDifference_));

    const float forwardAmount_ = XMVectorGetX(XMVector3Dot(targetDirection_, ownerForward_));
    const float rightAmount_ = XMVectorGetX(XMVector3Dot(targetDirection_, ownerRight_));
    const float upAmount_ = std::clamp(XMVectorGetX(XMVector3Dot(targetDirection_, ownerUp_)), -1.0f, 1.0f);

    float yaw_ = std::atan2(rightAmount_, forwardAmount_);
    float pitch_ = std::asin(upAmount_);
    yaw_ = std::clamp(
        yaw_,
        -XMConvertToRadians((std::max)(chain_.shieldMaxYawDegrees, 0.0f)),
        XMConvertToRadians((std::max)(chain_.shieldMaxYawDegrees, 0.0f)));
    pitch_ = std::clamp(
        pitch_,
        -XMConvertToRadians((std::max)(chain_.shieldMaxPitchDegrees, 0.0f)),
        XMConvertToRadians((std::max)(chain_.shieldMaxPitchDegrees, 0.0f)));

    const float cosPitch_ = std::cos(pitch_);
    XMFLOAT3 result_{};
    XMStoreFloat3(
        &result_,
        XMVector3Normalize(
            XMVectorAdd(
                XMVectorAdd(
                    XMVectorScale(ownerForward_, std::cos(yaw_) * cosPitch_),
                    XMVectorScale(ownerRight_, std::sin(yaw_) * cosPitch_)),
                XMVectorScale(ownerUp_, std::sin(pitch_)))));
    return result_;
}

bool ShieldBlockIKComponent::SolveTwoBone(
    const IKMapComponent::IKChain& chain_,
    const std::vector<XMFLOAT3>& animatedPositions_,
    const XMFLOAT3& targetDirection_,
    std::vector<XMFLOAT3>& solvedPositions_) const
{
    // 肩から見た防御方向へ手首目標を置き、余弦定理で肘位置を求めて腕の長さを保つ。
    if (animatedPositions_.size() < 3 || solvedPositions_.size() < 3)
    {
        return false;
    }

    const XMFLOAT3 shoulderPosition_ = animatedPositions_[0];
    const XMFLOAT3 elbowPosition_ = animatedPositions_[1];
    const XMFLOAT3 wristPosition_ = animatedPositions_[2];
    const float upperLength_ = Length(Subtract(elbowPosition_, shoulderPosition_));
    const float lowerLength_ = Length(Subtract(wristPosition_, elbowPosition_));
    if (upperLength_ <= EPSILON || lowerLength_ <= EPSILON)
    {
        return false;
    }

    const float ratio_ = std::clamp(chain_.shieldElbowWristRatio, 0.0f, 1.0f);
    const XMFLOAT3 shieldPoint_ = Lerp(elbowPosition_, wristPosition_, ratio_);
    const float targetDistance_ = std::clamp(
        Length(Subtract(shieldPoint_, shoulderPosition_)) / (std::max)(ratio_, 0.35f),
        std::abs(upperLength_ - lowerLength_) + EPSILON,
        upperLength_ + lowerLength_ - EPSILON);
    const XMFLOAT3 direction_ = Normalize(targetDirection_);
    if (Length(direction_) <= EPSILON)
    {
        return false;
    }

    const XMFLOAT3 wristTarget_ = Add(shoulderPosition_, Scale(direction_, targetDistance_));
    const XMFLOAT3 shoulderToWrist_ = Subtract(wristTarget_, shoulderPosition_);
    const float distance_ = Length(shoulderToWrist_);
    if (distance_ <= EPSILON)
    {
        return false;
    }

    const XMFLOAT3 axisDirection_ = Normalize(shoulderToWrist_);

    XMFLOAT3 polePosition_ = elbowPosition_;
    const std::shared_ptr<GltfModelCommponennt> model_ = model.lock();
    if (model_ && !chain_.shieldPoleNodeName.empty())
    {
        const int poleNodeIndex_ = model_->GetNodeIndex(chain_.shieldPoleNodeName.c_str());
        if (poleNodeIndex_ >= 0)
        {
            polePosition_ = GetTranslation(model_->GetNodes().at(poleNodeIndex_).worldTransform);
        }
    }

    XMVECTOR axisVector_ = XMLoadFloat3(&axisDirection_);
    XMVECTOR poleVector_ = XMVectorSubtract(XMLoadFloat3(&polePosition_), XMLoadFloat3(&shoulderPosition_));
    poleVector_ = XMVectorSubtract(
        poleVector_,
        XMVectorScale(axisVector_, XMVectorGetX(XMVector3Dot(poleVector_, axisVector_))));
    if (XMVectorGetX(XMVector3LengthSq(poleVector_)) <= EPSILON)
    {
        poleVector_ = XMVector3Cross(axisVector_, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
        if (XMVectorGetX(XMVector3LengthSq(poleVector_)) <= EPSILON)
        {
            poleVector_ = XMVector3Cross(axisVector_, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
        }
    }
    poleVector_ = XMVector3Normalize(poleVector_);

    // 余弦定理で肩->手首軸上の肘位置を求め、垂直方向へ曲げ高さを足して上下腕の長さを保つ。
    const float projectedLength_ =
        (upperLength_ * upperLength_ - lowerLength_ * lowerLength_ + distance_ * distance_)
        / (2.0f * distance_);
    const float bendHeight_ = std::sqrt((std::max)(
        upperLength_ * upperLength_ - projectedLength_ * projectedLength_,
        0.0f));

    XMVECTOR shoulderVector_ = XMLoadFloat3(&shoulderPosition_);
    XMVECTOR solvedElbow_ = XMVectorAdd(
        XMVectorAdd(shoulderVector_, XMVectorScale(axisVector_, projectedLength_)),
        XMVectorScale(poleVector_, bendHeight_));

    XMStoreFloat3(&solvedPositions_[0], shoulderVector_);
    XMStoreFloat3(&solvedPositions_[1], solvedElbow_);
    solvedPositions_[2] = wristTarget_;
    return true;
}

void ShieldBlockIKComponent::ApplySolvedPose(
    const IKMapComponent::IKChain& chain_,
    const std::vector<XMFLOAT3>& solvedPositions_,
    float weight_)
{
    // 解いた肩/肘/手首の方向に合わせて各ボーンのローカル回転を更新し、現在ポーズへ重み付きで混ぜる。
    const std::shared_ptr<GltfModelCommponennt> model_ = model.lock();
    if (!model_)
    {
        return;
    }

    std::vector<GltfModelCommponennt::Node>& nodes_ = model_->GetNodes();
    bool chainChanged_ = false;

    for (size_t pointIndex_ = 0; pointIndex_ + 1 < 3u; ++pointIndex_)
    {
        model_->UpdateNodeTransformOnly(chain_.nodeIndices[pointIndex_]);
        model_->UpdateNodeTransformOnly(chain_.nodeIndices[pointIndex_ + 1]);

        GltfModelCommponennt::Node& node_ = nodes_.at(chain_.nodeIndices[pointIndex_]);
        const GltfModelCommponennt::Node& child_ = nodes_.at(chain_.nodeIndices[pointIndex_ + 1]);

        const XMFLOAT3 currentDirection_ = Normalize(
            Subtract(GetTranslation(child_.worldTransform), GetTranslation(node_.worldTransform)));
        const XMFLOAT3 targetDirection_ = Normalize(
            Subtract(solvedPositions_[pointIndex_ + 1], solvedPositions_[pointIndex_]));
        if (Length(currentDirection_) <= EPSILON || Length(targetDirection_) <= EPSILON)
        {
            continue;
        }

        const XMVECTOR currentVector_ = XMLoadFloat3(&currentDirection_);
        const XMVECTOR targetVector_ = XMLoadFloat3(&targetDirection_);
        XMVECTOR axis_ = XMVector3Cross(currentVector_, targetVector_);
        if (XMVectorGetX(XMVector3LengthSq(axis_)) <= EPSILON)
        {
            continue;
        }

        axis_ = XMVector3Normalize(axis_);
        const float dot_ = std::clamp(XMVectorGetX(XMVector3Dot(currentVector_, targetVector_)), -1.0f, 1.0f);

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

        const XMMATRIX desiredWorldRotation_ = worldRotation_
            * XMMatrixRotationAxis(axis_, std::acos(dot_) * std::clamp(weight_, 0.0f, 1.0f));
        const XMMATRIX desiredLocalRotation_ = desiredWorldRotation_
            * XMMatrixInverse(nullptr, parentWorldRotation_);

        XMVECTOR scale_;
        XMVECTOR rotation_;
        XMVECTOR translation_;
        if (XMMatrixDecompose(&scale_, &rotation_, &translation_, desiredLocalRotation_))
        {
            XMStoreFloat4(&node_.rotation, XMQuaternionNormalize(rotation_));
            model_->UpdateNodeTransformOnly(chain_.nodeIndices[pointIndex_]);
            chainChanged_ = true;
        }
    }

    if (chainChanged_)
    {
        model_->UpdateNodeTransformFrom(chain_.nodeIndices.front());
    }
}

void ShieldBlockIKComponent::DrawDebugShapes(
    const IKMapComponent::IKChain& chain_,
    const std::vector<XMFLOAT3>& solvedPositions_,
    const XMFLOAT3& attackPosition_) const
{
    // 解決後の腕ラインと攻撃方向を描画し、探索範囲や角度制限の調整をしやすくする。
    (void)chain_;
    if (!showDebugShapes || solvedPositions_.size() < 3)
    {
        return;
    }

    ShapeRenderSystem& renderer_ = GraphicsDeviceManager::Instance().GetShapeRendererSystem();
    DrawBoneBetween(renderer_, solvedPositions_[0], solvedPositions_[1], debugChainColor);
    DrawBoneBetween(renderer_, solvedPositions_[1], solvedPositions_[2], debugChainColor);
    DrawBoneBetween(renderer_, solvedPositions_[2], attackPosition_, debugTargetColor);
}

void ShieldBlockIKComponent::Gui()
{
    // 盾IK固有の実行ON/OFFとデバッグ表示だけを持ち、チェーンや角度上限はIKMap側で管理する。
    ImGui::Checkbox("Enabled", &enabled);
    ImGui::Checkbox("Show Debug Shapes", &showDebugShapes);
    if (showDebugShapes)
    {
        ImGui::ColorEdit4("Debug Chain Color", &debugChainColor.x);
        ImGui::ColorEdit4("Debug Target Color", &debugTargetColor.x);
    }
    ImGui::TextDisabled("Shield block chains and limits are configured in IKMapComponent.");
}

void ShieldBlockIKComponent::Serialize(nlohmann::json& json_) const
{
    // このコンポーネント側では実行状態とデバッグ色だけを保存する。
    json_["Enabled"] = enabled;
    json_["ShowDebugShapes"] = showDebugShapes;
    json_["DebugChainColor"] = { debugChainColor.x, debugChainColor.y, debugChainColor.z, debugChainColor.w };
    json_["DebugTargetColor"] = { debugTargetColor.x, debugTargetColor.y, debugTargetColor.z, debugTargetColor.w };
}

void ShieldBlockIKComponent::Deserialize(const nlohmann::json& json_)
{
    // 読み込み時はランタイムのブレンド状態を捨て、現在の攻撃状況から再計算させる。
    enabled = json_.value("Enabled", true);
    showDebugShapes = json_.value("ShowDebugShapes", false);
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
    shieldStates.clear();
}

REGISTER_COMPONENT(ShieldBlockIKComponent, "ShieldBlockIKComponent")
