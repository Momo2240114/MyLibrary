#include "PrecompiledHeader.h"

#include "DragonTailCommandIKComponent.h"

#include "IKMapComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Graphics/Resource/GlTFModel.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace
{
    // 尻尾IKではYaw/Pitch/Rollを角度で作り、Quaternionに変換して各ボーンへ分配する。
    constexpr float DEG_TO_RAD = 3.1415926535f / 180.0f;
    constexpr float RAD_TO_DEG = 180.0f / 3.1415926535f;
    constexpr float EPSILON = 0.00001f;
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
        // コマンド開始/終了時の尻尾の動きを滑らかにするための補間関数。
        const float clampedValue_ = std::clamp(value_, 0.0f, 1.0f);
        return clampedValue_ * clampedValue_ * (3.0f - 2.0f * clampedValue_);
    }

    float NormalizeAngleDegrees(float degrees_)
    {
        while (degrees_ > 180.0f)
        {
            degrees_ -= 360.0f;
        }
        while (degrees_ < -180.0f)
        {
            degrees_ += 360.0f;
        }
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

    bool IsFinite(float value_)
    {
        return std::isfinite(value_);
    }

    bool IsFinite(const DirectX::XMFLOAT4& value_)
    {
        return IsFinite(value_.x) && IsFinite(value_.y) && IsFinite(value_.z) && IsFinite(value_.w);
    }

    float LengthSquared(const DirectX::XMVECTOR& value_)
    {
        return DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(value_));
    }

    bool TryNormalizeVector(DirectX::XMVECTOR& value_)
    {
        if (LengthSquared(value_) <= EPSILON * EPSILON)
        {
            return false;
        }
        value_ = DirectX::XMVector3Normalize(value_);
        return true;
    }

    bool TryStoreNormalizedVector(DirectX::XMFLOAT3& result_, DirectX::XMVECTOR value_)
    {
        if (!TryNormalizeVector(value_))
        {
            return false;
        }
        DirectX::XMStoreFloat3(&result_, value_);
        return true;
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
    // 尻尾を動かすモデルとIKチェーンを、コンポーネント開始時に解決する。
    model = owner_ ? owner_->GetComponent<GltfModelCommponennt>().get() : nullptr;
    ikMap = owner_ ? owner_->GetComponent<IKMapComponent>().get() : nullptr;

    if (ikMap)
    {
        ikMap->ResolveChainByName(chainName);
    }

    RegisterActions();
}

bool DragonTailCommandIKComponent::IsLoadReady() const
{
    const auto owner_ = const_cast<DragonTailCommandIKComponent*>(this)->GetGameObject();
    const auto model_ = owner_ ? owner_->GetComponent<GltfModelCommponennt>() : nullptr;
    if (!model_)
    {
        return true;
    }

    model_->UpdateLoadState();
    return !model_->IsModelLoading();
}

void DragonTailCommandIKComponent::LateUpdate(float deltaTime_)
{
    if (!model || !ikMap)
    {
        return;
    }
    model->UpdateLoadState();
    if (!model->IsModelReady())
    {
        return;
    }

    // アニメーション更新後のLateUpdateでIKを重ね、最終的な尻尾姿勢を作る。
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

    // 攻撃コマンドが終わったら、尻尾を通常追従状態へ戻す。
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
    if (!owner_)
    {
        return;
    }

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
    if (!owner_)
    {
        return;
    }

    // AIやアニメーションイベントから尻尾攻撃を発火できるようにActionを登録する。
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
                if (!globalActionName_.empty())
            {
                registeredGlobalActionNames.push_back(globalActionName_);
            }
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
    // 回転薙ぎ払いは根元から動き出し、先端側が遅れて追従するように補正する。
    currentCommand.mode = DRAGON_TAIL_IK_MODE::SPIN_SWEEP;
    currentCommand.duration = duration_;
    currentCommand.sweepAngleDegrees = 120.0f;
    currentCommand.liftAngleDegrees = 12.0f;
    currentCommand.expandAngleDegrees = 20.0f;
    commandTimer = 0.0f;
}

float DragonTailCommandIKComponent::CalculateCommandRate() const
{
    if (currentCommand.duration <= 0.0001f)
    {
        return 1.0f;
    }

    return std::clamp(
        (commandTimer - currentCommand.blendInTime) / currentCommand.duration,
        0.0f,
        1.0f);
}

float DragonTailCommandIKComponent::CalculateSpinLagRate(
    float commandRate_,
    float normalizedPosition_) const
{
    const float lagStartRate_ = std::clamp(spinTipLagStartRate, 0.0f, 0.95f);
    if (normalizedPosition_ <= lagStartRate_)
    {
        return commandRate_;
    }

    const float tipOnlyRate_ =
        (normalizedPosition_ - lagStartRate_) / (1.0f - lagStartRate_);
    const float tipLagRate_ =
        std::clamp(spinTipLagRate, 0.0f, 0.8f) *
        std::clamp(tipOnlyRate_, 0.0f, 1.0f);
    return std::clamp(commandRate_ - tipLagRate_, 0.0f, 1.0f);
}

float DragonTailCommandIKComponent::CalculateBlendRate() const
{
    if (currentCommand.mode == DRAGON_TAIL_IK_MODE::DISABLED)
    {
        return 0.0f;
    }

    // コマンドのblendIn/hold/blendOutに応じてIK影響度を返す。
    if (commandTimer < currentCommand.blendInTime)
    {
        if (currentCommand.blendInTime <= 0.0001f)
        {
            return 1.0f;
        }
        return SmoothStep(commandTimer / currentCommand.blendInTime);
    }

    const float holdEndTime_ = currentCommand.blendInTime + currentCommand.duration;
    if (commandTimer <= holdEndTime_)
    {
        return 1.0f;
    }

    const float blendOutTimer_ = commandTimer - holdEndTime_;
    if (currentCommand.blendOutTime <= 0.0001f)
    {
        return 0.0f;
    }

    return 1.0f - SmoothStep(blendOutTimer_ / currentCommand.blendOutTime);
}

bool DragonTailCommandIKComponent::ApplyTailIK(float deltaTime_)
{
    BS_PROFILE_SCOPE("DragonTailCommandIKComponent::ApplyTailIK");

    auto* chain_ = ikMap->FindChain(chainName);
    if (!chain_ || !chain_->enabled)
    {
        return false;
    }
    if (!ikMap->ResolveChainByName(chainName))
    {
        return false;
    }
    if (chain_->nodeIndices.size() < 2)
    {
        return false;
    }

    // アニメーション姿勢へ尻尾IKを後乗せするため、補正前の回転/World行列を保存する。
    auto& nodes_ = model->GetNodes();
    const size_t boneCount_ = chain_->nodeIndices.size();
    for (const int nodeIndex_ : chain_->nodeIndices)
    {
        if (nodeIndex_ < 0 || nodeIndex_ >= static_cast<int>(nodes_.size()))
        {
            return false;
        }
    }

    std::vector<DirectX::XMFLOAT4> animationRotations_;
    std::vector<DirectX::XMFLOAT4X4> animationWorldTransforms_;
    animationRotations_.reserve(boneCount_);
    animationWorldTransforms_.reserve(boneCount_);

    for (const int nodeIndex_ : chain_->nodeIndices)
    {
        animationRotations_.push_back(nodes_.at(nodeIndex_).rotation);
        animationWorldTransforms_.push_back(nodes_.at(nodeIndex_).worldTransform);
    }

    targetYawDegrees = 0.0f;
    targetPitchDegrees = 0.0f;
    targetRollDegrees = 0.0f;
    const float commandRate_ = CalculateCommandRate();
    const float swingRate_ = std::sin(commandRate_ * 3.1415926535f);

    // コマンド種別ごとに目標Yaw/Pitch/Rollを作る。sin波で中間を一番大きく振る。
    switch (currentCommand.mode)
    {
    case DRAGON_TAIL_IK_MODE::FOLLOW_BODY:
        break;

    case DRAGON_TAIL_IK_MODE::SWEEP_LEFT:
        targetYawDegrees = -currentCommand.sweepAngleDegrees * swingRate_;
        targetPitchDegrees = currentCommand.liftAngleDegrees * swingRate_;
        break;

    case DRAGON_TAIL_IK_MODE::SWEEP_RIGHT:
        targetYawDegrees = currentCommand.sweepAngleDegrees * swingRate_;
        targetPitchDegrees = currentCommand.liftAngleDegrees * swingRate_;
        break;

    case DRAGON_TAIL_IK_MODE::SPIN_SWEEP:
        targetYawDegrees = currentCommand.sweepAngleDegrees * std::sin(commandRate_ * 6.283185307f);
        targetPitchDegrees = currentCommand.liftAngleDegrees * swingRate_;
        targetRollDegrees = currentCommand.expandAngleDegrees * swingRate_;
        break;

    case DRAGON_TAIL_IK_MODE::AIM_TO_TARGET:
    {
        // 対象へ尻尾先端を向けるため、先端の現在前方向とターゲット方向の差分角を求める。
        const auto owner_ = GetGameObject();
        if (!owner_ || currentCommand.targetName.empty())
        {
            break;
        }

        const std::shared_ptr<GameObject> targetObject_ =
            owner_->GetManager().Find(currentCommand.targetName);
        if (!targetObject_ || !targetObject_->transform)
        {
            break;
        }

        const DirectX::XMFLOAT3 targetPosition_ = targetObject_->transform->GetWorldPosition();
        const int aimBaseNodeIndex_ = chain_->nodeIndices[boneCount_ - 2];
        const int aimTipNodeIndex_ = chain_->nodeIndices[boneCount_ - 1];
        const auto& aimBaseNode_ = nodes_.at(aimBaseNodeIndex_);
        const auto& aimTipNode_ = nodes_.at(aimTipNodeIndex_);

        const DirectX::XMMATRIX aimBaseWorldMatrix_ =
            DirectX::XMLoadFloat4x4(&aimBaseNode_.worldTransform);
        const DirectX::XMMATRIX inverseAimBaseWorldMatrix_ =
            DirectX::XMMatrixInverse(nullptr, aimBaseWorldMatrix_);
        const DirectX::XMMATRIX aimTipWorldMatrix_ =
            DirectX::XMLoadFloat4x4(&aimTipNode_.worldTransform);

        const DirectX::XMFLOAT3 aimTipWorldPosition_ = GetTranslation(aimTipNode_.worldTransform);
        DirectX::XMVECTOR currentForwardWorld_ = DirectX::XMVector3TransformNormal(
            DirectX::XMLoadFloat3(&localForwardAxis),
            aimTipWorldMatrix_);
        DirectX::XMVECTOR targetDirectionWorld_ = DirectX::XMVectorSubtract(
            DirectX::XMLoadFloat3(&targetPosition_),
            DirectX::XMLoadFloat3(&aimTipWorldPosition_));

        if (TryNormalizeVector(currentForwardWorld_) &&
            TryNormalizeVector(targetDirectionWorld_))
        {
            DirectX::XMFLOAT3 currentForwardInAimBase_;
            DirectX::XMFLOAT3 targetDirectionInAimBase_;

            if (TryStoreNormalizedVector(
                currentForwardInAimBase_,
                DirectX::XMVector3TransformNormal(currentForwardWorld_, inverseAimBaseWorldMatrix_)) &&
                TryStoreNormalizedVector(
                    targetDirectionInAimBase_,
                    DirectX::XMVector3TransformNormal(targetDirectionWorld_, inverseAimBaseWorldMatrix_)))
            {
                targetYawDegrees = std::clamp(
                    NormalizeAngleDegrees(
                        CalculateYawDegrees(targetDirectionInAimBase_) -
                        CalculateYawDegrees(currentForwardInAimBase_)),
                    -currentCommand.sweepAngleDegrees,
                    currentCommand.sweepAngleDegrees);
                targetPitchDegrees = std::clamp(
                    NormalizeAngleDegrees(
                        CalculatePitchDegrees(targetDirectionInAimBase_) -
                        CalculatePitchDegrees(currentForwardInAimBase_)),
                    -currentCommand.liftAngleDegrees,
                    currentCommand.liftAngleDegrees);
            }
        }
        break;
    }

    default:
        break;
    }

    const float safeDeltaTime_ = std::clamp(deltaTime_, 0.0f, 0.1f);
    const float followRate_ =
        1.0f - std::exp(-(std::max)(followSpeed, 0.0f) * safeDeltaTime_);

    // 目標角へ指数補間し、コマンド開始直後や終了時の角度変化をなめらかにする。
    currentYawDegrees +=
        NormalizeAngleDegrees(targetYawDegrees - currentYawDegrees) * followRate_;
    currentPitchDegrees +=
        (targetPitchDegrees - currentPitchDegrees) * followRate_;
    currentRollDegrees +=
        NormalizeAngleDegrees(targetRollDegrees - currentRollDegrees) * followRate_;

    if (std::abs(currentYawDegrees) < 0.01f &&
        std::abs(currentPitchDegrees) < 0.01f &&
        std::abs(currentRollDegrees) < 0.01f &&
        (currentCommand.mode == DRAGON_TAIL_IK_MODE::DISABLED ||
            currentCommand.mode == DRAGON_TAIL_IK_MODE::FOLLOW_BODY))
    {
        currentYawDegrees = 0.0f;
        currentPitchDegrees = 0.0f;
        currentRollDegrees = 0.0f;
    }

    if (!IsFinite(currentYawDegrees) ||
        !IsFinite(currentPitchDegrees) ||
        !IsFinite(currentRollDegrees))
    {
        currentYawDegrees = 0.0f;
        currentPitchDegrees = 0.0f;
        currentRollDegrees = 0.0f;
        return false;
    }

    const float commandInfluence_ =
        currentCommand.mode == DRAGON_TAIL_IK_MODE::DISABLED ||
        currentCommand.mode == DRAGON_TAIL_IK_MODE::FOLLOW_BODY
        ? 1.0f
        : CalculateBlendRate();
    const float influence_ = commandInfluence_ * std::clamp(chain_->influence, 0.0f, 1.0f);

    if (currentCommand.mode != DRAGON_TAIL_IK_MODE::DISABLED &&
        currentCommand.mode != DRAGON_TAIL_IK_MODE::FOLLOW_BODY &&
        commandInfluence_ <= 0.0001f &&
        commandTimer >= currentCommand.blendInTime + currentCommand.duration)
    {
        currentYawDegrees = 0.0f;
        currentPitchDegrees = 0.0f;
        currentRollDegrees = 0.0f;
        return false;
    }

    if (influence_ <= 0.0001f ||
        (std::abs(currentYawDegrees) < 0.01f &&
            std::abs(currentPitchDegrees) < 0.01f &&
            std::abs(currentRollDegrees) < 0.01f))
    {
        return false;
    }

    const DirectX::XMMATRIX aimBaseWorldMatrix_ =
        DirectX::XMLoadFloat4x4(&nodes_.at(chain_->nodeIndices[boneCount_ - 2]).worldTransform);
    DirectX::XMVECTOR aimYawAxisWorld_ = DirectX::XMVector3TransformNormal(
        DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
        aimBaseWorldMatrix_);
    DirectX::XMVECTOR aimPitchAxisWorld_ = DirectX::XMVector3TransformNormal(
        DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),
        aimBaseWorldMatrix_);
    DirectX::XMVECTOR aimRollAxisWorld_ = DirectX::XMVector3TransformNormal(
        DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
        aimBaseWorldMatrix_);

    if (!TryNormalizeVector(aimYawAxisWorld_) ||
        !TryNormalizeVector(aimPitchAxisWorld_) ||
        !TryNormalizeVector(aimRollAxisWorld_))
    {
        return false;
    }

    std::vector<float> weights_(boneCount_, 0.0f);
    float weightTotal_ = 0.0f;
    // rootWeightからtipWeightへ二乗カーブで寄せ、先端側を強く動かす。
    for (size_t boneIndex_ = 0; boneIndex_ < boneCount_; ++boneIndex_)
    {
        const float normalizedPosition_ = boneCount_ <= 1
            ? 1.0f
            : static_cast<float>(boneIndex_) / static_cast<float>(boneCount_ - 1);
        const float tipBias_ = normalizedPosition_ * normalizedPosition_;
        weights_[boneIndex_] =
            currentCommand.rootWeight +
            (currentCommand.tipWeight - currentCommand.rootWeight) * tipBias_;
        weightTotal_ += weights_[boneIndex_];
    }

    if (weightTotal_ <= EPSILON)
    {
        return false;
    }

    for (float& weight_ : weights_)
    {
        weight_ /= weightTotal_;
    }

    for (size_t boneIndex_ = 0; boneIndex_ < boneCount_; ++boneIndex_)
    {
        const int nodeIndex_ = chain_->nodeIndices[boneIndex_];
        auto& node_ = nodes_.at(nodeIndex_);

        const DirectX::XMFLOAT4X4* parentWorldTransform_ = nullptr;
        if (node_.parentIndex >= 0)
        {
            const auto parentIterator_ = std::find(
                chain_->nodeIndices.begin(),
                chain_->nodeIndices.end(),
                node_.parentIndex);

            if (parentIterator_ != chain_->nodeIndices.end())
            {
                parentWorldTransform_ =
                    &animationWorldTransforms_[std::distance(chain_->nodeIndices.begin(), parentIterator_)];
            }
            else if (node_.parentIndex < static_cast<int>(nodes_.size()))
            {
                parentWorldTransform_ = &nodes_.at(node_.parentIndex).worldTransform;
            }
        }

        DirectX::XMMATRIX parentWorldMatrix_ = DirectX::XMMatrixIdentity();
        if (parentWorldTransform_)
        {
            parentWorldMatrix_ = DirectX::XMLoadFloat4x4(parentWorldTransform_);
        }

        const DirectX::XMMATRIX inverseParentWorldMatrix_ =
            DirectX::XMMatrixInverse(nullptr, parentWorldMatrix_);
        // 基準軸を各ボーンの親ローカルへ変換し、ローカルQuaternionとして適用できる形にする。
        DirectX::XMVECTOR boneYawAxisLocal_ =
            DirectX::XMVector3TransformNormal(aimYawAxisWorld_, inverseParentWorldMatrix_);
        DirectX::XMVECTOR bonePitchAxisLocal_ =
            DirectX::XMVector3TransformNormal(aimPitchAxisWorld_, inverseParentWorldMatrix_);
        DirectX::XMVECTOR boneRollAxisLocal_ =
            DirectX::XMVector3TransformNormal(aimRollAxisWorld_, inverseParentWorldMatrix_);

        if (!TryNormalizeVector(boneYawAxisLocal_) ||
            !TryNormalizeVector(bonePitchAxisLocal_) ||
            !TryNormalizeVector(boneRollAxisLocal_))
        {
            continue;
        }

        const float normalizedPosition_ = boneCount_ <= 1
            ? 1.0f
            : static_cast<float>(boneIndex_) / static_cast<float>(boneCount_ - 1);
        float yawDegrees_ = currentYawDegrees;
        float pitchDegrees_ = currentPitchDegrees;
        float rollDegrees_ = currentRollDegrees;
        if (currentCommand.mode == DRAGON_TAIL_IK_MODE::SPIN_SWEEP)
        {
            // 根元側は通常通り追従させ、指定位置より先だけ回転タイミングを遅らせる。
            const float laggedCommandRate_ =
                CalculateSpinLagRate(commandRate_, normalizedPosition_);
            const float laggedSwingRate_ =
                std::sin(laggedCommandRate_ * 3.1415926535f);
            yawDegrees_ =
                currentCommand.sweepAngleDegrees *
                std::sin(laggedCommandRate_ * 6.283185307f);
            pitchDegrees_ = currentCommand.liftAngleDegrees * laggedSwingRate_;
            rollDegrees_ = currentCommand.expandAngleDegrees * laggedSwingRate_;
        }

        const float yawRadians_ = yawDegrees_ * DEG_TO_RAD * weights_[boneIndex_] * influence_;
        const float pitchRadians_ = pitchDegrees_ * DEG_TO_RAD * weights_[boneIndex_] * influence_;
        const float rollRadians_ = rollDegrees_ * DEG_TO_RAD * weights_[boneIndex_] * influence_;

        const DirectX::XMVECTOR animationRotation_ =
            DirectX::XMLoadFloat4(&animationRotations_[boneIndex_]);

        const DirectX::XMVECTOR yawRotation_ = DirectX::XMQuaternionRotationAxis(
            boneYawAxisLocal_,
            yawRadians_);

        const DirectX::XMVECTOR pitchRotation_ = DirectX::XMQuaternionRotationAxis(
            bonePitchAxisLocal_,
            pitchRadians_);

        const DirectX::XMVECTOR rollRotation_ = DirectX::XMQuaternionRotationAxis(
            boneRollAxisLocal_,
            rollRadians_);

        const DirectX::XMVECTOR commandRotation_ = DirectX::XMQuaternionMultiply(
            rollRotation_,
            DirectX::XMQuaternionMultiply(pitchRotation_, yawRotation_));

        const DirectX::XMVECTOR resultRotation_ = DirectX::XMQuaternionNormalize(
            DirectX::XMQuaternionMultiply(animationRotation_, commandRotation_));

        // 元のアニメーション回転へコマンド回転を重ね、チェーン先頭からTransformを更新する。
        DirectX::XMFLOAT4 resultRotationStorage_;
        DirectX::XMStoreFloat4(&resultRotationStorage_, resultRotation_);
        if (!IsFinite(resultRotationStorage_))
        {
            continue;
        }

        node_.rotation = resultRotationStorage_;
        model->UpdateNodeTransformOnly(nodeIndex_);
    }

    model->UpdateNodeTransformFrom(chain_->nodeIndices.front());
    return true;
}

bool DragonTailCommandIKComponent::GetTailTipWorldPosition(DirectX::XMFLOAT3& position_) const
{
    // 尻尾先端に攻撃判定やエフェクトを出すため、チェーン末端ノードのWorld座標を返す。
    if (!model || !ikMap)
    {
        return false;
    }

    const auto* chain_ = ikMap->FindChain(chainName);
    if (!chain_ || chain_->nodeIndices.empty())
    {
        return false;
    }

    const int tipNodeIndex_ = chain_->nodeIndices.back();
    const auto& nodes_ = model->GetNodes();

    if (tipNodeIndex_ < 0 || tipNodeIndex_ >= static_cast<int>(nodes_.size()))
    {
        return false;
    }

    position_ = GetTranslation(nodes_.at(tipNodeIndex_).worldTransform);
    return true;
}

// 尻尾IKと同じチェーンから根元を取得し、判定とモデルの参照先を揃える。
bool DragonTailCommandIKComponent::GetTailRootWorldPosition(DirectX::XMFLOAT3& position_) const
{
    if (!model || !ikMap)
    {
        return false;
    }
    const auto* chain_ = ikMap->FindChain(chainName);
    if (!chain_ || chain_->nodeIndices.empty())
    {
        return false;
    }

    const int rootNodeIndex_ = chain_->nodeIndices.front();
    const auto& nodes_ = model->GetNodes();
    if (rootNodeIndex_ < 0 || rootNodeIndex_ >= static_cast<int>(nodes_.size()))
    {
        return false;
    }
    position_ = GetTranslation(nodes_[rootNodeIndex_].worldTransform);
    return true;
}

bool DragonTailCommandIKComponent::GetTailWorldPositions(std::vector<DirectX::XMFLOAT3>& positions_) const
{
    BS_PROFILE_SCOPE("DragonTailCommandIKComponent::GetTailWorldPositions");
    positions_.clear();
    if (!model || !ikMap)
    {
        return false;
    }
    const auto chain_ = ikMap->FindChain(chainName);
    if (!chain_ || chain_->nodeIndices.size() < 2)
    {
        return false;
    }
    const auto& nodes_ = model->GetNodes();
    positions_.reserve(chain_->nodeIndices.size());
    for (const int nodeIndex_ : chain_->nodeIndices)
    {
        if (nodeIndex_ < 0 || nodeIndex_ >= static_cast<int>(nodes_.size()))
        {
            positions_.clear();
            return false;
        }
        positions_.push_back(GetTranslation(nodes_[nodeIndex_].worldTransform));
    }
    return true;
}

bool DragonTailCommandIKComponent::GetTailTipWorldForward(DirectX::XMFLOAT3& forward_) const
{
    // 尻尾先端の向きを、エフェクトや判定方向へ渡せるWorld前方向として返す。
    if (!model || !ikMap)
    {
        return false;
    }

    const auto* chain_ = ikMap->FindChain(chainName);
    if (!chain_ || chain_->nodeIndices.empty())
    {
        return false;
    }

    const int tipNodeIndex_ = chain_->nodeIndices.back();
    const auto& nodes_ = model->GetNodes();

    if (tipNodeIndex_ < 0 || tipNodeIndex_ >= static_cast<int>(nodes_.size()))
    {
        return false;
    }

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
    ImGui::DragFloat("Follow Speed", &followSpeed, 0.1f, 0.1f, 30.0f);
    ImGui::DragFloat("Spin Tip Lag Rate", &spinTipLagRate, 0.01f, 0.0f, 0.8f);
    ImGui::DragFloat("Spin Tip Lag Start", &spinTipLagStartRate, 0.01f, 0.0f, 0.95f);
    ImGui::Text("Target Yaw: %.2f", targetYawDegrees);
    ImGui::Text("Target Pitch: %.2f", targetPitchDegrees);
    ImGui::Text("Target Roll: %.2f", targetRollDegrees);
    ImGui::Text("Current Yaw: %.2f", currentYawDegrees);
    ImGui::Text("Current Pitch: %.2f", currentPitchDegrees);
    ImGui::Text("Current Roll: %.2f", currentRollDegrees);
    if (ikMap)
    {
        const auto* chain_ = ikMap->FindChain(chainName);
        ImGui::Text("Chain Bones: %d", chain_ ? static_cast<int>(chain_->nodeIndices.size()) : 0);
    }

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
    // 尻尾IKの対象チェーン、Action登録設定、追従速度、回転攻撃の先端遅れ量を保存する。
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
    json_["FollowSpeed"] = followSpeed;
    json_["SpinTipLagRate"] = spinTipLagRate;
    json_["SpinTipLagStartRate"] = spinTipLagStartRate;
}

void DragonTailCommandIKComponent::Deserialize(const nlohmann::json& json_)
{
    // 保存された調整値を復元し、Action登録も現在のカテゴリで作り直す。
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
    followSpeed = json_.value("FollowSpeed", 6.0f);
    spinTipLagRate = json_.value("SpinTipLagRate", 0.18f);
    spinTipLagStartRate = json_.value("SpinTipLagStartRate", 0.55f);

    RegisterActions();
}

REGISTER_COMPONENT(DragonTailCommandIKComponent, "DragonTailCommandIKComponent")
