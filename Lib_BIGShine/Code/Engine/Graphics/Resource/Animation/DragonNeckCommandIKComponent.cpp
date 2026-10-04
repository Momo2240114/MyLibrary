#include "PrecompiledHeader.h"

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
    // 首IKは角度指定とQuaternion生成を行き来するため、度数法/ラジアン変換を固定で持つ。
    constexpr float DEG_TO_RAD = 3.1415926535f / 180.0f;
    constexpr float RAD_TO_DEG = 180.0f / 3.1415926535f;
    constexpr float EPSILON = 0.00001f;
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
        // 無効な方向ベクトルは、首の向き計算に使わないようゼロで返す。
        const float length_ = std::sqrt(
            value_.x * value_.x +
            value_.y * value_.y +
            value_.z * value_.z);

        if (length_ <= EPSILON) return { 0.0f, 0.0f, 0.0f };

        return {
            value_.x / length_,
            value_.y / length_,
            value_.z / length_
        };
    }

    float SmoothStep(float value_)
    {
        // 首の向きが急に切り替わらないよう、ブレンド率に滑らかな立ち上がり/終わりを付ける。
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

    bool IsFinite(float value_)
    {
        return std::isfinite(value_);
    }

    bool IsFinite(const DirectX::XMFLOAT3& value_)
    {
        return IsFinite(value_.x) && IsFinite(value_.y) && IsFinite(value_.z);
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
        if (LengthSquared(value_) <= EPSILON * EPSILON) return false;
        value_ = DirectX::XMVector3Normalize(value_);
        return true;
    }

    bool TryStoreNormalizedVector(DirectX::XMFLOAT3& result_, DirectX::XMVECTOR value_)
    {
        if (!TryNormalizeVector(value_)) return false;
        DirectX::XMStoreFloat3(&result_, value_);
        return IsFinite(result_);
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
    // 首IKはモデルのノード列とIKMapのチェーン定義を使って動くため、開始時に参照を解決する。
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

bool DragonNeckCommandIKComponent::IsLoadReady() const
{
    const auto owner_ = const_cast<DragonNeckCommandIKComponent*>(this)->GetGameObject();
    const auto model_ = owner_ ? owner_->GetComponent<GltfModelCommponennt>() : nullptr;
    if (!model_) return true;

    model_->UpdateLoadState();
    return !model_->IsModelLoading();
}

void DragonNeckCommandIKComponent::Update(float deltaTime_)
{
    if (!model || !ikMap) return;
    model->UpdateLoadState();
    if (!model->IsModelReady()) return;

    // 現在のコマンドを毎フレーム評価し、指定時間が終わったら必要に応じて解除する。
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

void DragonNeckCommandIKComponent::LateUpdate(float deltaTime_)
{
    (void)deltaTime_;
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

    // アニメイベントやAIから首の向きを命令できるよう、ローカル/必要ならグローバルActionへ登録する。
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

    // ターゲット注視モードは長時間のLOOK_AT_TARGETコマンドとして扱い、
    // 毎フレーム対象位置と制限角度だけ更新する。
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

    if (currentCommand.mode != DRAGON_NECK_IK_MODE::DISABLED &&
        !currentCommand.keepLastDirection)
    {
        // アニメイベントなどの一時Command中は、通常のPlayer注視で上書きしない。
        return;
    }

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

    // blendIn -> hold -> blendOutの3区間で、IKの影響度を0〜1へ変換する。
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
    // ワールド座標指定とGameObject名指定を、同じtargetPositionとして扱えるように解決する。
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
    auto* chain_ = ikMap->FindChain(chainName);
    if (!chain_ || !chain_->enabled) return false;
    if (!ikMap->ResolveChainByName(chainName)) return false;
    if (chain_->nodeIndices.size() < 2) return false;

    // アニメーション結果を基準にIK補正を重ねるため、補正前の回転とWorld行列を保持する。
    auto& nodes_ = model->GetNodes();

    const size_t boneCount_ = chain_->nodeIndices.size();
    for (const int nodeIndex_ : chain_->nodeIndices)
    {
        if (nodeIndex_ < 0 || nodeIndex_ >= static_cast<int>(nodes_.size())) return false;
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

    if (currentCommand.mode == DRAGON_NECK_IK_MODE::FIXED_YAW_OFFSET)
    {
        targetYawDegrees = currentCommand.startYawDegrees;
        targetPitchDegrees = currentCommand.pitchOffsetDegrees;
    }
    else if (currentCommand.mode == DRAGON_NECK_IK_MODE::SWEEP_YAW)
    {
        // Sweepは開始Yawから終了Yawへ、コマンド時間内で滑らかに首を振る。
        const float t_ = currentCommand.duration <= 0.0001f
            ? 1.0f
            : std::clamp(
                (commandTimer - currentCommand.blendInTime) / currentCommand.duration,
                0.0f,
                1.0f);

        targetYawDegrees =
            currentCommand.startYawDegrees +
            (currentCommand.endYawDegrees - currentCommand.startYawDegrees) * SmoothStep(t_);
        targetPitchDegrees = currentCommand.pitchOffsetDegrees;
    }
    else if (currentCommand.mode == DRAGON_NECK_IK_MODE::LOOK_AT_TARGET ||
        currentCommand.mode == DRAGON_NECK_IK_MODE::LOOK_AT_WORLD_POSITION)
    {
        DirectX::XMFLOAT3 targetPosition_;
        if (ResolveTargetPosition(targetPosition_))
        {
            // 先端ノードの現在向きとターゲット方向を、首の根元基準のローカル方向へ変換して差分角を出す。
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
                    targetYawDegrees =
                        NormalizeAngleDegrees(
                            CalculateYawDegrees(targetDirectionInAimBase_) -
                            CalculateYawDegrees(currentForwardInAimBase_)) +
                        currentCommand.yawOffsetDegrees;

                    targetPitchDegrees =
                        NormalizeAngleDegrees(
                            CalculatePitchDegrees(targetDirectionInAimBase_) -
                            CalculatePitchDegrees(currentForwardInAimBase_)) +
                        currentCommand.pitchOffsetDegrees;
                }
            }
        }
    }

    // 首が破綻するほど回らないよう、コマンドごとの最大Yaw/Pitchに制限する。
    targetYawDegrees = std::clamp(
        targetYawDegrees,
        -currentCommand.maxYawDegrees,
        currentCommand.maxYawDegrees);

    targetPitchDegrees = std::clamp(
        targetPitchDegrees,
        -currentCommand.maxPitchDegrees,
        currentCommand.maxPitchDegrees);

    const float safeDeltaTime_ = std::clamp(deltaTime_, 0.0f, 0.1f);
    const float followRate_ =
        1.0f - std::exp(-(std::max)(followSpeed, 0.0f) * safeDeltaTime_);

    // 目標角度へ指数補間で追従させ、コマンド切り替え時の首の跳ねを抑える。
    currentYawDegrees +=
        NormalizeAngleDegrees(targetYawDegrees - currentYawDegrees) * followRate_;
    currentPitchDegrees +=
        (targetPitchDegrees - currentPitchDegrees) * followRate_;

    if (std::abs(currentYawDegrees) < 0.01f &&
        std::abs(currentPitchDegrees) < 0.01f &&
        currentCommand.mode == DRAGON_NECK_IK_MODE::DISABLED)
    {
        currentYawDegrees = 0.0f;
        currentPitchDegrees = 0.0f;
    }

    if (!IsFinite(currentYawDegrees) || !IsFinite(currentPitchDegrees))
    {
        currentYawDegrees = 0.0f;
        currentPitchDegrees = 0.0f;
        return false;
    }

    const float commandInfluence_ = currentCommand.mode == DRAGON_NECK_IK_MODE::DISABLED
        ? 1.0f
        : CalculateBlendRate();
    const float influence_ = commandInfluence_ * std::clamp(chain_->influence, 0.0f, 1.0f);

    if (currentCommand.mode != DRAGON_NECK_IK_MODE::DISABLED &&
        commandInfluence_ <= 0.0001f &&
        commandTimer >= currentCommand.blendInTime + currentCommand.duration)
    {
        currentYawDegrees = 0.0f;
        currentPitchDegrees = 0.0f;
        return false;
    }

    if (influence_ <= 0.0001f ||
        (std::abs(currentYawDegrees) < 0.01f && std::abs(currentPitchDegrees) < 0.01f))
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

    if (!TryNormalizeVector(aimYawAxisWorld_) || !TryNormalizeVector(aimPitchAxisWorld_)) return false;

    std::vector<float> yawWeights_(boneCount_, 0.0f);
    std::vector<float> pitchWeights_(boneCount_, 0.0f);
    float yawWeightTotal_ = 0.0f;
    float pitchWeightTotal_ = 0.0f;

    // 根元より先端側を強めに回すことで、首全体が自然にしなって目標方向へ向く。
    for (size_t boneIndex_ = 0; boneIndex_ < boneCount_; ++boneIndex_)
    {
        const float normalizedPosition_ = boneCount_ <= 1
            ? 1.0f
            : static_cast<float>(boneIndex_) / static_cast<float>(boneCount_ - 1);

        yawWeights_[boneIndex_] = 0.25f + normalizedPosition_ * normalizedPosition_;
        pitchWeights_[boneIndex_] =
            0.10f + normalizedPosition_ * normalizedPosition_ * normalizedPosition_;
        yawWeightTotal_ += yawWeights_[boneIndex_];
        pitchWeightTotal_ += pitchWeights_[boneIndex_];
    }

    for (size_t boneIndex_ = 0; boneIndex_ < boneCount_; ++boneIndex_)
    {
        if (yawWeightTotal_ > EPSILON) yawWeights_[boneIndex_] /= yawWeightTotal_;
        if (pitchWeightTotal_ > EPSILON) pitchWeights_[boneIndex_] /= pitchWeightTotal_;
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
        // ワールド基準のYaw/Pitch軸を各ボーンの親ローカルへ変換し、ローカル回転として合成する。
        DirectX::XMVECTOR boneYawAxisLocal_ =
            DirectX::XMVector3TransformNormal(aimYawAxisWorld_, inverseParentWorldMatrix_);
        DirectX::XMVECTOR bonePitchAxisLocal_ =
            DirectX::XMVector3TransformNormal(aimPitchAxisWorld_, inverseParentWorldMatrix_);

        if (!TryNormalizeVector(boneYawAxisLocal_) || !TryNormalizeVector(bonePitchAxisLocal_)) continue;

        const float yawRadians_ = currentYawDegrees * DEG_TO_RAD * yawWeights_[boneIndex_] * influence_;
        const float pitchRadians_ = currentPitchDegrees * DEG_TO_RAD * pitchWeights_[boneIndex_] * influence_;

        const DirectX::XMVECTOR animationRotation_ =
            DirectX::XMLoadFloat4(&animationRotations_[boneIndex_]);
        const DirectX::XMVECTOR yawRotation_ = DirectX::XMQuaternionRotationAxis(
            boneYawAxisLocal_,
            yawRadians_);

        const DirectX::XMVECTOR pitchRotation_ = DirectX::XMQuaternionRotationAxis(
            bonePitchAxisLocal_,
            pitchRadians_);

        const DirectX::XMVECTOR commandRotation_ =
            DirectX::XMQuaternionMultiply(pitchRotation_, yawRotation_);
        const DirectX::XMVECTOR resultRotation_ = DirectX::XMQuaternionNormalize(
            DirectX::XMQuaternionMultiply(animationRotation_, commandRotation_));

        // アニメーション回転へIKの追加回転を重ね、ノード行列を先頭から再計算する。
        DirectX::XMFLOAT4 resultRotationStorage_;
        DirectX::XMStoreFloat4(&resultRotationStorage_, resultRotation_);
        if (!IsFinite(resultRotationStorage_)) continue;

        node_.rotation = resultRotationStorage_;
        model->UpdateNodeTransformOnly(nodeIndex_);
    }

    model->UpdateNodeTransformFrom(chain_->nodeIndices.front());
    return true;
}

bool DragonNeckCommandIKComponent::GetMouthWorldPosition(DirectX::XMFLOAT3& position_) const
{
    // ブレスや口元エフェクトの発生位置として、設定されたMouthノードのWorld座標を返す。
    if (!model || mouthNodeIndex < 0) return false;

    const auto& nodes_ = model->GetNodes();
    if (mouthNodeIndex >= static_cast<int>(nodes_.size())) return false;

    position_ = GetTranslation(nodes_.at(mouthNodeIndex).worldTransform);
    return true;
}

bool DragonNeckCommandIKComponent::GetMouthWorldForward(DirectX::XMFLOAT3& forward_) const
{
    // 口元エフェクトの向きに使うため、MouthノードのWorld行列へローカル前方向を変換する。
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
    ImGui::DragFloat("Follow Speed", &followSpeed, 0.1f, 0.1f, 30.0f);
    ImGui::Text("Target Yaw: %.2f", targetYawDegrees);
    ImGui::Text("Target Pitch: %.2f", targetPitchDegrees);
    ImGui::Text("Current Yaw: %.2f", currentYawDegrees);
    ImGui::Text("Current Pitch: %.2f", currentPitchDegrees);
    if (ikMap)
    {
        const auto* chain_ = ikMap->FindChain(chainName);
        ImGui::Text("Chain Bones: %d", chain_ ? static_cast<int>(chain_->nodeIndices.size()) : 0);
        if (chain_ && chain_->nodeIndices.size() >= 2)
        {
            ImGui::Text("Aim Base Index: %d", chain_->nodeIndices[chain_->nodeIndices.size() - 2]);
            ImGui::Text("Aim Tip Index: %d", chain_->nodeIndices.back());
        }
    }
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
    // チェーン名、口ノード、Action設定、注視モードの調整値を保存する。
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
    json_["FollowSpeed"] = followSpeed;
}

void DragonNeckCommandIKComponent::Deserialize(const nlohmann::json& json_)
{
    // 保存値を読み戻し、注視モードが有効ならコマンドも再作成しておく。
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
    followSpeed = json_.value("FollowSpeed", 6.0f);

    RegisterActions();
    if (lookTargetObjectMode)
    {
        SetTargetObjectLookMode(true);
    }
}

REGISTER_COMPONENT(DragonNeckCommandIKComponent, "DragonNeckCommandIKComponent")
