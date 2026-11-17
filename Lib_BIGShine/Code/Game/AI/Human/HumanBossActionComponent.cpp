#include "PrecompiledHeader.h"
#include "HumanBossActionComponent.h"
#include "HumanBossAIComponent.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Game/Charcter/Player/PlayerControllerComponent.h"
#include "Game/GameMode/GameModeBlackboardKeys.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/Graphics/Resource/Animation/AnimationGraphBridgeComponent.h"
#include "Engine/Graphics/Resource/Animation/AnimationGraphBlackboardKeys.h"
#include "Engine/Physics/GuardCollider.h"
#include "Engine/Physics/HurtBox.h"
#include "Engine/GamePlay/Combat/HitReactionComponent.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/GamePlay/Combat/AttackEffectHitComponent.h"
#include "Engine/GamePlay/Combat/ProjectileMotionComponent.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/Physics/HitBox.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/System/EffectManager.h"
#include "Engine/GamePlay/System/JustGuardSystem.h"
#include "Engine/GamePlay/System/JustDodgeSystem.h"
#include "Engine/Graphics/VFX/WeaponTrailComponent.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"
#include "Engine/GamePlay/Core/Component/MotionWarpComponent.h"
#include "Engine/GamePlay/Combat/AttackTemplateManager.h"
#include "Engine/Physics/CharacterCollider.h"
#include "Engine/Physics/PhysXSystem.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Math/Curve/CurveComponent.h"
#include "Engine/GamePlay/VFX/DangerAreaComponent.h"
#include "Engine/GamePlay/System/CombatObservationSystem.h"
#include "Engine/GamePlay/System/TimeManager.h"
#include "Engine/GamePlay/Input/ControllerInput.h"
#include "Engine/GamePlay/Camera/CameraActionDirectorComponent.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Camera/CameraManager.h"
#include "Engine/GamePlay/Camera/CameraShakeComponent.h"
#include <functional>
#include <imgui_stdlib.h>
#include <cmath>
#include <algorithm>
#include <limits>
#include <utility>

namespace
{
    // 表示モデルが子Objectでも、実際に移動を担当するCharacterColliderを親階層から取得する。
    std::shared_ptr<CharacterCollider> ResolveAttackCharacter(std::shared_ptr<GameObject> object_)
    {
        while (object_)
        {
            if (const auto collider_ = object_->GetComponent<CharacterCollider>()) return collider_;
            const auto parent_ = object_->transform ? object_->transform->parent.lock() : nullptr;
            object_ = parent_ ? parent_->gameObject.lock() : nullptr;
        }
        return nullptr;
    }
}

void HumanBossActionComponent::InitializeAttackAssist()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::InitializeAttackAssist");
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    const auto warp_ = owner_->GetOrAddComponent<MotionWarpComponent>(false);
    if (!warp_) return;
    warp_->SetRegisterGlobalActions(false);
    // 保存された調整値を維持し、敵用の未設定プリセットだけを追加する。
    for (int index_ = 0; index_ < 7; ++index_)
    {
        constexpr const char* NAMES[] = { "HumanBoss.ComboFirst", "HumanBoss.ComboSecond", "HumanBoss.Heavy", "HumanBoss.GuardBreak", "HumanBoss.Counter", "HumanBoss.Punish", "HumanBoss.Delay" };
        constexpr float RANGES[] = { 5.0f, 5.0f, 6.0f, 4.0f, 3.5f, 4.5f, 4.5f };
        constexpr float CORRECTIONS[] = { 4.2f, 0.45f, 4.8f, 2.6f, 2.2f, 3.0f, 3.0f };
        MotionWarpPreset preset_;
        preset_.presetName = NAMES[index_];
        preset_.targetMode = MOTION_WARP_TARGET_MODE::GAME_OBJECT;
        preset_.positionMode = MOTION_WARP_POSITION_MODE::TARGET_END_POSITION;
        preset_.startNormalizedTime = 0.0f;
        preset_.endNormalizedTime = 0.45f;
        preset_.targetDistance = index_ == 2 || index_ == 3 ? 1.6f : 1.5f;
        preset_.positionStrength = 1.0f;
        preset_.rotationStrength = index_ == 1 ? 0.7f : 0.8f;
        preset_.maximumPositionCorrectionPerFrame = index_ == 1 ? 0.35f : 0.4f;
        preset_.maximumPositionCorrectionPerSecond = 24.0f;
        preset_.maximumTotalPositionCorrection = index_ == 1 ? 3.0f : CORRECTIONS[index_];
        preset_.maximumWarpTargetDistance = RANGES[index_];
        preset_.maximumRotationDegrees = index_ == 1 ? 45.0f : 90.0f;
        preset_.maximumRotationDegreesPerSecond = 600.0f;
        preset_.lockTargetOnStart = index_ == 1;
        preset_.lockTargetNormalizedTime = index_ == 2 ? 0.26f : 0.04f;
        preset_.maximumTargetTravelDistance = index_ == 1 ? 0.0f : 0.6f;
        preset_.maximumTargetTravelAngleDegrees = index_ == 1 ? 0.0f : 20.0f;
        preset_.rotationStartNormalizedTime = 0.0f;
        preset_.rotationFullStrengthEndNormalizedTime = index_ == 2 ? 0.3f : 0.04f;
        preset_.rotationEndNormalizedTime = 0.45f;
        preset_.positionCorrectionCurveName = "HumanBoss.WarpCorrection";
        warp_->AddPresetIfMissing(preset_);
    }
    // 長距離行動専用。TargetはPhase開始時に固定し、既存の近接補正は変更しない。
    MotionWarpPreset charge_;
    charge_.presetName = "HumanBoss.ShadowCharge";
    charge_.targetMode = MOTION_WARP_TARGET_MODE::GAME_OBJECT;
    charge_.positionMode = MOTION_WARP_POSITION_MODE::TARGET_END_POSITION;
    charge_.rotationMode = MOTION_WARP_ROTATION_MODE::NONE;
    charge_.startNormalizedTime = 0.0f;
    charge_.endNormalizedTime = CHARGE_WARP_END_NORMALIZED_TIME;
    charge_.targetDistance = 0.0f;
    charge_.warpRotation = false;
    charge_.lockTargetOnStart = true;
    charge_.maximumTargetTravelDistance = 0.0f;
    charge_.maximumTargetTravelAngleDegrees = 0.0f;
    // 延長した突進を旧上限で拒否しないよう、専用の補正上限も1.5倍にそろえる。
    charge_.maximumWarpTargetDistance = 24.0f;
    charge_.maximumTotalPositionCorrection = 21.0f;
    charge_.maximumPositionCorrectionPerSecond = 27.0f;
    charge_.maximumPositionCorrectionPerFrame = 0.525f;
    charge_.positionCorrectionCurveName = "HumanBoss.ChargeTravel";
    charge_.minimumPositionScale = 1.0f;
    charge_.maximumPositionScale = 1.0f;
    warp_->AddPresetIfMissing(charge_);
}

bool HumanBossActionComponent::CanWarpPath(const DirectX::XMFLOAT3& start_, const DirectX::XMFLOAT3& end_) const
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::CanWarpPath");
    const auto owner_ = gameObject.lock();
    const auto collider_ = ResolveAttackCharacter(owner_);
    auto scene_ = PhysXSystem::GetScene();
    if (!owner_ || !owner_->transform || !collider_ || !scene_) return false;
    const auto capsule_ = collider_->GetCapsuleData();
    DirectX::XMFLOAT3 center_{};
    DirectX::XMStoreFloat3(&center_, DirectX::XMVectorScale(DirectX::XMVectorAdd(capsule_.p0, capsule_.p1), 0.5f));
    const float halfHeight_ = 0.5f * DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(capsule_.p0, capsule_.p1)));
    const auto ownerPosition_ = owner_->transform->GetWorldPosition();
    const float deltaX_ = end_.x - start_.x;
    const float deltaZ_ = end_.z - start_.z;
    const float distance_ = std::sqrt(deltaX_ * deltaX_ + deltaZ_ * deltaZ_);
    if (!std::isfinite(distance_)) return false;
    const float skin_ = (std::max)(collider_->GetContactOffset(), 0.02f);
    physx::PxQueryFilterData filter_;
    filter_.flags = physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::ePREFILTER;
    filter_.data.word3 = collider_->GetCollisionOwnerId();
    auto callback_ = PhysXSystem::GetCharacterControllerQueryFilterCallback();
    const physx::PxVec3 queryCenter_(center_.x + start_.x - ownerPosition_.x, center_.y + start_.y - ownerPosition_.y + skin_, center_.z + start_.z - ownerPosition_.z);
    // Gameplay用Volumeと自身は既存Filterで除外し、壁や他Characterによる経路遮断を確認する。
    if (distance_ > 0.0001f)
    {
        physx::PxSweepBuffer hit_;
        const physx::PxCapsuleGeometry geometry_((std::max)(capsule_.radius - skin_, 0.01f), halfHeight_);
        const physx::PxTransform pose_(queryCenter_, physx::PxQuat(physx::PxHalfPi, physx::PxVec3(0.0f, 0.0f, 1.0f)));
        if (scene_->sweep(geometry_, pose_, physx::PxVec3(deltaX_ / distance_, 0.0f, deltaZ_ / distance_), distance_, hit_,
            physx::PxHitFlag::eDEFAULT, filter_, callback_) && hit_.hasBlock) return false;
    }
    const float step_ = collider_->GetStepOffset();
    const float snap_ = collider_->GetGroundSnapDistance();
    const float footY_ = center_.y + start_.y - ownerPosition_.y - halfHeight_ - capsule_.radius;
    const int sampleCount_ = (std::max)(1, static_cast<int>(std::ceil(distance_ / (std::max)(capsule_.radius, 0.2f))));
    // 終点だけでなく途中の足場も調べ、段差許容量を超える崖やステージ外への補正を避ける。
    for (int index_ = 0; index_ <= sampleCount_; ++index_)
    {
        const float rate_ = static_cast<float>(index_) / sampleCount_;
        physx::PxRaycastBuffer ground_;
        const physx::PxVec3 origin_(queryCenter_.x + deltaX_ * rate_, footY_ + step_ + skin_, queryCenter_.z + deltaZ_ * rate_);
        if (!scene_->raycast(origin_, physx::PxVec3(0.0f, -1.0f, 0.0f), step_ + snap_ + skin_, ground_,
            physx::PxHitFlag::eDEFAULT, filter_, callback_) || !ground_.hasBlock || ground_.block.normal.y < 0.5f) return false;
        const auto groundCollider_ = ground_.block.actor ? static_cast<const ColliderComponent*>(ground_.block.actor->userData) : nullptr;
        if (groundCollider_ && !groundCollider_->CanBeCharacterGround()) return false;
    }
    return true;
}

bool HumanBossActionComponent::BuildAttackAssistPlan(const SkillPhase& phase_,
    const std::shared_ptr<GameObject>& target_, DirectX::XMFLOAT3& plannedMotion_,
    float& stopDistance_, float& endNormalizedTime_, float& playbackDuration_, bool checkReachability_, float reachMargin_) const
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::BuildAttackAssistPlan");
    const auto owner_ = gameObject.lock();
    const auto animation_ = owner_ ? AnimationGraphBridgeComponent::Acquire(owner_) : nullptr;
    const auto warp_ = owner_ ? owner_->GetComponent<MotionWarpComponent>() : nullptr;
    MotionWarpPreset naturalPreset_;
    naturalPreset_.targetDistance = 1.5f;
    naturalPreset_.warpPosition = false;
    naturalPreset_.warpRotation = false;
    const auto preset_ = phase_.motionWarpPreset.empty() ? &naturalPreset_ : (warp_ ? warp_->GetPreset(phase_.motionWarpPreset) : nullptr);
    const auto ownCollider_ = ResolveAttackCharacter(owner_);
    const auto targetCollider_ = ResolveAttackCharacter(target_);
    if (!owner_ || !owner_->transform || !target_ || !target_->transform || !animation_ || !preset_ || !ownCollider_ || !targetCollider_ || !ownCollider_->enabled || !targetCollider_->enabled)
    { validationError = "Attack assist target, preset, animation or CharacterCollider missing."; return false; }
    const auto stateIterator_ = animation_->GetStateCatalog().find(phase_.animationState);
    if (stateIterator_ == animation_->GetStateCatalog().end() || !stateIterator_->second) return false;
    const auto& state_ = *stateIterator_->second;
    if (!phase_.motionWarpPreset.empty() && attackAssistEnabled && warp_ && warp_->IsEnabled() &&
        (preset_->warpPosition || preset_->warpRotation) && !state_.rootMotion.enable)
    { validationError = "Attack assist requires the existing RootMotion application path."; return false; }
    const auto model_ = owner_->GetComponent<GltfModelCommponennt>();
    if (!model_ || !model_->IsModelReady() || state_.animationIndex < 0 || state_.animationIndex >= static_cast<int>(model_->GetAnimations().size())) return false;
    const float duration_ = model_->GetAnimations()[state_.animationIndex].secondsLength;
    if (duration_ <= 0.0001f) return false;
    auto& templates_ = AttackTemplateManager::Instance();
    if (templates_.GetTemplates().empty()) templates_.LoadAll();
    float firstHit_ = duration_;
    bool hasAttack_ = false;
    for (const auto& event_ : state_.events)
    {
        if (event_.eventTemplateType != ANIMATION_EVENT_TEMPLATE_TYPE::ATTACK_TEMPLATE) continue;
        const auto template_ = templates_.FindTemplate(event_.attackTemplateId);
        if (!template_ || !template_->weaponHitBox.enabled) continue;
        firstHit_ = (std::min)(firstHit_, event_.startTime + template_->weaponHitBox.activeStartTime);
        hasAttack_ = true;
    }
    if (!hasAttack_) { validationError = "Attack assist requires a timed AttackTemplate event."; return false; }
    // ヒット開始直前で補正を閉じ、速度カーブで再生時間が変わっても追尾窓を延ばさない。
    endNormalizedTime_ = (std::min)(preset_->endNormalizedTime, (std::max)(0.0f, firstHit_ - 0.01f) / duration_);
    const float start_ = (std::max)(preset_->startNormalizedTime, phase_.animationStartTime / duration_);
    DirectX::XMFLOAT3 localMotion_{};
    if (!animation_->EstimateRootMotion(phase_.animationState, start_, endNormalizedTime_, localMotion_, playbackDuration_))
    { validationError = "Attack assist RootMotion window cannot be estimated."; return false; }
    const auto rotation_ = owner_->transform->GetWorldRotation();
    const auto orientation_ = DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(
        DirectX::XMConvertToRadians(rotation_.y), DirectX::XMConvertToRadians(rotation_.x), DirectX::XMConvertToRadians(rotation_.z));
    const auto worldMotion_ = DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3(localMotion_), orientation_);
    plannedMotion_ = { worldMotion_.x, 0.0f, worldMotion_.z };
    stopDistance_ = (std::max)(preset_->targetDistance, ownCollider_->GetCapsuleData().radius + targetCollider_->GetCapsuleData().radius + 0.15f) + (std::max)(0.0f, reachMargin_);
    const auto ownerPosition_ = owner_->transform->GetWorldPosition();
    const auto targetPosition_ = target_->transform->GetWorldPosition();
    if (std::abs(targetPosition_.y - ownerPosition_.y) > ownCollider_->GetStepOffset() + ownCollider_->GetGroundSnapDistance())
    { validationError = "Attack assist target height rejected."; return false; }
    DirectX::XMFLOAT3 direction_{ targetPosition_.x - ownerPosition_.x, 0.0f, targetPosition_.z - ownerPosition_.z };
    const float distance_ = std::sqrt(direction_.x * direction_.x + direction_.z * direction_.z);
    if ((checkReachability_ && distance_ > preset_->maximumWarpTargetDistance) || distance_ < 0.0001f)
    { validationError = "Attack assist target distance rejected."; return false; }
    direction_.x /= distance_; direction_.z /= distance_;
    const float needed_ = (std::max)(0.0f, distance_ - stopDistance_);
    const float approach_ = plannedMotion_.x * direction_.x + plannedMotion_.z * direction_.z;
    const float correctionX_ = direction_.x * needed_ - plannedMotion_.x;
    const float correctionZ_ = direction_.z * needed_ - plannedMotion_.z;
    if (!checkReachability_) return true;
    const float required_ = distance_ > stopDistance_ + approach_ ?  std::sqrt(correctionX_ * correctionX_ + correctionZ_ * correctionZ_) : 0.0f;
    const bool positionEnabled_ = attackAssistEnabled && warp_ && warp_->IsEnabled() && preset_->warpPosition && preset_->positionMode == MOTION_WARP_POSITION_MODE::TARGET_END_POSITION;
    const float frameCount_ = std::ceil(playbackDuration_ / (std::max)(assistFrameDeltaTime, 0.001f));
    const float capacity_ = positionEnabled_ ? (std::min)(preset_->maximumTotalPositionCorrection * preset_->positionStrength,
        (std::min)(preset_->maximumPositionCorrectionPerSecond * playbackDuration_, preset_->maximumPositionCorrectionPerFrame * frameCount_)) : 0.0f;
    debugPlannedDistance = std::sqrt(plannedMotion_.x * plannedMotion_.x + plannedMotion_.z * plannedMotion_.z);
    debugRequiredCorrection = required_;
    if (required_ > capacity_ + 0.001f) { validationError = "Required Warp exceeds correction or time budget."; return false; }
    if (positionEnabled_ && required_ > 0.001f)
    {
        // カーブ終端と実際の更新間隔まで見込み、上限内でも期間中に完了できない補正を選ばない。
        const auto curve_ = owner_->GetComponent<CurveComponent>();
        const float frameTime_ = (std::max)(assistFrameDeltaTime, 0.001f);
        float normalized_ = start_;
        float delivered_ = 0.0f;
        float used_ = 0.0f;
        constexpr int MAX_ASSIST_STEPS = 2048;
        for (int index_ = 0; index_ < MAX_ASSIST_STEPS && normalized_ < endNormalizedTime_; ++index_)
        {
            float speed_ = state_.playbackSpeed;
            if (curve_ && !state_.speedCurveName.empty() && curve_->HasTrack(state_.speedCurveName))
                speed_ = curve_->GetValue(state_.speedCurveName, normalized_);
            for (const auto& event_ : state_.events)
                if (event_.overridePlaybackSpeed && normalized_ * duration_ >= event_.startTime && normalized_ * duration_ <= event_.endTime)
                    speed_ = (std::max)(event_.playbackSpeed, 0.01f);
            if (!std::isfinite(speed_) || speed_ <= 0.0001f) break;
            normalized_ = (std::min)(endNormalizedTime_, normalized_ + frameTime_ * speed_ / duration_);
            float progress_ = std::clamp((normalized_ - start_) / (endNormalizedTime_ - start_), 0.0f, 1.0f);
            if (curve_ && !preset_->positionCorrectionCurveName.empty() && curve_->HasTrack(preset_->positionCorrectionCurveName))
                progress_ = std::clamp(curve_->GetValue(preset_->positionCorrectionCurveName, progress_), 0.0f, 1.0f);
            const float frameLimit_ = (std::min)((std::max)(0.0f, preset_->maximumTotalPositionCorrection - used_),
                (std::min)(preset_->maximumPositionCorrectionPerFrame, preset_->maximumPositionCorrectionPerSecond * frameTime_));
            const float correction_ = std::clamp(required_ * progress_ - delivered_, -frameLimit_, frameLimit_);
            delivered_ += correction_;
            used_ += std::abs(correction_);
        }
        if (delivered_ + 0.01f < required_)
        { validationError = "Warp curve cannot finish before the attack window."; return false; }
    }

    const auto forward_ = owner_->transform->GetFront();
    const float forwardLength_ = std::sqrt(forward_.x * forward_.x + forward_.z * forward_.z);
    const float angle_ = forwardLength_ > 0.0001f ? DirectX::XMConvertToDegrees(std::acos(std::clamp(
        (forward_.x * direction_.x + forward_.z * direction_.z) / forwardLength_, -1.0f, 1.0f))) : 180.0f;
    DirectX::XMFLOAT3 facingMotion_{};
    float facingDuration_ = 0.0f;
    const bool rotationEnabled_ = attackAssistEnabled && warp_ && warp_->IsEnabled() && preset_->warpRotation && preset_->rotationMode == MOTION_WARP_ROTATION_MODE::FACE_TARGET;
    if (rotationEnabled_)
        animation_->EstimateRootMotion(phase_.animationState, (std::max)(start_, preset_->rotationStartNormalizedTime),
            (std::min)(endNormalizedTime_, preset_->rotationEndNormalizedTime), facingMotion_, facingDuration_);
    // 終盤の減衰分を見込み、最大角度だけでなく回転を終えられる時間も開始条件にする。
    const float angleCapacity_ = rotationEnabled_ ? (std::min)(preset_->maximumRotationDegrees,
        preset_->maximumRotationDegreesPerSecond * preset_->rotationStrength * facingDuration_ * 0.5f) : 10.0f;
    if (angle_ > angleCapacity_)
    { validationError = "Attack assist facing angle or rotation time rejected."; return false; }
    const DirectX::XMFLOAT3 destination_{ ownerPosition_.x + direction_.x * needed_, ownerPosition_.y, ownerPosition_.z + direction_.z * needed_ };
    if (!CanWarpPath(ownerPosition_, destination_)) { validationError = "Attack assist path blocked or unsupported."; return false; }
    return true;
}

float HumanBossActionComponent::GetAttackStartReach(const SkillDefinition& skill_, const std::shared_ptr<GameObject>& target_) const
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::GetAttackStartReach");
    const auto owner_ = gameObject.lock();
    const auto animation_ = owner_ ? AnimationGraphBridgeComponent::Acquire(owner_) : nullptr;
    const auto warp_ = owner_ ? owner_->GetComponent<MotionWarpComponent>() : nullptr;
    if (!animation_ || !owner_->transform || !target_ || !target_->transform) return 0.0f;
    for (const auto& phase_ : skill_.phases)
    {
        const auto state_ = animation_->GetStateCatalog().find(phase_.animationState);
        if (state_ == animation_->GetStateCatalog().end() || !state_->second ||
            std::none_of(state_->second->events.begin(), state_->second->events.end(),
                [](const AnimEvent& event_) { return event_.eventTemplateType == ANIMATION_EVENT_TEMPLATE_TYPE::ATTACK_TEMPLATE; })) continue;
        DirectX::XMFLOAT3 motion_{};
        float stop_ = 0.0f;
        float end_ = 0.0f;
        float duration_ = 0.0f;
        if (!BuildAttackAssistPlan(phase_, target_, motion_, stop_, end_, duration_, false)) return 0.0f;
        const auto ownPosition_ = owner_->transform->GetWorldPosition();
        const auto targetPosition_ = target_->transform->GetWorldPosition();
        const float deltaX_ = targetPosition_.x - ownPosition_.x;
        const float deltaZ_ = targetPosition_.z - ownPosition_.z;
        const float distance_ = std::sqrt(deltaX_ * deltaX_ + deltaZ_ * deltaZ_);
        if (distance_ <= 0.0001f) return 0.0f;
        const float approach_ = (motion_.x * deltaX_ + motion_.z * deltaZ_) / distance_;
        float reach_ = (std::max)(0.0f, stop_ + approach_);
        const auto preset_ = warp_ ? warp_->GetPreset(phase_.motionWarpPreset) : nullptr;
        if (attackAssistEnabled && warp_ && warp_->IsEnabled() && preset_ && preset_->warpPosition &&
            preset_->positionMode == MOTION_WARP_POSITION_MODE::TARGET_END_POSITION)
        {
            float curveEnd_ = 1.0f;
            const auto curve_ = owner_->GetComponent<CurveComponent>();
            if (curve_ && !preset_->positionCorrectionCurveName.empty() && curve_->HasTrack(preset_->positionCorrectionCurveName))
                curveEnd_ = std::clamp(curve_->GetValue(preset_->positionCorrectionCurveName, 1.0f), 0.0f, 1.0f);
            const float frames_ = std::ceil(duration_ / (std::max)(assistFrameDeltaTime, 0.001f));
            reach_ += (std::min)(preset_->maximumTotalPositionCorrection * preset_->positionStrength * curveEnd_,
                (std::min)(preset_->maximumPositionCorrectionPerSecond * duration_, preset_->maximumPositionCorrectionPerFrame * frames_));
            reach_ = (std::min)(reach_, preset_->maximumWarpTargetDistance);
        }
        return reach_;
    }
    return 0.0f;
}

bool HumanBossActionComponent::CanContinueAttack(const SkillPhase& phase_)
{
    // 再出現後の派生攻撃へ、影渡りの無敵を持ち越さない。
    if (shadowShiftActive && execution.GetPhaseIndex() == 0) SetShadowInvulnerable(false);
    // 移動時間が終了しても実位置が未到着なら、派生のWarpや攻撃を開始しない。
    if (shadowShiftActive && execution.GetPhaseIndex() == 0 && !shadowArrived)
    { status = "Shadow follow-up rejected: destination not reached."; return false; }
    if (shadowChain && phase_.warpHeight > 0.0f && !CanRaiseShadowDive(phase_.warpHeight)) return false;
    if (shadowChain && !phase_.motionWarpPreset.empty())
    {
        DirectX::XMFLOAT3 motion_{};
        float stop_ = 0.0f, end_ = 0.0f, duration_ = 0.0f;
        return BuildAttackAssistPlan(phase_, attackTarget.lock(), motion_, stop_, end_, duration_);
    }
    BS_PROFILE_SCOPE("HumanBossActionComponent::CanContinueAttack");
    // 予兆中にガード解除・回避・離脱した相手を、拘束位置へ引き戻さない。
    if (execution.GetSkillName() == "GUARD_EXECUTION" && phase_.enterAction == "HumanBoss_ExecutionCapture")
        return CanCaptureGuardingPlayer(attackTarget.lock());
    if (execution.GetSkillName() == "COUNTER_ATTACK" && phase_.enterAction == "HumanBoss_ExecutionCapture")
    {
        // 確定反撃は次の開始処理で拘束位置へ揃えるため、通常攻撃の到達判定では打ち切らない。
        const auto target_ = attackTarget.lock();
        const auto recoilingTarget_ = executionCapturedPlayer.lock();
        const auto health_ = target_ ? target_->GetComponent<HealthComponent>() : nullptr;
        if (!target_ || target_ != recoilingTarget_ || target_->IsPendingDestroy() || (health_ && health_->IsDead()))
        {
            status = "Execution follow-up stopped: recoil target missing or dead.";
            return false;
        }
        return true;
    }
    if (!attackAssistSkill) return true;
    const auto owner_ = GetGameObject();
    const auto animation_ = owner_ ? AnimationGraphBridgeComponent::Acquire(owner_) : nullptr;
    if (!animation_) { status = "Follow-up stopped: animation missing."; return false; }
    const auto state_ = animation_->GetStateCatalog().find(phase_.animationState);
    if (state_ == animation_->GetStateCatalog().end() || !state_->second)
    { status = "Follow-up stopped: state missing: " + phase_.animationState; return false; }
    // 構えや待機Phaseは維持し、次の打撃へ移る時だけ到達判定を行う。
    if (std::none_of(state_->second->events.begin(), state_->second->events.end(),
        [](const AnimEvent& event_) { return event_.eventTemplateType == ANIMATION_EVENT_TEMPLATE_TYPE::ATTACK_TEMPLATE; })) return true;
    const auto target_ = attackTarget.lock();
    const auto health_ = target_ ? target_->GetComponent<HealthComponent>() : nullptr;
    if (!target_ || !target_->transform || (health_ && health_->IsDead()))
    { status = "Follow-up stopped: target missing or dead."; return false; }
    DirectX::XMFLOAT3 motion_{};
    float stopDistance_ = 0.0f;
    float end_ = 0.0f;
    float duration_ = 0.0f;
    validationError.clear();
    // 継続判定だけ小さな余裕を持たせ、近距離でわずかに後退する段を過度に切らない。
    constexpr float FOLLOW_UP_REACH_MARGIN = 0.25f;
    if (!BuildAttackAssistPlan(phase_, target_, motion_, stopDistance_, end_, duration_, true, FOLLOW_UP_REACH_MARGIN))
    { status = "Follow-up stopped before " + phase_.animationState + ": " + validationError; return false; }
    const auto warp_ = owner_->GetComponent<MotionWarpComponent>();
    const auto preset_ = warp_ ? warp_->GetPreset(phase_.motionWarpPreset) : nullptr;
    if ((hasAttackTargetPosition || windupFacingTracked) && attackAssistEnabled && warp_ && warp_->IsEnabled() && preset_ &&
        ((preset_->warpPosition && preset_->positionMode != MOTION_WARP_POSITION_MODE::NONE) ||
            (preset_->warpRotation && preset_->rotationMode == MOTION_WARP_ROTATION_MODE::FACE_TARGET)))
    {
        // 大きく横へ回避した相手を追わず、各段の開始時だけ近距離の後退に対応する。
        const auto position_ = owner_->transform->GetWorldPosition();
        const auto actual_ = target_->transform->GetWorldPosition();
        const float fixedX_ = attackTargetPosition.x - position_.x;
        const float fixedZ_ = attackTargetPosition.z - position_.z;
        const float actualX_ = actual_.x - position_.x;
        const float actualZ_ = actual_.z - position_.z;
        const float fixedLength_ = std::sqrt(fixedX_ * fixedX_ + fixedZ_ * fixedZ_);
        const float actualLength_ = std::sqrt(actualX_ * actualX_ + actualZ_ * actualZ_);
        if (fixedLength_ > 0.0001f && actualLength_ > 0.0001f)
        {
            const float angle_ = DirectX::XMConvertToDegrees(std::acos(std::clamp(
                (fixedX_ * actualX_ + fixedZ_ * actualZ_) / (fixedLength_ * actualLength_), -1.0f, 1.0f)));
            constexpr float MINIMUM_AIM_TOLERANCE_DEGREES = 10.0f;
            constexpr float MAXIMUM_AIM_TOLERANCE_DEGREES = 30.0f;
            const float tolerance_ = std::clamp(preset_->maximumRotationDegrees, MINIMUM_AIM_TOLERANCE_DEGREES, MAXIMUM_AIM_TOLERANCE_DEGREES);
            if (angle_ > tolerance_)
            { status = "Follow-up stopped: target left the locked attack direction."; return false; }
            // 各段の開始時だけ近距離の後退へ狙いを更新し、振り始めた後の追尾は行わない。
            const float travelX_ = actual_.x - attackTargetPosition.x;
            const float travelZ_ = actual_.z - attackTargetPosition.z;
            constexpr float CLOSE_FOLLOW_UP_DISTANCE = 3.25f;
            constexpr float MAXIMUM_FOLLOW_UP_RETARGET_DISTANCE = 3.0f;
            if (actualLength_ <= CLOSE_FOLLOW_UP_DISTANCE &&
                travelX_ * travelX_ + travelZ_ * travelZ_ <= MAXIMUM_FOLLOW_UP_RETARGET_DISTANCE * MAXIMUM_FOLLOW_UP_RETARGET_DISTANCE)
            {
                attackTargetPosition = actual_;
                hasAttackTargetPosition = true;
                return true; // 距離・角度・補正量・経路は直前のBuildAttackAssistPlanで検証済み。
            }
            DirectX::XMFLOAT3 destination_{ position_.x + motion_.x, position_.y, position_.z + motion_.z };
            if (preset_->warpPosition && preset_->positionMode == MOTION_WARP_POSITION_MODE::TARGET_END_POSITION)
            {
                // 次段は固定位置へ向かうため、実Targetへ再追尾できると仮定して射程を広げない。
                const float directionX_ = fixedX_ / fixedLength_;
                const float directionZ_ = fixedZ_ / fixedLength_;
                const float naturalApproach_ = motion_.x * directionX_ + motion_.z * directionZ_;
                const float frames_ = std::ceil(duration_ / (std::max)(assistFrameDeltaTime, 0.001f));
                const float correction_ = (std::min)(preset_->maximumTotalPositionCorrection * preset_->positionStrength,
                    (std::min)(preset_->maximumPositionCorrectionPerSecond * duration_, preset_->maximumPositionCorrectionPerFrame * frames_));
                const float approach_ = (std::min)((std::max)(0.0f, fixedLength_ - stopDistance_),
                    (std::max)(0.0f, naturalApproach_ + correction_));
                destination_ = { position_.x + directionX_ * approach_, position_.y, position_.z + directionZ_ * approach_ };
            }
            const float remainingX_ = actual_.x - destination_.x;
            const float remainingZ_ = actual_.z - destination_.z;
            if (std::sqrt(remainingX_ * remainingX_ + remainingZ_ * remainingZ_) > stopDistance_)
            { status = "Follow-up stopped: locked movement cannot reach the current target."; return false; }
            if (!CanWarpPath(position_, destination_))
            { status = "Follow-up stopped: locked movement path blocked."; return false; }
        }
    }
    return true;
}

void HumanBossActionComponent::BeginAttackAssist(const SkillPhase& phase_)
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::BeginAttackAssist");
    const auto owner_ = GetGameObject();
    const auto warp_ = owner_ ? owner_->GetComponent<MotionWarpComponent>() : nullptr;
    if (!warp_) return;
    warp_->Stop();
    if (shadowChain && !phase_.motionWarpPreset.empty())
    {
        // 再出現位置を新しい攻撃の始点にし、移動前の追尾予算を持ち越さない。
        attackAssistSkill = true;
        attackActive = true;
        hasAttackTargetPosition = false;
        windupFacingTracked = false;
        windupTargetLocked = false;
        windupRotationCorrection = 0.0f;
        attackStartPosition = owner_->transform->GetWorldPosition();
        if (const auto target_ = attackTarget.lock())
            attackInitialTargetPosition = attackTargetPosition = target_->transform->GetWorldPosition();
    }
    if (!attackAssistEnabled || !attackAssistSkill || phase_.motionWarpPreset.empty()) return;
    const auto target_ = attackTarget.lock();
    DirectX::XMFLOAT3 plannedMotion_{};
    float stopDistance_ = 0.0f;
    float end_ = 0.0f;
    float duration_ = 0.0f;
    if (!BuildAttackAssistPlan(phase_, target_, plannedMotion_, stopDistance_, end_, duration_)) return;
    const auto animation_ = AnimationGraphBridgeComponent::Acquire(owner_);
    const auto model_ = owner_->GetComponent<GltfModelCommponennt>();
    const auto& state_ = *animation_->GetStateCatalog().at(phase_.animationState);
    const float clipDuration_ = model_->GetAnimations()[state_.animationIndex].secondsLength;
    const float start_ = phase_.animationStartTime / clipDuration_;
    const std::weak_ptr<HumanBossActionComponent> weak_ = owner_->GetComponent<HumanBossActionComponent>();
    if (warp_->PlayForTarget(phase_.motionWarpPreset, target_, plannedMotion_, stopDistance_, start_, end_,
        hasAttackTargetPosition || windupFacingTracked ? &attackTargetPosition : nullptr,
        [weak_](const DirectX::XMFLOAT3& start_, const DirectX::XMFLOAT3& end_)
        {
            const auto self_ = weak_.lock();
            return self_ && self_->CanWarpPath(start_, end_);
        }, !hasAttackTargetPosition && windupFacingTracked ? windupRotationCorrection : 0.0f))
    {
        attackTargetPosition = warp_->GetTargetPosition();
        hasAttackTargetPosition = true;
    }
}

HumanBossActionComponent::~HumanBossActionComponent()
{
    // Componentだけを削除した場合も、残った部位の一時無敵と非表示を解除する。
    SetShadowInvulnerable(false);
    SetShadowVisible(true);
}

const std::string& HumanBossActionComponent::GetName() const
{
    static const std::string NAME = "HumanBossActionComponent";
    return NAME;
}

void HumanBossActionComponent::Start()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::Start");
    InitializeAttackAssist();
    RegisterActions();
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->manager) return;
    // 成功イベントの攻撃元を確認し、このボスへの反撃時だけ無敵解除Actionを実行する。
    const std::weak_ptr<HumanBossActionComponent> weak_ = owner_->GetComponent<HumanBossActionComponent>();
    owner_->GetManager().GetJustGuardSystem().AddJustGuardCallback([weak_](const JustGuardEvent& event_)
    {
        const auto self_ = weak_.lock();
        if (!self_) return;
        self_->NotifyCounterOpportunity(event_.attacker.lock());
        const auto boss_ = self_->GetGameObject();
        const auto player_ = event_.defender.lock();
        auto hitObject_ = event_.hitBox ? event_.hitBox->GetGameObject() : nullptr;
        if (!boss_ || !player_ || !player_->GetComponent<PlayerControllerComponent>()) return;
        while (hitObject_ && !hitObject_->GetComponent<ProjectileMotionComponent>())
        {
            const auto parent_ = hitObject_->transform ? hitObject_->transform->parent.lock() : nullptr;
            hitObject_ = parent_ ? parent_->gameObject.lock() : nullptr;
        }
        if (!hitObject_ || std::none_of(self_->shadowOrbs.begin(), self_->shadowOrbs.end(),
            [&hitObject_](const std::weak_ptr<GameObject>& orb_) { return orb_.lock() == hitObject_; })) return;
        const auto hit_ = hitObject_->GetComponent<AttackEffectHitComponent>();
        const auto motion_ = hitObject_->GetComponent<ProjectileMotionComponent>();
        if (!hit_ || !motion_ || !hit_->IsOwnedBy(boss_.get())) return;
        hit_->SetRuntimeHitOwner(player_.get());
        hit_->SetRuntimeHitTeamId(0);
        hit_->SetRuntimeSourceAction("HumanBossShadowOrbReflected");
        hit_->ClearHitHistory();
        motion_->Reflect(boss_, player_, self_->orbReflectSpeedMultiplier);
        if (const auto mesh_ = hitObject_->GetComponent<MeshVFXComponent>())
            mesh_->SetRuntimeAppearance({ 1.0f, 1.0f, 1.0f }, { 0.65f, 0.9f, 1.0f, 1.0f },
                { 1.0f, 0.82f, 0.3f, 1.0f });
        if (const auto ai_ = boss_->GetComponent<HumanBossAIComponent>()) ai_->NotifyReflectedOrb();
    });
    owner_->GetManager().GetJustDodgeSystem().AddJustDodgeCallback([weak_](const JustDodgeEvent& event_)
    {
        if (const auto self_ = weak_.lock()) self_->NotifyCounterOpportunity(event_.attacker.lock());
    });
}

void HumanBossActionComponent::RegisterActions()
{
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    // Registryより先にComponentが破棄されても、通知から解放済みのthisを参照しない。
    const std::weak_ptr<HumanBossActionComponent> weak_ = owner_->GetComponent<HumanBossActionComponent>();
    RegisterRangedActions();
    owner_->RegisterAction("HumanBoss", "HumanBoss_ReleaseCounterInvulnerability", [weak_]()
    { if (const auto self_ = weak_.lock()) self_->ReleaseCounterInvulnerability(); });
    owner_->RegisterAction("HumanBoss", "ActionEnd", [weak_]()
    {
        if (const auto self_ = weak_.lock()) self_->execution.NotifyPhaseEnd();
    });
    owner_->RegisterAction("HumanBoss", "OnGuardSucceeded", [weak_]()
    {
        if (const auto self_ = weak_.lock())
            if (self_->IsActive() && (self_->currentDefense == HumanBossDefense::GUARD || self_->currentDefense == HumanBossDefense::JUST_GUARD)) self_->guardSucceeded = true;
    });
    owner_->RegisterAction("HumanBoss", "OnJustGuard", [weak_]()
    {
        if (const auto self_ = weak_.lock())
        {
            if (self_->IsActive() && self_->currentDefense == HumanBossDefense::JUST_GUARD) self_->justGuardSucceeded = true;
        }
    });
    owner_->RegisterAction("HumanBoss", "HumanBoss_ExecutionCapture", [weak_]()
    { if (const auto self_ = weak_.lock()) self_->BeginExecutionCapture(); });
    owner_->RegisterAction("HumanBoss", "HumanBoss_ExecutionRecoil", [weak_]()
    { if (const auto self_ = weak_.lock()) self_->BeginExecutionRecoil(); });
    owner_->RegisterAction("HumanBoss", "HumanBoss_ExecutionRelease", [weak_]()
    { if (const auto self_ = weak_.lock()) self_->ReleaseExecutionCapture(); });
    owner_->RegisterAction("HumanBoss", "HumanBoss_ExecutionCamera", [weak_]()
    { if (const auto self_ = weak_.lock()) self_->PlayExecutionCamera(); });
    owner_->RegisterAction("HumanBoss", "HumanBoss_ExecutionGrabFeedback", [weak_]()
    { if (const auto self_ = weak_.lock()) self_->PlayExecutionGrabFeedback(); });
    owner_->RegisterAction("HumanBoss", "HumanBoss_ExecutionSlamFeedback", [weak_]()
    { if (const auto self_ = weak_.lock()) self_->PlayExecutionSlamFeedback(); });
    owner_->RegisterAction("HumanBoss", "HumanBoss_WindupBegin", [weak_]()
    {
        if (const auto self_ = weak_.lock())
        {
            self_->windupTracking = true;
            if (const auto actor_ = self_->GetGameObject())
                if (const auto blackboard_ = actor_->GetComponent<BlackboardComponent>()) blackboard_->SetBool("HumanBossAI", "TelegraphActive", true);
        }
    });
    owner_->RegisterAction("HumanBoss", "HumanBoss_WindupEnd", [weak_]()
    {
        if (const auto self_ = weak_.lock())
        {
            self_->windupTracking = false;
            if (const auto actor_ = self_->GetGameObject())
                if (const auto blackboard_ = actor_->GetComponent<BlackboardComponent>()) blackboard_->SetBool("HumanBossAI", "TelegraphActive", false);
        }
    });
    // 各攻撃のAnimation Eventから操作し、開始時に前段の軌跡を切り離す。
    owner_->RegisterAction("HumanBoss", "HumanBoss_TrailBegin", [weak_]()
    {
        if (const auto self_ = weak_.lock()) self_->SetWeaponTrails(true, true);
    });
    owner_->RegisterAction("HumanBoss", "HumanBoss_TrailEnd", [weak_]()
    {
        if (const auto self_ = weak_.lock()) self_->SetWeaponTrails(false, false);
    });
    owner_->RegisterAction("HumanBoss", "HumanBoss_ShadowShowArea", [weak_]()
    { if (const auto self_ = weak_.lock()) self_->ShowShadowArea(); });
    owner_->RegisterAction("HumanBoss", "HumanBoss_ShadowStrike", [weak_]()
    { if (const auto self_ = weak_.lock()) self_->StrikeShadowArea(); });
    owner_->RegisterAction("HumanBoss", "HumanBoss_ShadowSlashStep", [weak_]()
    {
        const auto self_ = weak_.lock();
        const auto actor_ = self_ ? self_->GetGameObject() : nullptr;
        const auto phase_ = self_ ? self_->execution.GetCurrentPhase() : nullptr;
        const auto target_ = self_ ? self_->attackTarget.lock() : nullptr;
        if (!actor_ || !actor_->transform || !target_ || !target_->transform || !phase_ ||
            self_->rangedSkill != "SHADOW_SLASH" || phase_->animationState != "SS_Sp_Skill1") return;
        DirectX::XMFLOAT3 ground_{};
        DirectX::XMFLOAT3 currentGround_{};
        const auto start_ = actor_->transform->GetWorldPosition();
        if (!self_->ResolveShadowDiveGround(ground_) || !self_->ResolveShadowGround(start_, currentGround_))
        { self_->Stop(); return; }
        const DirectX::XMFLOAT3 destination_{ ground_.x, start_.y + ground_.y - currentGround_.y, ground_.z };
        // 瞬間移動では途中の経路を通らない。出現地点だけをカプセルで検証する。
        if (!self_->CanRaiseShadowDive(0.1f)) { self_->Stop(); return; }
        self_->SpawnShadowEffect();
        if (const auto warp_ = actor_->GetComponent<MotionWarpComponent>()) warp_->Stop();
        if (const auto movement_ = actor_->GetComponent<CharacterMovementComponent>()) movement_->StopHorizontalMovementImmediately();
        actor_->transform->ApplyWorldMatrix(actor_->transform->GetWorldMatrix() * DirectX::XMMatrixTranslation(
            destination_.x - start_.x, destination_.y - start_.y, destination_.z - start_.z));
        actor_->UpdateTransform();
        if (const auto movement_ = actor_->GetComponent<CharacterMovementComponent>())
        {
            const auto targetPosition_ = target_->transform->GetWorldPosition();
            movement_->SetFacingDirection({ targetPosition_.x - destination_.x, 0.0f,
                targetPosition_.z - destination_.z }, true);
        }
        self_->SpawnShadowEffect();
    });
    owner_->RegisterAction("HumanBoss", "HumanBoss_ShadowClear", [weak_]()
    { if (const auto self_ = weak_.lock()) self_->ClearShadowArea(); });
    owner_->RegisterAction("HumanBoss", "HumanBoss_ShadowDiveStart", [weak_]()
    {
        if (const auto self_ = weak_.lock())
        {
            const auto actor_ = self_->GetGameObject();
            const auto phase_ = self_->execution.GetCurrentPhase();
            if (!actor_ || !actor_->transform || !phase_) return;
            if (!self_->ResolveShadowDiveGround(self_->shadowGroundPosition)) { self_->Stop(); return; }
            // プレイヤーの正面上へ出現させ、移動距離によらず着地攻撃の位置を揃える。
            const auto position_ = actor_->transform->GetWorldPosition();
            const DirectX::XMFLOAT3 destination_{ self_->shadowGroundPosition.x,
                self_->shadowGroundPosition.y + phase_->warpHeight, self_->shadowGroundPosition.z };
            actor_->transform->ApplyWorldMatrix(actor_->transform->GetWorldMatrix() * DirectX::XMMatrixTranslation(
                destination_.x - position_.x, destination_.y - position_.y, destination_.z - position_.z));
            actor_->UpdateTransform();
            if (const auto movement_ = actor_->GetComponent<CharacterMovementComponent>())
            {
                movement_->StopHorizontalMovementImmediately();
                if (const auto target_ = self_->attackTarget.lock())
                    if (target_->transform)
                    {
                        const auto targetPosition_ = target_->transform->GetWorldPosition();
                        movement_->SetFacingDirection({ targetPosition_.x - destination_.x, 0.0f,
                            targetPosition_.z - destination_.z }, true);
                    }
            }
            self_->shadowLandingLocked = false;
            if (const auto target_ = self_->attackTarget.lock())
                if (target_->transform) self_->shadowPreviousTargetPosition = target_->transform->GetWorldPosition();
            self_->SpawnShadowEffect();
        }
    });
    owner_->RegisterAction("HumanBoss", "HumanBossStop", [weak_]()
    {
        if (const auto self_ = weak_.lock()) self_->Stop();
    });
}

bool HumanBossActionComponent::ResolveShadowGround(const DirectX::XMFLOAT3& position_, DirectX::XMFLOAT3& ground_) const
{
    const auto owner_ = gameObject.lock();
    const auto collider_ = ResolveAttackCharacter(owner_);
    auto scene_ = PhysXSystem::GetScene();
    if (!scene_ || !collider_) return false;
    physx::PxQueryFilterData filter_;
    filter_.flags = physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::ePREFILTER;
    filter_.data.word3 = collider_->GetCollisionOwnerId();
    physx::PxRaycastBuffer hit_;
    if (!scene_->raycast(physx::PxVec3(position_.x, position_.y + 1.0f, position_.z), physx::PxVec3(0, -1, 0),
        12.0f, hit_, physx::PxHitFlag::eDEFAULT, filter_, PhysXSystem::GetCharacterControllerQueryFilterCallback()) ||
        !hit_.hasBlock || hit_.block.normal.y < 0.5f) return false;
    const auto groundCollider_ = hit_.block.actor ? static_cast<const ColliderComponent*>(hit_.block.actor->userData) : nullptr;
    if (groundCollider_ && !groundCollider_->CanBeCharacterGround()) return false;
    ground_ = { hit_.block.position.x, hit_.block.position.y, hit_.block.position.z };
    return true;
}

bool HumanBossActionComponent::ResolveShadowDiveGround(DirectX::XMFLOAT3& ground_) const
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::ResolveShadowDiveGround");
    const auto owner_ = gameObject.lock();
    const auto target_ = attackTarget.lock();
    const auto ownCollider_ = ResolveAttackCharacter(owner_);
    const auto targetCollider_ = ResolveAttackCharacter(target_);
    if (!owner_ || !owner_->transform || !target_ || !target_->transform || !ownCollider_ || !targetCollider_) return false;
    const auto forward_ = target_->transform->GetFront();
    const float length_ = std::sqrt(forward_.x * forward_.x + forward_.z * forward_.z);
    if (!std::isfinite(length_) || length_ < 0.0001f) return false;
    const float distance_ = ownCollider_->GetCapsuleData().radius + targetCollider_->GetCapsuleData().radius + 0.5f;
    auto position_ = target_->transform->GetWorldPosition();
    // 着地時の体同士の重なりを避ける間合いを、プレイヤーの前方向へ確保する。
    position_.x += forward_.x / length_ * distance_;
    position_.z += forward_.z / length_ * distance_;
    return ResolveShadowGround(position_, ground_);
}

bool HumanBossActionComponent::CanRaiseShadowDive(float height_) const
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::CanRaiseShadowDive");
    const auto owner_ = gameObject.lock();
    const auto collider_ = ResolveAttackCharacter(owner_);
    auto scene_ = PhysXSystem::GetScene();
    DirectX::XMFLOAT3 ground_{};
    if (!owner_ || !owner_->transform || !collider_ || !scene_ || height_ <= 0.0f || height_ > 10.0f ||
        !ResolveShadowDiveGround(ground_)) return false;
    const auto capsule_ = collider_->GetCapsuleData();
    DirectX::XMFLOAT3 center_{};
    DirectX::XMStoreFloat3(&center_, DirectX::XMVectorScale(DirectX::XMVectorAdd(capsule_.p0, capsule_.p1), 0.5f));
    const float halfHeight_ = 0.5f * DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(capsule_.p0, capsule_.p1)));
    physx::PxQueryFilterData filter_;
    filter_.flags = physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::ePREFILTER;
    filter_.data.word3 = collider_->GetCollisionOwnerId();
    physx::PxSweepBuffer hit_;
    const physx::PxCapsuleGeometry geometry_((std::max)(0.01f, capsule_.radius - collider_->GetContactOffset()), halfHeight_);
    const auto position_ = owner_->transform->GetWorldPosition();
    const physx::PxTransform pose_(physx::PxVec3(center_.x + ground_.x - position_.x,
        center_.y + ground_.y - position_.y + 0.02f, center_.z + ground_.z - position_.z),
        physx::PxQuat(physx::PxHalfPi, physx::PxVec3(0, 0, 1)));
    // ワープ先の着地点から上空までを検査し、壁や天井に埋まる出現を防ぐ。
    return !scene_->sweep(geometry_, pose_, physx::PxVec3(0, 1, 0), height_, hit_, physx::PxHitFlag::eDEFAULT,
        filter_, PhysXSystem::GetCharacterControllerQueryFilterCallback()) || !hit_.hasBlock;
}

void HumanBossActionComponent::UpdateShadowDive(float deltaTime_)
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::UpdateShadowDive");
    const auto phase_ = execution.GetCurrentPhase();
    if (!phase_ || phase_->warpHeight <= 0.0f || shadowLandingLocked || execution.GetPhaseTime() > shadowLandingTrackTime) return;
    const auto owner_ = GetGameObject();
    const auto target_ = attackTarget.lock();
    const auto movement_ = owner_ ? owner_->GetComponent<CharacterMovementComponent>() : nullptr;
    if (!movement_ || !owner_->transform || !target_ || !target_->transform || deltaTime_ <= 0.0f) return;
    const auto position_ = owner_->transform->GetWorldPosition();
    auto targetPosition_ = target_->transform->GetWorldPosition();
    const auto currentTarget_ = targetPosition_;
    // 移動方向を短時間だけ予測し、追跡時間と予測距離の両方に上限を設ける。
    targetPosition_.x += std::clamp((currentTarget_.x - shadowPreviousTargetPosition.x) / deltaTime_ * 0.12f, -0.4f, 0.4f);
    targetPosition_.z += std::clamp((currentTarget_.z - shadowPreviousTargetPosition.z) / deltaTime_ * 0.12f, -0.4f, 0.4f);
    shadowPreviousTargetPosition = currentTarget_;
    const auto ownCollider_ = ResolveAttackCharacter(owner_);
    const auto targetCollider_ = ResolveAttackCharacter(target_);
    if (!ownCollider_ || !targetCollider_) return;
    const auto forward_ = target_->transform->GetFront();
    const float forwardLength_ = std::sqrt(forward_.x * forward_.x + forward_.z * forward_.z);
    if (!std::isfinite(forwardLength_) || forwardLength_ < 0.0001f) return;
    const float frontDistance_ = ownCollider_->GetCapsuleData().radius + targetCollider_->GetCapsuleData().radius + 0.5f;
    // 出現後の短い追従も正面位置を目標にし、プレイヤーの真上へ寄せない。
    targetPosition_.x += forward_.x / forwardLength_ * frontDistance_;
    targetPosition_.z += forward_.z / forwardLength_ * frontDistance_;
    const float dx_ = targetPosition_.x - position_.x;
    const float dz_ = targetPosition_.z - position_.z;
    const float distance_ = std::sqrt(dx_ * dx_ + dz_ * dz_);
    if (distance_ <= 0.05f) return;
    const float step_ = (std::min)(distance_, 6.0f * deltaTime_);
    DirectX::XMFLOAT3 groundStart_{ position_.x, shadowGroundPosition.y, position_.z };
    DirectX::XMFLOAT3 groundEnd_{ position_.x + dx_ / distance_ * step_, shadowGroundPosition.y, position_.z + dz_ / distance_ * step_ };
    if (!CanWarpPath(groundStart_, groundEnd_)) return;
    // 地上経路だけでなく空中の高さでも経路を検査し、梁を横切らないようにする。
    auto scene_ = PhysXSystem::GetScene();
    if (!scene_) return;
    const auto capsule_ = ownCollider_->GetCapsuleData();
    DirectX::XMFLOAT3 center_{};
    DirectX::XMStoreFloat3(&center_, DirectX::XMVectorScale(DirectX::XMVectorAdd(capsule_.p0, capsule_.p1), 0.5f));
    const float halfHeight_ = 0.5f * DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(capsule_.p0, capsule_.p1)));
    physx::PxQueryFilterData filter_;
    filter_.flags = physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::ePREFILTER;
    filter_.data.word3 = ownCollider_->GetCollisionOwnerId();
    physx::PxSweepBuffer hit_;
    const physx::PxCapsuleGeometry geometry_((std::max)(0.01f, capsule_.radius - ownCollider_->GetContactOffset()), halfHeight_);
    const physx::PxTransform pose_(physx::PxVec3(center_.x, center_.y, center_.z), physx::PxQuat(physx::PxHalfPi, physx::PxVec3(0, 0, 1)));
    if (scene_->sweep(geometry_, pose_, physx::PxVec3(dx_ / distance_, 0, dz_ / distance_), step_, hit_, physx::PxHitFlag::eDEFAULT,
        filter_, PhysXSystem::GetCharacterControllerQueryFilterCallback()) && hit_.hasBlock) return;
    movement_->SetHorizontalVelocity({ dx_ / distance_ * step_ / deltaTime_, 0, dz_ / distance_ * step_ / deltaTime_ });
}

void HumanBossActionComponent::ShowShadowArea()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::ShowShadowArea");
    const auto owner_ = GetGameObject();
    const auto phase_ = execution.GetCurrentPhase();
    if (!owner_ || !owner_->transform || !shadowChain || !phase_ || shadowAreaTemplate.empty()) return;
    auto& manager_ = AttackTemplateManager::Instance();
    if (manager_.GetTemplates().empty()) manager_.LoadAll();
    const auto attack_ = manager_.FindTemplate(shadowAreaTemplate);
    if (!attack_ || attack_->attackEffects.empty() || attack_->attackEffects[0].hitShapes.empty()) { Stop(); return; }
    if (!ResolveShadowGround(owner_->transform->GetWorldPosition(), shadowLandingPosition)) { Stop(); return; }
    shadowAttackSnapshot = std::make_shared<AttackTemplate>(*attack_);
    shadowLandingLocked = true;
    shadowAreaStruck = false;
    ParticleSpawnRequest particle_;
    particle_.assetPath = "Data/Particle/EnemyShadowMudBurst.json";
    particle_.position = shadowLandingPosition;
    particle_.lifeTime = 0.45f;
    owner_->GetManager().GetEffectManager().SpawnParticle(particle_);
    DangerAreaTemplate danger_;
    danger_.shapeType = DangerAreaShapeType::CIRCLE;
    danger_.materialPath = shadowDangerMaterial;
    danger_.radius = shadowAttackSnapshot->attackEffects[0].hitShapes[0].radius;
    danger_.lifeTime = phase_->duration;
    if (shadowAreaTemplate != "HumanBossShadowDomain")
    {
        // 空中の実高度と落下速度に合わせ、予兆の発光を着地直前へ寄せる。
        const float height_ = (std::max)(0.0f, owner_->transform->GetWorldPosition().y - shadowLandingPosition.y);
        danger_.lifeTime += height_ / (std::max)(1.0f, shadowDiveFallSpeed) + 0.15f;
    }
    owner_->GetOrAddComponent<DangerAreaComponent>()->ShowAt(danger_, shadowLandingPosition);
    // 泥面は判定のないMeshVFXで生成し、予兆中にダメージを発生させない。
    MeshVFXSpawnRequest request_;
    request_.assetPath = "Data/VFX/EnemyShadowMudPool.json";
    request_.position = shadowLandingPosition;
    request_.lifeTime = danger_.lifeTime;
    request_.lifetimeOwner = owner_->GetComponent<HumanBossActionComponent>();
    const auto visual_ = owner_->GetManager().GetEffectManager().SpawnMeshVFX(request_);
    if (visual_ && visual_->transform)
    {
        visual_->transform->localScale = { danger_.radius, 1.0f, danger_.radius };
        visual_->transform->MarkDirty();
        visual_->UpdateTransform();
        if (const auto mesh_ = visual_->GetComponent<MeshVFXComponent>())
            for (auto& part_ : mesh_->GetAsset().parts)
            {
                part_.duration = danger_.lifeTime;
                part_.loop = false;
                part_.animateAlpha = false;
            }
        shadowVisuals.push_back(visual_);
    }
}

void HumanBossActionComponent::StrikeShadowArea()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::StrikeShadowArea");
    const auto owner_ = GetGameObject();
    if (!owner_ || !shadowLandingLocked || !shadowAttackSnapshot || shadowAreaStruck) return;
    if (shadowAreaTemplate != "HumanBossShadowDomain")
    {
        const auto movement_ = owner_->GetComponent<CharacterMovementComponent>();
        const auto collider_ = movement_ ? movement_->ResolveCollider() : nullptr;
        if (!collider_ || !collider_->isGrounded) return;
    }
    shadowAreaStruck = true;
    // 接地後も実際の地面を再確認し、消えた足場へ攻撃を発生させない。
    DirectX::XMFLOAT3 ground_{};
    if (!ResolveShadowGround(shadowLandingPosition, ground_) || std::abs(ground_.y - shadowLandingPosition.y) > 0.5f)
    { Stop(); return; }
    if (const auto danger_ = owner_->GetComponent<DangerAreaComponent>()) danger_->StopAll();
    const auto instance_ = owner_->GetManager().GetAttackSystem().CreateAttackInstance();
    // 複数の範囲判定を生成しても、接地した中心から発動音を一度だけ鳴らす。
    const auto& feedback_ = shadowAttackSnapshot->feedback;
    AudioManager::Instance().PlaySpatialAttackSound(feedback_.startSoundPath, shadowLandingPosition,
        feedback_.volume, feedback_.minDistance, feedback_.maxDistance);
    if (shadowAreaTemplate != "HumanBossShadowDomain")
    {
        const float radius_ = shadowAttackSnapshot->attackEffects.front().hitShapes.front().radius;
        owner_->GetManager().GetEffectManager().SpawnGroundMark(GroundMarkShape::IMPACT, ground_, radius_, radius_);
    }
    for (const auto& effect_ : shadowAttackSnapshot->attackEffects)
    {
        const auto object_ = owner_->GetManager().GetEffectManager().SpawnAttackEffect(effect_, *shadowAttackSnapshot, owner_.get(), instance_);
        if (object_ && object_->transform)
        {
            // 予兆開始時の中心を使用し、攻撃時にPlayerやOwnerへ追尾し直さない。
            object_->transform->ApplyWorldMatrix(DirectX::XMMatrixTranslation(shadowLandingPosition.x, shadowLandingPosition.y, shadowLandingPosition.z));
            object_->UpdateTransform();
            if (const auto mesh_ = object_->GetComponent<MeshVFXComponent>())
            {
                const float radius_ = shadowAttackSnapshot->attackEffects[0].hitShapes[0].radius;
                for (auto& part_ : mesh_->GetAsset().parts)
                {
                    part_.scaleStart.x *= radius_;
                    part_.scaleStart.z *= radius_;
                    part_.scaleEnd.x *= radius_;
                    part_.scaleEnd.z *= radius_;
                }
            }
        }
    }
}

void HumanBossActionComponent::NotifyCounterOpportunity(std::shared_ptr<GameObject> attacker_)
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::NotifyCounterOpportunity");
    const auto owner_ = GetGameObject();
    if (!IsEnabled() || !owner_ || owner_->IsPendingDestroy()) return;
    // 武器や部位が攻撃Ownerでも、親をたどって自身の攻撃だけを対象にする。
    while (attacker_)
    {
        if (attacker_ == owner_)
        {
            owner_->ExecuteAction("HumanBoss_ReleaseCounterInvulnerability");
            return;
        }
        const auto parent_ = attacker_->transform ? attacker_->transform->parent.lock() : nullptr;
        attacker_ = parent_ ? parent_->gameObject.lock() : nullptr;
    }
}

void HumanBossActionComponent::ReleaseCounterInvulnerability()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::ReleaseCounterInvulnerability");
    SetShadowInvulnerable(false);
    // 本体だけでなく部位のAction由来の無敵も解除し、反撃の接近補正と被弾を受け付ける。
    const std::function<void(const std::shared_ptr<GameObject>&)> release_ = [&](const std::shared_ptr<GameObject>& object_)
    {
        if (!object_) return;
        for (const auto& hurt_ : object_->GetComponents<HurtBox>())
            if (hurt_) hurt_->SetInvincible(false);
        if (object_->transform)
            for (const auto& child_ : object_->transform->children)
                if (child_) release_(child_->gameObject.lock());
    };
    release_(GetGameObject());
}

void HumanBossActionComponent::SetShadowInvulnerable(bool invulnerable_)
{
    if (invulnerable_ == shadowInvulnerabilityApplied) return;
    BS_PROFILE_SCOPE("HumanBossActionComponent::SetShadowInvulnerable");
    if (!invulnerable_)
    {
        for (const auto& reference_ : shadowProtectedHurtBoxes)
            if (const auto hurt_ = reference_.lock()) hurt_->SetTemporaryInvincible(false);
        shadowProtectedHurtBoxes.clear();
        shadowInvulnerabilityApplied = false;
        return;
    }
    // Scene全体を検索せず、自身と装備・部位のHurtBoxだけを保護する。
    const std::function<void(const std::shared_ptr<GameObject>&)> protect_ = [&](const std::shared_ptr<GameObject>& object_)
    {
        if (!object_) return;
        for (const auto& hurt_ : object_->GetComponents<HurtBox>())
        {
            if (!hurt_) continue;
            hurt_->SetTemporaryInvincible(true);
            shadowProtectedHurtBoxes.push_back(hurt_);
        }
        if (object_->transform)
            for (const auto& child_ : object_->transform->children)
                if (child_) protect_(child_->gameObject.lock());
    };
    protect_(GetGameObject());
    shadowInvulnerabilityApplied = true;
}

void HumanBossActionComponent::SetShadowVisible(bool visible_)
{
    if (visible_)
    {
        for (const auto& entry_ : hiddenShadowModels)
            if (const auto model_ = entry_.first.lock()) model_->SetRuntimeVisible(entry_.second);
        hiddenShadowModels.clear();
        shadowModelsHidden = false;
        return;
    }
    if (shadowModelsHidden || !shadowHideModels) return;
    BS_PROFILE_SCOPE("HumanBossActionComponent::SetShadowVisible");
    const std::function<void(const std::shared_ptr<GameObject>&)> hide_ = [&](const std::shared_ptr<GameObject>& object_)
    {
        if (!object_) return;
        for (const auto& model_ : object_->GetComponents<GltfModelCommponennt>())
        {
            if (!model_) continue;
            hiddenShadowModels.emplace_back(model_, model_->IsRuntimeVisible());
            model_->SetRuntimeVisible(false);
        }
        if (object_->transform)
            for (const auto& child_ : object_->transform->children)
                if (child_) hide_(child_->gameObject.lock());
    };
    hide_(GetGameObject());
    shadowModelsHidden = true;
}

void HumanBossActionComponent::ClearShadowArea()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::ClearShadowArea");
    const auto owner_ = GetGameObject();
    if (owner_)
    {
        if (const auto danger_ = owner_->GetComponent<DangerAreaComponent>()) danger_->StopAll();
        for (const auto& reference_ : shadowVisuals)
            if (const auto visual_ = reference_.lock()) owner_->GetManager().GetEffectManager().StopEffect(visual_, this);
    }
    shadowVisuals.clear();
    shadowAttackSnapshot.reset();
    shadowLandingLocked = false;
}

bool HumanBossActionComponent::BuildShadowPath(HumanBossShadowShift mode_,
    const std::shared_ptr<GameObject>& target_, float side_, std::vector<DirectX::XMFLOAT3>& waypoints_) const
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::BuildShadowPath");
    waypoints_.clear();
    const auto owner_ = gameObject.lock();
    const auto ownCollider_ = ResolveAttackCharacter(owner_);
    const auto targetCollider_ = ResolveAttackCharacter(target_);
    const auto movement_ = owner_ ? owner_->GetComponent<CharacterMovementComponent>() : nullptr;
    const auto health_ = target_ ? target_->GetComponent<HealthComponent>() : nullptr;
    if (!owner_ || !owner_->transform || !target_ || !target_->transform || !ownCollider_ ||
        !targetCollider_ || !movement_ || !movement_->ResolveCollider() || !ownCollider_->enabled || !targetCollider_->enabled || (health_ && health_->IsDead())) return false;
    const auto start_ = owner_->transform->GetWorldPosition();
    const auto targetPosition_ = target_->transform->GetWorldPosition();
    const float dx_ = targetPosition_.x - start_.x;
    const float dz_ = targetPosition_.z - start_.z;
    const float distance_ = std::sqrt(dx_ * dx_ + dz_ * dz_);
    if (!std::isfinite(distance_) || distance_ < 0.1f) return false;
    const DirectX::XMFLOAT3 direction_{ dx_ / distance_, 0.0f, dz_ / distance_ };
    const float sign_ = side_ < 0.0f ? -1.0f : 1.0f;
    const DirectX::XMFLOAT3 lateral_{ direction_.z * sign_, 0.0f, -direction_.x * sign_ };
    const float clearance_ = (std::max)(2.2f, ownCollider_->GetCapsuleData().radius + targetCollider_->GetCapsuleData().radius + 0.4f);
    auto destination_ = start_;
    if (mode_ == HumanBossShadowShift::APPROACH)
    {
        if (distance_ <= clearance_ + 0.75f) return false;
        const float travel_ = (std::min)(shadowApproachDistance, distance_ - clearance_);
        destination_.x += direction_.x * travel_;
        destination_.z += direction_.z * travel_;
    }
    else if (mode_ == HumanBossShadowShift::BACK)
    {
        destination_.x -= direction_.x * shadowBackDistance;
        destination_.z -= direction_.z * shadowBackDistance;
    }
    else if (mode_ == HumanBossShadowShift::SIDE)
    {
        destination_.x += lateral_.x * shadowSideDistance;
        destination_.z += lateral_.z * shadowSideDistance;
    }
    else
    {
        // 対象の正面を横切らず、側面の中継点から背後へ回る。
        const auto forward_ = target_->transform->GetFront();
        const float length_ = std::sqrt(forward_.x * forward_.x + forward_.z * forward_.z);
        if (length_ < 0.0001f) return false;
        const DirectX::XMFLOAT3 waypoint_{ targetPosition_.x + lateral_.x * (clearance_ + 0.6f), start_.y,
            targetPosition_.z + lateral_.z * (clearance_ + 0.6f) };
        waypoints_.push_back(waypoint_);
        destination_.x = targetPosition_.x - forward_.x / length_ * (clearance_ + 0.5f);
        destination_.z = targetPosition_.z - forward_.z / length_ * (clearance_ + 0.5f);
    }
    waypoints_.push_back(destination_);
    auto previous_ = start_;
    float totalDistance_ = 0.0f;
    for (const auto& waypoint_ : waypoints_)
    {
        const float segmentX_ = waypoint_.x - previous_.x;
        const float segmentZ_ = waypoint_.z - previous_.z;
        totalDistance_ += std::sqrt(segmentX_ * segmentX_ + segmentZ_ * segmentZ_);
        if (totalDistance_ > shadowMaximumPathDistance || !CanWarpPath(previous_, waypoint_)) return false;
        previous_ = waypoint_;
    }
    return true;
}

bool HumanBossActionComponent::PlanShadowTrajectory(HumanBossShadowShift mode_,
    const std::vector<DirectX::XMFLOAT3>& waypoints_)
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::PlanShadowTrajectory");
    constexpr int SAMPLE_COUNT = 64;
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->transform || waypoints_.empty()) return false;
    shadowBaseWaypoints = { owner_->transform->GetWorldPosition() };
    shadowBaseWaypoints.insert(shadowBaseWaypoints.end(), waypoints_.begin(), waypoints_.end());
    const auto curve_ = owner_->GetComponent<CurveComponent>();
    // Editorで選択した既存Curveを読み込み、実行中は評価結果を固定する。
    if (curve_)
        for (const auto& name_ : { shadowSwayCurve, shadowSpeedCurve })
            if (!name_.empty() && !curve_->HasTrack(name_)) curve_->LoadTrackAsset("Data/Curve/" + name_ + ".json");
    const auto evaluate_ = [&](const std::string& name_, float progress_, float fallback_)
    {
        const float value_ = curve_ && curve_->HasTrack(name_) ? curve_->GetValue(name_, progress_) : fallback_;
        return std::isfinite(value_) ? std::clamp(value_, 0.0f, 4.0f) : fallback_;
    };
    std::uniform_real_distribution<float> random_(-1.0f, 1.0f);
    // 振幅・回数・位相・速度の乱数はここで一度だけ決定する。
    const float amplitude_ = shadowSwayEnabled ? shadowSwayWidth * shadowSwayStrength[static_cast<int>(mode_)] *
        (1.0f - (random_(shadowRandom) + 1.0f) * 0.5f * shadowSwayRandomness) : 0.0f;
    const float cycles_ = shadowSwayCycles * (1.0f + random_(shadowRandom) * shadowSwayRandomness * 0.3f);
    const float phase_ = random_(shadowRandom) * DirectX::XM_PI * shadowSwayRandomness;
    const float sign_ = random_(shadowRandom) < shadowSwayBias ? 1.0f : -1.0f;
    const float speedExponent_ = 1.0f + random_(shadowRandom) * shadowSwayRandomness * 0.2f;
    std::vector<float> baseDistances_{ 0.0f };
    const auto length_ = [](const DirectX::XMFLOAT3& a_, const DirectX::XMFLOAT3& b_)
    { const float x_ = b_.x - a_.x; const float z_ = b_.z - a_.z; return std::sqrt(x_ * x_ + z_ * z_); };
    for (std::size_t index_ = 1; index_ < shadowBaseWaypoints.size(); ++index_)
        baseDistances_.push_back(baseDistances_.back() + length_(shadowBaseWaypoints[index_ - 1], shadowBaseWaypoints[index_]));
    if (baseDistances_.back() < 0.0001f) return false;
    // BEHINDの中継点はHermite補間で滑らかにつなぎ、対象の側面を弧状に回る。
    const auto basePosition_ = [&](float progress_, bool smooth_)
    {
        const float distance_ = progress_ * baseDistances_.back();
        std::size_t segment_ = 0;
        while (segment_ + 2 < baseDistances_.size() && distance_ > baseDistances_[segment_ + 1]) ++segment_;
        const auto a_ = shadowBaseWaypoints[segment_];
        const auto b_ = shadowBaseWaypoints[segment_ + 1];
        const float u_ = std::clamp((distance_ - baseDistances_[segment_]) /
            (std::max)(0.0001f, baseDistances_[segment_ + 1] - baseDistances_[segment_]), 0.0f, 1.0f);
        if (!smooth_ || shadowBaseWaypoints.size() < 3)
            return DirectX::XMFLOAT3{ a_.x + (b_.x - a_.x) * u_, a_.y, a_.z + (b_.z - a_.z) * u_ };
        const auto previous_ = segment_ > 0 ? shadowBaseWaypoints[segment_ - 1] : a_;
        const auto next_ = segment_ + 2 < shadowBaseWaypoints.size() ? shadowBaseWaypoints[segment_ + 2] : b_;
        const float u2_ = u_ * u_, u3_ = u2_ * u_;
        return DirectX::XMFLOAT3{
            (2 * u3_ - 3 * u2_ + 1) * a_.x + (u3_ - 2 * u2_ + u_) * (b_.x - previous_.x) * 0.5f +
                (-2 * u3_ + 3 * u2_) * b_.x + (u3_ - u2_) * (next_.x - a_.x) * 0.5f,
            a_.y,
            (2 * u3_ - 3 * u2_ + 1) * a_.z + (u3_ - 2 * u2_ + u_) * (b_.z - previous_.z) * 0.5f +
                (-2 * u3_ + 3 * u2_) * b_.z + (u3_ - u2_) * (next_.z - a_.z) * 0.5f };
    };
    shadowPathValidated = false;
    for (int attempt_ = 0; attempt_ < 5 && !shadowPathValidated; ++attempt_)
    {
        const float width_ = attempt_ == 0 ? amplitude_ : attempt_ == 1 ? amplitude_ * 0.5f : attempt_ == 2 ? amplitude_ * 0.25f : 0.0f;
        const bool smooth_ = attempt_ < 4 && shadowSwayEnabled;
        shadowTrajectory.clear();
        shadowTrajectoryDistances = { 0.0f };
        bool safe_ = true;
        for (int index_ = 0; index_ <= SAMPLE_COUNT; ++index_)
        {
            const float progress_ = static_cast<float>(index_) / SAMPLE_COUNT;
            auto point_ = basePosition_(progress_, smooth_);
            const auto before_ = basePosition_((std::max)(0.0f, progress_ - 0.005f), smooth_);
            const auto after_ = basePosition_((std::min)(1.0f, progress_ + 0.005f), smooth_);
            const float tangentLength_ = (std::max)(0.0001f, length_(before_, after_));
            // 両端で必ず0になる包絡を使い、位相や左右の偏りがあっても終点をずらさない。
            const float envelope_ = std::pow((std::max)(0.0f, std::sin(DirectX::XM_PI * progress_)), shadowConvergence) *
                (std::min)(1.0f, evaluate_(shadowSwayCurve, progress_, 1.0f));
            const float wave_ = 0.8f * std::sin(DirectX::XM_2PI * cycles_ * progress_ + phase_) +
                0.2f * std::sin(DirectX::XM_2PI * cycles_ * 0.55f * progress_ + phase_ * 0.5f);
            const float offset_ = width_ * envelope_ * std::clamp(sign_ * wave_ + shadowSwayBias * 0.2f, -1.0f, 1.0f);
            point_.x += (after_.z - before_.z) / tangentLength_ * offset_;
            point_.z -= (after_.x - before_.x) / tangentLength_ * offset_;
            if (index_ == 0) point_ = shadowBaseWaypoints.front();
            if (index_ == SAMPLE_COUNT) point_ = shadowBaseWaypoints.back();
            if (!shadowTrajectory.empty())
            {
                // 蛇行した全区間にCapsule Sweepと足場確認を適用する。
                if (!CanWarpPath(shadowTrajectory.back(), point_)) safe_ = false;
                shadowTrajectoryDistances.push_back(shadowTrajectoryDistances.back() + length_(shadowTrajectory.back(), point_));
            }
            shadowTrajectory.push_back(point_);
        }
        if (safe_ && shadowTrajectoryDistances.back() <= shadowMaximumPathDistance)
        { shadowPathValidated = true; shadowActualWidth = width_; }
    }
    if (!shadowPathValidated) return false;
    shadowSpeedProgress = { 0.0f };
    float previousSpeed_ = 0.0f, peakSpeed_ = 0.0f;
    for (int index_ = 0; index_ <= SAMPLE_COUNT; ++index_)
    {
        const float progress_ = static_cast<float>(index_) / SAMPLE_COUNT;
        const float speed_ = std::pow(evaluate_(shadowSpeedCurve, progress_, 6.0f * progress_ * (1.0f - progress_)), speedExponent_);
        if (index_ > 0) shadowSpeedProgress.push_back(shadowSpeedProgress.back() + (previousSpeed_ + speed_) * 0.5f / SAMPLE_COUNT);
        peakSpeed_ = (std::max)(peakSpeed_, speed_);
        previousSpeed_ = speed_;
    }
    const float integral_ = shadowSpeedProgress.back();
    if (integral_ < 0.0001f) { shadowPathValidated = false; return false; }
    for (auto& progress_ : shadowSpeedProgress) progress_ /= integral_;
    // 蛇行の実距離とカーブの最大倍率から、最高速度を超えずに到着できる時間を確保する。
    shadowPlannedDuration = (std::max)(shadowMoveEndTime - shadowMoveStartTime,
        shadowTrajectoryDistances.back() * peakSpeed_ / integral_ / (std::max)(1.0f, shadowMoveSpeed));
    shadowTravelDistance = 0.0f;
    shadowPendingMove = false;
    shadowPathBlocked = false;
    return true;
}

DirectX::XMFLOAT3 HumanBossActionComponent::SampleShadowTrajectory(float distance_) const
{
    if (shadowTrajectory.empty()) return {};
    const auto iterator_ = std::lower_bound(shadowTrajectoryDistances.begin(), shadowTrajectoryDistances.end(), distance_);
    if (iterator_ == shadowTrajectoryDistances.begin()) return shadowTrajectory.front();
    if (iterator_ == shadowTrajectoryDistances.end()) return shadowTrajectory.back();
    const std::size_t index_ = static_cast<std::size_t>(iterator_ - shadowTrajectoryDistances.begin());
    const float rate_ = std::clamp((distance_ - shadowTrajectoryDistances[index_ - 1]) /
        (std::max)(0.0001f, shadowTrajectoryDistances[index_] - shadowTrajectoryDistances[index_ - 1]), 0.0f, 1.0f);
    const auto a_ = shadowTrajectory[index_ - 1];
    const auto b_ = shadowTrajectory[index_];
    return { a_.x + (b_.x - a_.x) * rate_, a_.y, a_.z + (b_.z - a_.z) * rate_ };
}

bool HumanBossActionComponent::CanShadowShift(const SkillDefinition& skill_, HumanBossShadowShift mode_,
    const std::shared_ptr<GameObject>& target_, float side_) const
{
    // 移動を二重適用する設定や、時間指定のない移動Phaseは実行しない。
    if (skill_.category != SkillCategory::UTILITY || skill_.phases.empty() ||
        skill_.phases[0].endCondition != SkillPhaseEnd::DURATION || skill_.phases[0].duration <= 0.0f ||
        skill_.phases[0].forwardSpeed != 0.0f || !skill_.phases[0].forwardSpeedCurveName.empty() ||
        !skill_.phases[0].motionWarpPreset.empty()) return false;
    const auto owner_ = gameObject.lock();
    const auto animation_ = owner_ ? AnimationGraphBridgeComponent::Acquire(owner_) : nullptr;
    if (!animation_) return false;
    const auto state_ = animation_->GetStateCatalog().find(skill_.phases[0].animationState);
    if (state_ == animation_->GetStateCatalog().end() || !state_->second) return false;
    if (state_->second->rootMotion.enable && state_->second->rootMotionScale != 0.0f &&
        (state_->second->rootMotion.applyPositionX || state_->second->rootMotion.applyPositionZ)) return false;
    std::vector<DirectX::XMFLOAT3> waypoints_;
    // 横・背後移動は左右のどちらかが塞がれていても、反対側を検討する。
    const auto canFollowUp_ = [&]()
    {
        if (skill_.phases.size() == 1) return true;
        if (!target_ || !target_->transform || waypoints_.empty()) return false;
        const auto targetPosition_ = target_->transform->GetWorldPosition();
        const auto destination_ = waypoints_.back();
        const float dx_ = targetPosition_.x - destination_.x;
        const float dz_ = targetPosition_.z - destination_.z;
        float reach_ = 3.5f;
        if (skill_.name != "SHADOW_SLASH")
        {
            auto& manager_ = AttackTemplateManager::Instance();
            if (manager_.GetTemplates().empty()) manager_.LoadAll();
            const std::string id_ = skill_.name == "SHADOW_DIVE_01" ? "HumanBossShadowDive01" :
                skill_.name == "SHADOW_DIVE_02" ? "HumanBossShadowDive02" : "HumanBossShadowDomain";
            const auto attack_ = manager_.FindTemplate(id_);
            if (!attack_ || attack_->attackEffects.empty() || attack_->attackEffects[0].hitShapes.empty()) return false;
            reach_ = attack_->attackEffects[0].hitShapes[0].radius - 0.25f;
            if (skill_.name != "SHADOW_DOMAIN") reach_ += 6.0f * shadowLandingTrackTime;
        }
        // 移動後も明らかに届かない派生は抽選へ出さず、単独移動を残す。
        return std::sqrt(dx_ * dx_ + dz_ * dz_) <= reach_;
    };
    return (BuildShadowPath(mode_, target_, side_, waypoints_) && canFollowUp_()) ||
        ((mode_ == HumanBossShadowShift::SIDE || mode_ == HumanBossShadowShift::BEHIND) &&
            BuildShadowPath(mode_, target_, -side_, waypoints_) && canFollowUp_());
}

bool HumanBossActionComponent::BeginShadowShift(const SkillDefinition& skill_, HumanBossShadowShift mode_,
    const std::shared_ptr<GameObject>& target_, float side_)
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::BeginShadowShift");
    if (!CanShadowShift(skill_, mode_, target_, side_)) return false;
    std::vector<DirectX::XMFLOAT3> waypoints_;
    if (!BuildShadowPath(mode_, target_, side_, waypoints_) &&
        !((mode_ == HumanBossShadowShift::SIDE || mode_ == HumanBossShadowShift::BEHIND) &&
            BuildShadowPath(mode_, target_, -side_, waypoints_))) return false;
    if (!PlanShadowTrajectory(mode_, waypoints_)) return false;
    auto plannedSkill_ = skill_;
    plannedSkill_.phases[0].duration = (std::max)(plannedSkill_.phases[0].duration,
        shadowMoveStartTime + shadowPlannedDuration + shadowReappearDuration);
    if (plannedSkill_.phases[0].duration >= plannedSkill_.phases[0].maximumDuration) return false;
    if (!Begin(plannedSkill_, HumanBossDefense::NONE, {}, target_)) return false;
    shadowChain = skill_.phases.size() > 1;
    shadowAreaTemplate = skill_.name == "SHADOW_DIVE_01" ? "HumanBossShadowDive01" :
        skill_.name == "SHADOW_DIVE_02" ? "HumanBossShadowDive02" : skill_.name == "SHADOW_DOMAIN" ? "HumanBossShadowDomain" : "";
    shadowAreaStruck = false;
    shadowLandingLocked = false;
    shadowDiveFallSpeed = 27.6f;
    for (const auto& phase_ : skill_.phases)
        if (phase_.endCondition == SkillPhaseEnd::GROUND_CONTACT && phase_.verticalSpeed < 0.0f)
            shadowDiveFallSpeed = std::abs(phase_.verticalSpeed);
    shadowWaypoints = std::move(waypoints_);
    shadowWaypoint = 0;
    shadowElapsed = 0.0f;
    shadowEffectTimer = 0.0f;
    shadowHasEffectPosition = false;
    shadowArrived = false;
    shadowShiftActive = true;
    SetShadowInvulnerable(true);
    return true;
}

void HumanBossActionComponent::SpawnShadowEffect()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::SpawnShadowEffect");
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->transform) return;
    MeshVFXSpawnRequest request_;
    const bool usesPurpleShadow_ = shadowChain || rangedSkill == "SHADOW_SLASH" ||
        rangedSkill == "SHADOW_CHARGE" || rangedSkill == "SHADOW_RAGNAROK";
    request_.assetPath = usesPurpleShadow_ ? "Data/VFX/EnemyShadowShift.json" : "Data/VFX/EnemyMudShadowShift.json";
    request_.position = owner_->transform->GetWorldPosition();
    request_.lifeTime = 0.55f;
    const auto mesh_ = owner_->GetManager().GetEffectManager().SpawnMeshVFX(request_);
    owner_->GetManager().GetEffectManager().RegisterAttackEffectOwner(mesh_, owner_);
    // 移動点へ泥煙と発光粒子を重ね、Meshだけの残像を補う。
    ParticleSpawnRequest particle_;
    particle_.assetPath = usesPurpleShadow_ ? "Data/Particle/EnemyShadowTravel.json" : "Data/Particle/EnemyMudShadowTravel.json";
    particle_.position = request_.position;
    particle_.lifeTime = 0.95f;
    const auto particleObject_ = owner_->GetManager().GetEffectManager().SpawnParticle(particle_);
    owner_->GetManager().GetEffectManager().RegisterAttackEffectOwner(particleObject_, owner_);
    shadowLastEffectPosition = request_.position;
    shadowHasEffectPosition = true;
}

void HumanBossActionComponent::UpdateShadowShift(float deltaTime_)
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::UpdateShadowShift");
    const auto owner_ = GetGameObject();
    const auto movement_ = owner_ ? owner_->GetComponent<CharacterMovementComponent>() : nullptr;
    const auto target_ = attackTarget.lock();
    if (!owner_ || !owner_->transform || !movement_ || !target_ || !target_->transform)
    {
        Stop();
        return;
    }
    shadowElapsed += (std::max)(0.0f, deltaTime_);
    SetShadowVisible(shadowElapsed < shadowMoveStartTime || shadowArrived || shadowPathBlocked);
    movement_->StopHorizontalMovement();
    const auto blackboard_ = owner_->GetComponent<BlackboardComponent>();
    if (blackboard_) blackboard_->SetVector2(AnimationGraphBlackboardKeys::ANIMATION_CATEGORY, AnimationGraphBlackboardKeys::GUARD_MOVE_INPUT, { 0.0f, 0.0f });
    const auto position_ = owner_->transform->GetWorldPosition();
    FaceTarget({ target_->transform->GetWorldPosition().x - position_.x, 0.0f,
        target_->transform->GetWorldPosition().z - position_.z });
    // PhysXが前フレームの移動を反映したことを確認してから、経路距離を進める。
    if (shadowPendingMove)
    {
        const float pendingX_ = shadowPendingPosition.x - position_.x;
        const float pendingZ_ = shadowPendingPosition.z - position_.z;
        if (pendingX_ * pendingX_ + pendingZ_ * pendingZ_ <= 0.000025f)
        { shadowTravelDistance = shadowPendingDistance; shadowPendingMove = false; }
    }
    if (shadowElapsed < shadowMoveStartTime || shadowArrived || shadowPathBlocked || shadowTrajectory.empty()) return;
    const auto final_ = shadowTrajectory.back();
    const float finalX_ = final_.x - position_.x, finalZ_ = final_.z - position_.z;
    if (shadowTravelDistance >= shadowTrajectoryDistances.back() - 0.0001f && finalX_ * finalX_ + finalZ_ * finalZ_ <= 0.000025f)
    {
        shadowArrived = true;
        SetShadowVisible(true);
        SpawnShadowEffect();
        return;
    }
    const float progress_ = std::clamp((shadowElapsed - shadowMoveStartTime) / shadowPlannedDuration, 0.0f, 1.0f);
    const float sample_ = progress_ * static_cast<float>(shadowSpeedProgress.size() - 1);
    const auto index_ = (std::min)(static_cast<std::size_t>(sample_), shadowSpeedProgress.size() - 2);
    const float rate_ = sample_ - static_cast<float>(index_);
    const float distanceProgress_ = shadowSpeedProgress[index_] + (shadowSpeedProgress[index_ + 1] - shadowSpeedProgress[index_]) * rate_;
    const float nextDistance_ = (std::min)(shadowTrajectoryDistances.back(), (std::min)(
        distanceProgress_ * shadowTrajectoryDistances.back(), shadowTravelDistance + shadowMoveSpeed * (std::max)(0.0f, deltaTime_)));
    const auto destination_ = SampleShadowTrajectory((std::max)(shadowTravelDistance, nextDistance_));
    const float dx_ = destination_.x - position_.x, dz_ = destination_.z - position_.z;
    const float remaining_ = std::sqrt(dx_ * dx_ + dz_ * dz_);
    if (remaining_ < 0.0001f || deltaTime_ <= 0.0001f) return;
    const float step_ = (std::min)(remaining_, shadowMoveSpeed * deltaTime_);
    const DirectX::XMFLOAT3 next_{ position_.x + dx_ / remaining_ * step_, position_.y, position_.z + dz_ / remaining_ * step_ };
    if (!CanWarpPath(position_, next_))
    {
        // 動的障害物に遮断された場合は到着扱いにせず、安全な位置で終了する。
        shadowPathBlocked = true;
        SetShadowVisible(true);
        status = "Shadow shift stopped: curved path blocked or ground missing.";
        SpawnShadowEffect();
        return;
    }
    shadowPendingMove = true;
    shadowPendingPosition = next_;
    shadowPendingDistance = step_ >= remaining_ - 0.0001f ? nextDistance_ : shadowTravelDistance;
    movement_->SetHorizontalVelocity({ dx_ / remaining_ * step_ / deltaTime_, 0.0f, dz_ / remaining_ * step_ / deltaTime_ });
    if (blackboard_)
    {
        // 注視を維持したまま、移動方向を歩行Blendのローカル入力へ変換する。
        const float yaw_ = owner_->transform->GetWorldRotation().y * DirectX::XM_PI / 180.0f;
        blackboard_->SetVector2(AnimationGraphBlackboardKeys::ANIMATION_CATEGORY, AnimationGraphBlackboardKeys::GUARD_MOVE_INPUT,
            { (std::cos(yaw_) * dx_ - std::sin(yaw_) * dz_) / remaining_,
              (std::sin(yaw_) * dx_ + std::cos(yaw_) * dz_) / remaining_ });
    }
    shadowEffectTimer -= deltaTime_;
    const float effectX_ = position_.x - shadowLastEffectPosition.x;
    const float effectZ_ = position_.z - shadowLastEffectPosition.z;
    // 高速移動でも残像の間が開きすぎないよう、時間と移動距離で発生間隔を管理する。
    if (!shadowHasEffectPosition || shadowEffectTimer <= 0.0f ||
        effectX_ * effectX_ + effectZ_ * effectZ_ >= shadowEffectSpacing * shadowEffectSpacing)
    { SpawnShadowEffect(); shadowEffectTimer = shadowEffectInterval; }
}

bool HumanBossActionComponent::CanStart(const SkillDefinition& skill_, HumanBossDefense defense_,
    const std::shared_ptr<GameObject>& target_) const
{
    const auto owner_ = gameObject.lock();
    validationError.clear();
    if (!owner_ || execution.IsActive()) { validationError = "Owner missing or another action is running."; return false; }
    if (skill_.name == "COUNTER_ATTACK" || skill_.name == "GUARD_EXECUTION")
    {
        const auto player_ = target_ ? target_->GetComponent<PlayerControllerComponent>() : nullptr;
        if (!player_ || !player_->CanBeginEnemyExecution()) { validationError = "Execution target or player graph unavailable."; return false; }
        if (skill_.name == "GUARD_EXECUTION" && !CanCaptureGuardingPlayer(target_))
        { validationError = "Guard execution requires a grounded guarding target within reach."; return false; }
    }
    if ((defense_ == HumanBossDefense::GUARD || defense_ == HumanBossDefense::JUST_GUARD) && !owner_->GetComponent<GuardCollider>()) { validationError = "GuardCollider missing."; return false; }
    if (defense_ == HumanBossDefense::DODGE)
    {
        const auto movement_ = owner_->GetComponent<CharacterMovementComponent>();
        if (!owner_->GetComponent<HurtBox>() || !movement_ || !movement_->ResolveCollider()) { validationError = "Dodge requires HurtBox, Movement and Collider."; return false; }
    }
    const auto animation_ = AnimationGraphBridgeComponent::Acquire(owner_);
    if (!animation_ || animation_->IsSkillOwned()) { validationError = "Animation missing or controlled by another skill."; return false; }
    // RootMotionを使うPhaseへ前進速度も足すと、同じ移動が二重に適用される。
    for (const auto& phase_ : skill_.phases)
    {
        const auto state_ = animation_->GetStateCatalog().find(phase_.animationState);
        if (state_ == animation_->GetStateCatalog().end() || !state_->second) continue;
        const auto& motion_ = state_->second->rootMotion;
        const bool usesHorizontalRootMotion_ = motion_.enable && state_->second->rootMotionScale != 0.0f &&
            (motion_.applyPositionX || motion_.applyPositionZ);
        if (usesHorizontalRootMotion_ && (phase_.forwardSpeed != 0.0f || !phase_.forwardSpeedCurveName.empty()))
        {
            validationError = "RootMotion phase must not also specify Forward Speed or its curve: " + phase_.animationState;
            return false;
        }
    }
    if (!SkillExecution::CanExecute(owner_, skill_, validationError)) return false;
    if (!CanStartRanged(skill_.name, target_)) return false;
    // 対になる反撃は専用の位置固定で間合いを作るため、通常攻撃の接近補正では除外しない。
    if (skill_.name == "COUNTER_ATTACK" || skill_.name == "GUARD_EXECUTION") return true;
    // 最初の打撃Phaseで開始可否を決め、補正OFFでも自然なRootMotionで届く範囲へ絞る。
    if (skill_.category != SkillCategory::ATTACK && skill_.category != SkillCategory::COUNTER) return true;
    for (const auto& phase_ : skill_.phases)
    {
        const auto state_ = animation_->GetStateCatalog().find(phase_.animationState);
        if (state_ == animation_->GetStateCatalog().end() || !state_->second ||
            std::none_of(state_->second->events.begin(), state_->second->events.end(),
                [](const AnimEvent& event_) { return event_.eventTemplateType == ANIMATION_EVENT_TEMPLATE_TYPE::ATTACK_TEMPLATE; })) continue;
        DirectX::XMFLOAT3 plannedMotion_{};
        float stopDistance_ = 0.0f;
        float endNormalizedTime_ = 0.0f;
        float playbackDuration_ = 0.0f;
        return BuildAttackAssistPlan(phase_, target_, plannedMotion_, stopDistance_, endNormalizedTime_, playbackDuration_);
    }
    return true;
}

bool HumanBossActionComponent::Begin(const SkillDefinition& skill_, HumanBossDefense defense_,
    const DirectX::XMFLOAT3& dodgeDirection_, const std::shared_ptr<GameObject>& target_)
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::Begin");
    InitializeAttackAssist();
    if (!CanStart(skill_, defense_, target_)) { status = validationError; return false; }
    currentDefense = defense_;
    justGuardSucceeded = false;
    guardSucceeded = false;
    const auto owner_ = GetGameObject();
    // 回避方向だけをBlendへ渡し、実移動はAnimationのRootMotionに任せる。
    if (defense_ == HumanBossDefense::DODGE && owner_->transform)
    {
        // DodgeのBlend入力は注視方向から見たローカル方向へ変換する。
        const float yaw_ = owner_->transform->GetWorldRotation().y * DirectX::XM_PI / 180.0f;
        const float directionLength_ = std::sqrt(dodgeDirection_.x * dodgeDirection_.x + dodgeDirection_.z * dodgeDirection_.z);
        if (const auto blackboard_ = owner_->GetComponent<BlackboardComponent>())
        {
            const float inverseSpeed_ = directionLength_ > 0.0001f ? 1.0f / directionLength_ : 0.0f;
            blackboard_->SetVector2(AnimationGraphBlackboardKeys::ANIMATION_CATEGORY, AnimationGraphBlackboardKeys::DODGE_INPUT,
                { (std::cos(yaw_) * dodgeDirection_.x - std::sin(yaw_) * dodgeDirection_.z) * inverseSpeed_,
                  (std::sin(yaw_) * dodgeDirection_.x + std::cos(yaw_) * dodgeDirection_.z) * inverseSpeed_ });
        }
    }
    if (const auto movement_ = owner_->GetComponent<CharacterMovementComponent>()) movement_->StopHorizontalMovement();
    SetWeaponTrails(false, true);
    rangedSkill = skill_.name;
    orbSpawnCommitted = false;
    if (rangedSkill == "SHADOW_CHARGE" || rangedSkill == "SHADOW_ORB" || rangedSkill == "SHADOW_RAGNAROK")
        rangedProgress = rangedSkill + ": starting";
    if (rangedSkill == "SHADOW_RAGNAROK") ragnarokRound = 0;
    attackTarget = target_;
    hasAttackTargetPosition = false;
    windupFacingTracked = false;
    windupTargetLocked = false;
    windupRotationCorrection = 0.0f;
    windupWarpPresetName.clear();
    attackStartPosition = owner_->transform ? owner_->transform->GetWorldPosition() : DirectX::XMFLOAT3{};
    if (target_ && target_->transform) attackInitialTargetPosition = attackTargetPosition = target_->transform->GetWorldPosition();
    for (const auto& phase_ : skill_.phases)
        if (!phase_.motionWarpPreset.empty()) { windupWarpPresetName = phase_.motionWarpPreset; break; }
    attackAssistSkill = skill_.category == SkillCategory::ATTACK || skill_.category == SkillCategory::COUNTER;
    const std::weak_ptr<HumanBossActionComponent> weak_ = owner_->GetComponent<HumanBossActionComponent>();
    const auto runtimeSkill_ = PrepareRangedSkill(skill_);
    if (!execution.Start(owner_, runtimeSkill_, [weak_](const SkillPhase& phase_)
    {
        if (const auto self_ = weak_.lock()) { self_->BeginAttackAssist(phase_); self_->BeginRangedPhase(phase_); }
    }, [weak_](const SkillPhase& phase_)
    {
        const auto self_ = weak_.lock();
        return self_ && self_->CanContinueAttack(phase_);
    }, true))
    {
        status = execution.GetError();
        currentDefense = HumanBossDefense::NONE;
        attackAssistSkill = false;
        if (const auto warp_ = owner_->GetComponent<MotionWarpComponent>()) warp_->Stop();
        return false;
    }
    if (!execution.IsActive() && (skill_.name == "SHADOW_CHARGE" || skill_.name == "SHADOW_ORB" || skill_.name == "SHADOW_RAGNAROK"))
    {
        if (!execution.GetError().empty()) status = execution.GetError();
        else if (status.empty()) status = "Ranged skill interrupted during start";
        return false; // 開始失敗に長いCooldownを付けず、AIへ失敗として返す。
    }
    attackActive = execution.IsActive() && (skill_.category == SkillCategory::ATTACK || skill_.category == SkillCategory::COUNTER ||
        skill_.name == "SHADOW_CHARGE" || skill_.name == "SHADOW_ORB" || skill_.name == "SHADOW_RAGNAROK");
    if (!execution.IsActive()) CloseDefense();
    SyncComboState();
    UpdateHitProtection(0.0f);
    if (skill_.name != "SHADOW_RAGNAROK" || execution.IsActive()) status.clear();
    return true;
}

void HumanBossActionComponent::Tick(float deltaTime_)
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::Tick");
    UpdateRagnarokParticles(deltaTime_);
    if (!attackAssistEnabled)
        if (const auto owner_ = GetGameObject())
            if (const auto warp_ = owner_->GetComponent<MotionWarpComponent>()) warp_->Stop();
    if (deltaTime_ > 0.0001f) assistFrameDeltaTime = deltaTime_;
    const bool active_ = execution.IsActive();
    if (hasAttackTargetPosition)
        if (const auto owner_ = GetGameObject())
            if (const auto warp_ = owner_->GetComponent<MotionWarpComponent>()) attackTargetPosition = warp_->GetTargetPosition();
    execution.Update(deltaTime_);
    if (shadowShiftActive && execution.IsActive())
    {
        if (execution.GetPhaseIndex() == 0) UpdateShadowShift(deltaTime_);
        else { SetShadowInvulnerable(false); shadowShiftActive = false; SetShadowVisible(true); if (!shadowArrived) SpawnShadowEffect(); }
    }
    if (shadowChain && execution.IsActive()) UpdateShadowDive(deltaTime_);
    if (execution.IsActive()) UpdateRangedAttack();
    if (active_ && !execution.IsActive() &&
        (execution.GetResult() == SkillExecutionResult::TIMEOUT || execution.GetResult() == SkillExecutionResult::PHASE_START_FAILED ||
            execution.GetResult() == SkillExecutionResult::FOLLOW_UP_REJECTED))
    {
        // 素材不足や通知待ちの上限で停止した場合も、残った攻撃イベントを閉じる。
        const auto owner_ = GetGameObject();
        const auto animation_ = owner_ ? AnimationGraphBridgeComponent::Acquire(owner_) : nullptr;
        if (animation_ && !animation_->IsSkillOwned() && animation_->GetStateCatalog().count("SS_Idle"))
            animation_->RequestStateTransition("SS_Idle", 0.1f);
        if (execution.GetResult() == SkillExecutionResult::FOLLOW_UP_REJECTED) StopAttackHitBoxes(false);
    }
    UpdateHitProtection(0.0f);
    if (execution.IsActive() && windupTracking)
        if (const auto owner_ = GetGameObject()) owner_->ExecuteAction("HumanBoss_WindupFaceTarget");
    if ((active_ || currentDefense != HumanBossDefense::NONE) && !execution.IsActive()) CloseDefense();
    SyncComboState();
    if (!execution.GetError().empty()) status = execution.GetError();
}

bool HumanBossActionComponent::CanCaptureGuardingPlayer(const std::shared_ptr<GameObject>& target_) const
{
    const auto actor_ = gameObject.lock();
    const auto player_ = target_ ? target_->GetComponent<PlayerControllerComponent>() : nullptr;
    if (!actor_ || !actor_->transform || !target_ || !target_->transform || !player_ ||
        player_->GetCurrentAction() != PLAYER_ACTION::GUARD || player_->IsAirborneForAction() || !player_->CanBeginEnemyExecution()) return false;
    const auto position_ = actor_->transform->GetWorldPosition();
    const auto targetPosition_ = target_->transform->GetWorldPosition();
    const float deltaX_ = targetPosition_.x - position_.x;
    const float deltaZ_ = targetPosition_.z - position_.z;
    return deltaX_ * deltaX_ + deltaZ_ * deltaZ_ <= guardExecutionReach * guardExecutionReach &&
        std::abs(targetPosition_.y - position_.y) <= 1.0f;
}

void HumanBossActionComponent::BeginExecutionRecoil()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::BeginExecutionRecoil");
    const auto actor_ = GetGameObject();
    const auto target_ = attackTarget.lock();
    const auto player_ = target_ ? target_->GetComponent<PlayerControllerComponent>() : nullptr;
    const auto phase_ = execution.GetCurrentPhase();
    if (!actor_ || !player_ || !phase_ ||
        !player_->BeginEnemyExecution(actor_, phase_->duration + 0.5f, executionTargetDistance, true))
    {
        status = "Execution recoil unavailable";
        Stop();
        return;
    }
    // 予備段階で中断されても同じ解除経路を使い、プレイヤーの操作制限を残さない。
    executionCapturedPlayer = target_;
    // 弾かれたひるみも同じ構図で見せ、エクスキューションへ連続してつなぐ。
    actor_->ExecuteAction("HumanBoss_ExecutionCamera");
}

void HumanBossActionComponent::BeginExecutionCapture()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::BeginExecutionCapture");
    const auto actor_ = GetGameObject();
    const auto target_ = attackTarget.lock();
    const auto player_ = target_ ? target_->GetComponent<PlayerControllerComponent>() : nullptr;
    if (!actor_ || !actor_->transform || !target_ || !target_->transform || !player_)
    { status = "Execution target unavailable"; Stop(); return; }
    // Animation切替時の通知で状態が変わった場合も、対ガードの掴み条件を再確認する。
    if (execution.GetSkillName() == "GUARD_EXECUTION" && !CanCaptureGuardingPlayer(target_))
    { status = "Guard execution avoided"; Stop(); return; }
    const auto position_ = actor_->transform->GetWorldPosition();
    const auto targetPosition_ = target_->transform->GetWorldPosition();
    if (const auto movement_ = actor_->GetComponent<CharacterMovementComponent>())
    {
        movement_->ResetMovementRequests();
        movement_->SetFacingDirection({ targetPosition_.x - position_.x, 0.0f, targetPosition_.z - position_.z }, true);
    }
    const auto scale_ = actor_->transform->GetWorldScale();
    const float yaw_ = std::atan2(targetPosition_.x - position_.x, targetPosition_.z - position_.z);
    actor_->transform->ApplyWorldMatrix(DirectX::XMMatrixScaling(scale_.x, scale_.y, scale_.z) * DirectX::XMMatrixRotationY(yaw_) *
        DirectX::XMMatrixTranslation(position_.x, position_.y, position_.z));
    actor_->UpdateTransform();
    // 体のCollider同士が重ならない間合いを確保し、固定中の押し合いで敵がずれないようにする。
    float distance_ = executionTargetDistance;
    const auto actorCollider_ = ResolveAttackCharacter(actor_);
    const auto targetCollider_ = ResolveAttackCharacter(target_);
    if (actorCollider_ && targetCollider_)
        distance_ = (std::max)(distance_, actorCollider_->GetCapsuleData().radius + targetCollider_->GetCapsuleData().radius + 0.15f);
    if (!player_->BeginEnemyExecution(actor_, executionHoldTime, distance_))
    { status = "Execution player graph rejected capture"; Stop(); return; }
    executionCapturedPlayer = target_;
    executionGrabFeedbackPlayed = false;
    executionSlamFeedbackPlayed = false;
    // 予備段階のカメラは継続し、掴みの演出だけを拘束成立時に加える。
    actor_->ExecuteAction("HumanBoss_ExecutionCamera");
    actor_->ExecuteAction("HumanBoss_ExecutionGrabFeedback");
}

void HumanBossActionComponent::PlayExecutionCamera()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::PlayExecutionCamera");
    if (executionCapturedPlayer.expired() || executionCameraAction.empty()) return;
    // ひるみから本拘束へ移っても、進行中のカメラを先頭から再生し直さない。
    if (!executionCameraDirector.expired() && activeExecutionCameraAction == executionCameraAction) return;
    const auto actor_ = GetGameObject();
    if (!actor_ || !actor_->manager || !actor_->transform) return;
    const auto cameras_ = actor_->GetManager().GetCameraManager();
    const auto camera_ = cameras_ ? cameras_->GetSourceCamera() : nullptr;
    const auto cameraObject_ = camera_ ? camera_->GetGameObject() : nullptr;
    const auto director_ = cameraObject_ ? cameraObject_->GetComponent<CameraActionDirectorComponent>() : nullptr;
    if (!director_) return;
    const float yaw_ = DirectX::XMConvertToRadians(actor_->transform->GetWorldRotation().y);
    // TARGET_LOCAL_RELATIVEへ敵の正面を渡し、プレイヤーの向きで構図が反転しないようにする。
    director_->PlayActionAtPosition(executionCameraAction, actor_->transform->GetWorldPosition(),
        { std::sin(yaw_), 0.0f, std::cos(yaw_) });
    executionCameraDirector = director_;
    activeExecutionCameraAction = executionCameraAction;
}

void HumanBossActionComponent::PlayExecutionGrabFeedback()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::PlayExecutionGrabFeedback");
    if (executionCapturedPlayer.expired() || executionGrabFeedbackPlayed) return;
    const auto actor_ = GetGameObject();
    if (!actor_ || !actor_->manager) return;
    executionGrabFeedbackPlayed = true;
    if (const auto time_ = actor_->GetManager().GetTimeManager())
    {
        // 拘束側と被拘束側の時間を揃え、カメラとUIを止めずに掴んだ瞬間だけ強調する。
        constexpr uint32_t EXECUTION_SLOW_LAYERS = static_cast<uint32_t>(TarGetLayerType::Player) |
            static_cast<uint32_t>(TarGetLayerType::Enemy) | static_cast<uint32_t>(TarGetLayerType::PlayerBullet) |
            static_cast<uint32_t>(TarGetLayerType::EnemyBullet);
        time_->PlayTimedScale(EXECUTION_SLOW_LAYERS, executionSlowScale, 0.02f,
            executionSlowHoldDuration, 0.12f, 1.0f);
    }
    // TimedScaleと同様に短時間で終了する振動を使い、中断後に駆動を残さない。
    ControllerInput::Instance().PlayRumble(executionGrabRumbleStrength * 0.75f, executionGrabRumbleStrength, 0.2f, 3);
}

void HumanBossActionComponent::PlayExecutionSlamFeedback()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::PlayExecutionSlamFeedback");
    const auto target_ = executionCapturedPlayer.lock();
    const auto phase_ = execution.GetCurrentPhase();
    if (!target_ || executionSlamFeedbackPlayed || !phase_ || phase_->enterAction != "HumanBoss_ExecutionCapture") return;
    const auto actor_ = GetGameObject();
    if (!actor_ || !actor_->manager) return;
    executionSlamFeedbackPlayed = true;
    // 死亡通知で処理が再入しても重複せず、カメラの有無に関係なく叩きつけのダメージを与える。
    if (const auto health_ = target_->GetComponent<HealthComponent>(); health_ && !health_->IsDead())
        health_->ApplyDamage(executionSlamDamage);
    const auto cameras_ = actor_->GetManager().GetCameraManager();
    const auto camera_ = cameras_ ? cameras_->GetSourceCamera() : nullptr;
    const auto cameraObject_ = camera_ ? camera_->GetGameObject() : nullptr;
    if (!cameraObject_) return;
    // シェイクはカメラActionより後段の既存Componentへ渡し、正面カットへ重ねる。
    cameraObject_->GetOrAddComponent<CameraShakeComponent>()->PlayShake(executionSlamShakeDuration,
        executionSlamShakeStrength, executionSlamShakeStrength * 12.0f, executionSlamShakeFrequency, 30);
}

void HumanBossActionComponent::ReleaseExecutionCapture()
{
    // 正常終了だけでなく、死亡・演出ロック・開始失敗でもPlayerを固定したまま残さない。
    if (const auto target_ = executionCapturedPlayer.lock())
        if (const auto player_ = target_->GetComponent<PlayerControllerComponent>()) player_->ReleaseEnemyExecution(GetGameObject());
    executionCapturedPlayer.reset();
    // 正常終了・死亡・中断で通常カメラへ戻す。別の優先演出へ切り替わっていた場合は維持する。
    if (const auto director_ = executionCameraDirector.lock()) director_->StopActionByName(activeExecutionCameraAction);
    executionCameraDirector.reset();
    activeExecutionCameraAction.clear();
    executionGrabFeedbackPlayed = false;
    executionSlamFeedbackPlayed = false;
}

void HumanBossActionComponent::CloseDefense()
{
    ReleaseExecutionCapture();
    if (rangedSkill == "SHADOW_CHARGE" || rangedSkill == "SHADOW_ORB" || rangedSkill == "SHADOW_RAGNAROK")
        rangedProgress = rangedSkill + (execution.GetResult() == SkillExecutionResult::COMPLETED ? ": completed" :
            ": stopped - " + (execution.GetError().empty() ? status : execution.GetError()));
    if (rangedSkill == "SHADOW_RAGNAROK")
        if (const auto owner_ = GetGameObject())
            if (const auto movement_ = owner_->GetComponent<CharacterMovementComponent>()) movement_->StopHorizontalMovementImmediately();
    ClearRangedArea();
    rangedSkill.clear();
    SetShadowInvulnerable(false);
    // 派生の判定と予約を閉じても、先に確定した周回弾は発射と寿命を継続する。
    if (shadowChain)
        if (const auto owner_ = GetGameObject()) owner_->GetManager().GetEffectManager().StopAttackEffects(owner_, shadowOrbs);
    SetShadowVisible(true);
    ClearShadowArea();
    shadowChain = false;
    shadowAreaTemplate.clear();
    shadowShiftActive = false;
    shadowWaypoints.clear();
    recoveryProtectionTimer = 0.0f;
    recoveryArmor = 0.0f;
    if (armorApplied)
        if (const auto reaction_ = protectedReaction.lock()) reaction_->SetArmor(previousReactionArmor);
    armorApplied = false;
    protectedReaction.reset();
    const auto owner_ = GetGameObject();
    attackAssistSkill = false;
    hasAttackTargetPosition = false;
    attackTarget.reset();
    windupFacingTracked = false;
    windupTargetLocked = false;
    windupRotationCorrection = 0.0f;
    windupWarpPresetName.clear();
    if (!owner_) return;
    if (const auto warp_ = owner_->GetComponent<MotionWarpComponent>()) warp_->Stop();
    if (currentDefense == HumanBossDefense::GUARD || currentDefense == HumanBossDefense::JUST_GUARD)
    {
        if (const auto guard_ = owner_->GetComponent<GuardCollider>())
        {
            guard_->SetJustGuardEnabled(false);
            guard_->Deactivate();
        }
    }
    if (currentDefense == HumanBossDefense::DODGE)
        if (const auto hurt_ = owner_->GetComponent<HurtBox>()) hurt_->SetInvincible(false);
    if (const auto movement_ = owner_->GetComponent<CharacterMovementComponent>()) movement_->StopHorizontalMovement();
    if (attackActive) SetWeaponTrails(false, false);
    currentDefense = HumanBossDefense::NONE;
    windupTracking = false;
    attackActive = false;
    if (const auto blackboard_ = owner_->GetComponent<BlackboardComponent>()) blackboard_->SetBool("HumanBossAI", "TelegraphActive", false);
    SyncComboState();
}

void HumanBossActionComponent::SyncComboState()
{
    const auto owner_ = GetGameObject();
    const auto blackboard_ = owner_ ? owner_->GetComponent<BlackboardComponent>() : nullptr;
    if (!blackboard_) return;
    const auto phase_ = execution.GetCurrentPhase();
    int pattern_ = 0;
    int stage_ = 0;
    constexpr std::size_t PREFIX_LENGTH = 13;
    if (attackActive && phase_ && phase_->animationState.compare(0, PREFIX_LENGTH, "Combo_Attack_") == 0)
    {
        const auto& name_ = phase_->animationState;
        // XXをパターン、YYを段数として読み、末尾のSeqなどの表記には依存しない。
        if (name_.size() >= PREFIX_LENGTH + 5 && name_[PREFIX_LENGTH + 2] == '_')
        {
            bool digits_ = true;
            for (const std::size_t offset_ : { 0u, 1u, 3u, 4u })
                digits_ = digits_ && name_[PREFIX_LENGTH + offset_] >= '0' && name_[PREFIX_LENGTH + offset_] <= '9';
            if (digits_)
            {
                pattern_ = (name_[PREFIX_LENGTH] - '0') * 10 + name_[PREFIX_LENGTH + 1] - '0';
                stage_ = (name_[PREFIX_LENGTH + 3] - '0') * 10 + name_[PREFIX_LENGTH + 4] - '0';
            }
        }
    }
    blackboard_->SetBool("HumanBossAI", "IsAttacking", attackActive && execution.IsActive());
    blackboard_->SetBool("HumanBossAI", "IsComboActive", pattern_ > 0);
    blackboard_->SetInt("HumanBossAI", "CurrentComboPattern", pattern_);
    blackboard_->SetInt("HumanBossAI", "CurrentComboIndex", stage_);
}

void HumanBossActionComponent::Stop(const std::string& idleState_)
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::Stop");
    const bool wasActive_ = execution.IsActive();
    execution.Stop();
    CloseDefense();
    // 死亡・演出ロックで停止した攻撃のAnimation Eventが、その後も発火しないよう閉じる。
    const auto owner_ = GetGameObject();
    const auto animation_ = owner_ ? AnimationGraphBridgeComponent::Acquire(owner_) : nullptr;
    if (wasActive_ && animation_ && !animation_->IsSkillOwned() && animation_->GetStateCatalog().count(idleState_))
        animation_->RequestStateTransition(idleState_, 0.1f);
    StopAttackHitBoxes();
    justGuardSucceeded = false;
    guardSucceeded = false;
}

void HumanBossActionComponent::StopAttackHitBoxes(bool clearTrail_)
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::StopAttackHitBoxes");
    // Idleへの遷移が成立しない場合も、旧攻撃の未発生予兆を残さない。
    if (const auto animation_ = AnimationGraphBridgeComponent::Acquire(GetGameObject()))
        animation_->RequestCancelAttackTelegraphs();
    ragnarokParticleSweeps.clear();
    if (const auto actor_ = GetGameObject())
        if (const auto warp_ = actor_->GetComponent<MotionWarpComponent>()) warp_->Stop();
    const auto owner_ = GetGameObject();
    SetWeaponTrails(false, clearTrail_);
    if (!owner_ || !owner_->manager) return;
    const auto health_ = owner_->GetComponent<HealthComponent>();
    bool locked_ = false;
    if (const auto mode_ = owner_->GetManager().GetGameModeManager())
        mode_->GetGlobalBlackboard().GetBool(GameModeBlackboardKeys::CATEGORY_GAME, GameModeBlackboardKeys::ENEMY_AI_LOCKED, locked_);
    // 通常の行動変更だけを保証し、死亡や戦闘演出への移行では従来どおり弾も閉じる。
    const bool preserveOrbs_ = !locked_ && !owner_->IsPendingDestroy() && owner_->IsCombatReady() && (!health_ || !health_->IsDead());
    const auto preservedEffects_ = preserveOrbs_ ? shadowOrbs : std::vector<std::weak_ptr<GameObject>>{};
    owner_->GetManager().GetEffectManager().StopAttackEffects(owner_, preservedEffects_);
    // 子の武器と独立Runtime HitBoxも、攻撃Ownerの参照で対象を選ぶ。
    for (const auto& object_ : owner_->GetManager().GetObjects())
    {
        if (!object_) continue;
        // 弾本体を残す場合は、その子HitBoxも停止せず、見た目だけ飛び続ける状態を防ぐ。
        bool preserved_ = false;
        auto transform_ = object_->transform;
        while (transform_ && !preserved_)
        {
            preserved_ = std::any_of(preservedEffects_.begin(), preservedEffects_.end(),
                [&transform_](const std::weak_ptr<GameObject>& effect_)
                {
                    const auto effectObject_ = effect_.lock();
                    return effectObject_ && effectObject_->transform == transform_;
                });
            transform_ = transform_->parent.lock();
        }
        if (preserved_) continue;
        for (const auto& hitBox_ : object_->GetComponents<HitBox>())
            if (hitBox_->GetAttackOwner() == owner_) hitBox_->Deactivate();
    }
}

void HumanBossActionComponent::SetWeaponTrails(bool emitting_, bool clearExisting_)
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::SetWeaponTrails");
    // 名前でScene全体を探さず、ボス自身と子階層の武器だけを操作する。
    const std::function<void(const std::shared_ptr<GameObject>&)> apply_ =
        [&](const std::shared_ptr<GameObject>& object_)
    {
        if (!object_) return;
        for (const auto& trail_ : object_->GetComponents<WeaponTrailComponent>())
        {
            if (!trail_) continue;
            if (emitting_) trail_->BeginTrail(clearExisting_);
            else trail_->EndTrail(clearExisting_);
        }
        if (!object_->transform) return;
        for (const auto& child_ : object_->transform->children)
            if (child_) apply_(child_->gameObject.lock());
    };
    apply_(GetGameObject());
}

bool HumanBossActionComponent::ConsumeJustGuardSuccess()
{
    const bool succeeded_ = justGuardSucceeded;
    justGuardSucceeded = false;
    return succeeded_;
}

void HumanBossActionComponent::FaceTarget(const DirectX::XMFLOAT3& direction_, bool immediate_)
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::FaceTarget");
    // 攻撃中の旋回はWarpの時間・角度制限へ一本化し、Sequenceからの即時旋回を防ぐ。
    if (attackAssistSkill && execution.IsActive())
    {
        const auto phase_ = execution.GetCurrentPhase();
        const auto owner_ = GetGameObject();
        const auto target_ = attackTarget.lock();
        const auto warp_ = owner_ ? owner_->GetComponent<MotionWarpComponent>() : nullptr;
        const auto preset_ = warp_ ? warp_->GetPreset(windupWarpPresetName) : nullptr;
        if (!attackAssistEnabled || !phase_ || phase_->animationState != "HumanBossAttackTell" ||
            !owner_ || !owner_->transform || !target_ || !target_->transform || !preset_ || !preset_->warpRotation ||
            preset_->rotationMode != MOTION_WARP_ROTATION_MODE::FACE_TARGET || windupWarpPresetName.empty()) return;
        const auto animation_ = AnimationGraphBridgeComponent::Acquire(owner_);
        const float duration_ = animation_ ? animation_->GetPlaybackDuration() : 0.0f;
        const float normalized_ = duration_ > 0.0001f ? animation_->GetPlaybackTime() / duration_ : 0.0f;
        if (normalized_ < preset_->rotationStartNormalizedTime || normalized_ > preset_->rotationEndNormalizedTime) return;
        float weight_ = 1.0f;
        if (normalized_ > preset_->rotationFullStrengthEndNormalizedTime)
            weight_ = std::clamp((preset_->rotationEndNormalizedTime - normalized_) /
                (std::max)(preset_->rotationEndNormalizedTime - preset_->rotationFullStrengthEndNormalizedTime, 0.0001f), 0.0f, 1.0f);
        windupFacingTracked = true;
        if (preset_->lockTargetOnStart || normalized_ >= preset_->lockTargetNormalizedTime) windupTargetLocked = true;
        const auto candidate_ = target_->transform->GetWorldPosition();
        const float travelX_ = candidate_.x - attackInitialTargetPosition.x;
        const float travelZ_ = candidate_.z - attackInitialTargetPosition.z;
        const float initialYaw_ = std::atan2(attackInitialTargetPosition.x - attackStartPosition.x, attackInitialTargetPosition.z - attackStartPosition.z);
        const float candidateYaw_ = std::atan2(candidate_.x - attackStartPosition.x, candidate_.z - attackStartPosition.z);
        const float travelAngle_ = DirectX::XMConvertToDegrees(std::atan2(std::sin(candidateYaw_ - initialYaw_), std::cos(candidateYaw_ - initialYaw_)));
        if (std::sqrt(travelX_ * travelX_ + travelZ_ * travelZ_) > preset_->maximumTargetTravelDistance ||
            std::abs(travelAngle_) > preset_->maximumTargetTravelAngleDegrees) windupTargetLocked = true;
        if (!windupTargetLocked) attackTargetPosition = candidate_;
        const auto position_ = owner_->transform->GetWorldPosition();
        const auto forward_ = owner_->transform->GetFront();
        const float yaw_ = std::atan2(forward_.x, forward_.z);
        const float targetYaw_ = std::atan2(attackTargetPosition.x - position_.x, attackTargetPosition.z - position_.z);
        const float difference_ = DirectX::XMConvertToDegrees(std::atan2(std::sin(targetYaw_ - yaw_), std::cos(targetYaw_ - yaw_)));
        const float limit_ = (std::min)((std::max)(0.0f, preset_->maximumRotationDegrees - windupRotationCorrection),
            preset_->maximumRotationDegreesPerSecond * preset_->rotationStrength * weight_ * assistFrameDeltaTime);
        const float applied_ = std::clamp(difference_, -limit_, limit_);
        const float nextYaw_ = yaw_ + DirectX::XMConvertToRadians(applied_);
        // 最終Targetへ瞬時に向けず、このフレームで許された角度だけをMovementへ予約する。
        if (const auto movement_ = owner_->GetComponent<CharacterMovementComponent>())
        {
            movement_->SetFacingDirection({ std::sin(nextYaw_), 0.0f, std::cos(nextYaw_) }, true);
            windupRotationCorrection += std::abs(applied_);
        }
        return;
    }
    if (const auto owner_ = GetGameObject())
        if (const auto movement_ = owner_->GetComponent<CharacterMovementComponent>())
            movement_->SetFacingDirection(direction_, immediate_);
}

void HumanBossActionComponent::SetLocomotion(const std::string& state_,
    const DirectX::XMFLOAT3& velocity_, const DirectX::XMFLOAT3& facing_, const DirectX::XMFLOAT2& blendInput_)
{
    const auto owner_ = GetGameObject();
    if (!owner_ || IsActive()) return;
    if (const auto movement_ = owner_->GetComponent<CharacterMovementComponent>())
    {
        movement_->SetHorizontalVelocity(velocity_);
        movement_->SetFacingDirection(facing_, false);
    }
    if (const auto blackboard_ = owner_->GetComponent<BlackboardComponent>())
    {
        blackboard_->SetVector2(AnimationGraphBlackboardKeys::ANIMATION_CATEGORY, AnimationGraphBlackboardKeys::GUARD_MOVE_INPUT, { blendInput_.x, blendInput_.y });
        blackboard_->SetVector2(AnimationGraphBlackboardKeys::ANIMATION_CATEGORY, AnimationGraphBlackboardKeys::MOVE_INPUT, { blendInput_.x, blendInput_.y });
    }
    if (const auto animation_ = AnimationGraphBridgeComponent::Acquire(owner_))
    {
        if (animation_->IsSkillOwned()) return;
        const auto current_ = animation_->GetActiveState();
        if ((!current_ || current_->name != state_) && animation_->GetStateCatalog().count(state_))
            animation_->RequestStateTransition(state_, 0.15f);
    }
}

void HumanBossActionComponent::DrawShadowTrajectoryPreview()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::DrawShadowTrajectoryPreview");
    const char* MODE_NAMES[] = { "BACK", "SIDE", "BEHIND", "APPROACH" };
    ImGui::Combo("Trajectory Preview Mode", &shadowPreviewMode, MODE_NAMES, IM_ARRAYSIZE(MODE_NAMES));
    ImGui::BeginDisabled(execution.IsActive());
    if (ImGui::Button("Preview Shadow Trajectory"))
    {
        const auto owner_ = GetGameObject();
        const auto observation_ = owner_ ? owner_->GetManager().GetCombatObservationSystem() : nullptr;
        const auto target_ = observation_ ? observation_->FindPlayerObject(owner_) : nullptr;
        std::vector<DirectX::XMFLOAT3> waypoints_;
        if (!BuildShadowPath(static_cast<HumanBossShadowShift>(shadowPreviewMode), target_, 1.0f, waypoints_) ||
            !PlanShadowTrajectory(static_cast<HumanBossShadowShift>(shadowPreviewMode), waypoints_))
        { shadowPathValidated = false; status = "Trajectory preview unavailable: target or safe path missing."; }
    }
    ImGui::EndDisabled();
    if (shadowTrajectory.empty()) return;
    const auto owner_ = GetGameObject();
    const auto current_ = owner_ && owner_->transform ? owner_->transform->GetWorldPosition() : shadowTrajectory.front();
    float minX_ = current_.x, maxX_ = current_.x, minZ_ = current_.z, maxZ_ = current_.z;
    for (const auto& point_ : shadowTrajectory)
    { minX_ = (std::min)(minX_, point_.x); maxX_ = (std::max)(maxX_, point_.x); minZ_ = (std::min)(minZ_, point_.z); maxZ_ = (std::max)(maxZ_, point_.z); }
    for (const auto& point_ : shadowBaseWaypoints)
    { minX_ = (std::min)(minX_, point_.x); maxX_ = (std::max)(maxX_, point_.x); minZ_ = (std::min)(minZ_, point_.z); maxZ_ = (std::max)(maxZ_, point_.z); }
    const ImVec2 origin_ = ImGui::GetCursorScreenPos();
    const ImVec2 size_{ (std::max)(180.0f, ImGui::GetContentRegionAvail().x), 190.0f };
    const float scale_ = (std::min)((size_.x - 24.0f) / (std::max)(1.0f, maxX_ - minX_), (size_.y - 24.0f) / (std::max)(1.0f, maxZ_ - minZ_));
    const auto project_ = [&](const DirectX::XMFLOAT3& point_)
    { return ImVec2{ origin_.x + 12.0f + (point_.x - minX_) * scale_, origin_.y + size_.y - 12.0f - (point_.z - minZ_) * scale_ }; };
    auto draw_ = ImGui::GetWindowDrawList();
    draw_->AddRectFilled(origin_, { origin_.x + size_.x, origin_.y + size_.y }, IM_COL32(22, 24, 29, 255));
    for (std::size_t index_ = 1; index_ < shadowBaseWaypoints.size(); ++index_)
        draw_->AddLine(project_(shadowBaseWaypoints[index_ - 1]), project_(shadowBaseWaypoints[index_]), IM_COL32(135, 135, 135, 255));
    const auto color_ = shadowPathValidated && !shadowPathBlocked ? IM_COL32(120, 220, 165, 255) : IM_COL32(240, 100, 100, 255);
    for (std::size_t index_ = 1; index_ < shadowTrajectory.size(); ++index_)
        draw_->AddLine(project_(shadowTrajectory[index_ - 1]), project_(shadowTrajectory[index_]), color_, 2.0f);
    draw_->AddCircleFilled(project_(shadowTrajectory.front()), 4.0f, IM_COL32(100, 170, 255, 255));
    draw_->AddCircleFilled(project_(shadowTrajectory.back()), 4.0f, IM_COL32(210, 130, 255, 255));
    draw_->AddCircleFilled(project_(current_), 3.0f, IM_COL32(255, 225, 90, 255));
    ImGui::Dummy(size_);
    ImGui::Text("XZ: grey=base, green=safe, red=blocked, yellow=current");
    ImGui::Text("Length %.2f / Move time %.2f / Width %.2f", shadowTrajectoryDistances.empty() ? 0.0f : shadowTrajectoryDistances.back(), shadowPlannedDuration, shadowActualWidth);
}

void HumanBossActionComponent::Gui()
{
    ImGui::DragFloat("ShadowApproachDistance", &shadowApproachDistance, 0.01f, 0.1f, 32.0f);
    ImGui::DragFloat("ShadowBackDistance", &shadowBackDistance, 0.01f, 0.1f, 24.0f);
    ImGui::DragFloat("ShadowSideDistance", &shadowSideDistance, 0.01f, 0.1f, 24.0f);
    ImGui::DragFloat("ShadowMaximumPathDistance", &shadowMaximumPathDistance, 0.01f, 1.0f, 40.0f);
    ImGui::DragFloat("ShadowReappearDuration", &shadowReappearDuration, 0.01f, 0.05f, 1.0f);
    ImGui::DragFloat("ShadowEffectInterval", &shadowEffectInterval, 0.01f, 0.05f, 0.3f);
    ImGui::DragFloat("ShadowEffectSpacing", &shadowEffectSpacing, 0.01f, 0.5f, 3.0f);
    ImGui::DragFloat("ShadowMoveSpeed", &shadowMoveSpeed, 0.01f, 1.0f, 60.0f);
    ImGui::DragFloat("ShadowMoveStartTime", &shadowMoveStartTime, 0.01f, 0.0f, 0.5f);
    ImGui::DragFloat("ShadowMoveEndTime", &shadowMoveEndTime, 0.01f, 0.1f, 4.0f);
    if (ImGui::TreeNode("Shadow Trajectory"))
    {
        ImGui::Checkbox("Enable Sway", &shadowSwayEnabled);
        ImGui::DragFloat("Maximum Sway Width", &shadowSwayWidth, 0.01f, 0.0f, 2.0f);
        const char* MODE_NAMES[] = { "BACK Strength", "SIDE Strength", "BEHIND Strength", "APPROACH Strength" };
        for (int index_ = 0; index_ < 4; ++index_) ImGui::DragFloat(MODE_NAMES[index_], &shadowSwayStrength[index_], 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Sway Cycles", &shadowSwayCycles, 0.01f, 0.5f, 2.5f);
        ImGui::DragFloat("Sway Randomness", &shadowSwayRandomness, 0.01f, 0.0f, 0.5f);
        ImGui::DragFloat("Sway Side Bias", &shadowSwayBias, 0.01f, -1.0f, 1.0f);
        ImGui::DragFloat("Endpoint Convergence", &shadowConvergence, 0.01f, 1.5f, 4.0f);
        ImGui::InputText("Sway Strength Curve", &shadowSwayCurve);
        ImGui::InputText("Movement Speed Curve", &shadowSpeedCurve);
        DrawShadowTrajectoryPreview();
        ImGui::TreePop();
    }
    ImGui::Checkbox("Hide Models During Shadow Shift", &shadowHideModels);
    ImGui::InputText("Shadow Danger Material", &shadowDangerMaterial);
    shadowMoveEndTime = (std::max)(shadowMoveStartTime + 0.05f, shadowMoveEndTime);
    ImGui::DragFloat("Shadow Landing Track Time", &shadowLandingTrackTime, 0.01f, 0.0f, 0.5f);
    ImGui::Checkbox("Attack Assist Enabled", &attackAssistEnabled);
    ImGui::Text("Planned RootMotion: %.3f / Required Warp: %.3f", debugPlannedDistance, debugRequiredCorrection);
    ImGui::DragFloat("Flinch Armor", &flinchArmor, 0.5f, 0.0f, 400.0f);
    ImGui::DragFloat("Guard Execution Reach", &guardExecutionReach, 0.05f, 0.8f, 4.0f);
    guardExecutionReach = std::clamp(guardExecutionReach, 0.8f, 4.0f);
    if (ImGui::TreeNode("Execution Presentation"))
    {
        ImGui::InputText("Camera Action", &executionCameraAction);
        ImGui::DragFloat("Grab Slow Scale", &executionSlowScale, 0.01f, 0.05f, 1.0f);
        ImGui::DragFloat("Grab Slow Hold", &executionSlowHoldDuration, 0.01f, 0.0f, 1.0f);
        ImGui::SliderFloat("Grab Rumble Strength", &executionGrabRumbleStrength, 0.0f, 1.0f);
        ImGui::DragFloat("Slam Shake Strength", &executionSlamShakeStrength, 0.005f, 0.0f, 0.5f);
        ImGui::DragFloat("Slam Damage", &executionSlamDamage, 1.0f, 0.0f, 99999.0f);
        executionSlamDamage = std::clamp(executionSlamDamage, 0.0f, 99999.0f);
        ImGui::DragFloat("Slam Shake Duration", &executionSlamShakeDuration, 0.01f, 0.05f, 1.0f);
        ImGui::DragFloat("Slam Shake Frequency", &executionSlamShakeFrequency, 0.5f, 1.0f, 60.0f);
        executionSlowScale = std::clamp(executionSlowScale, 0.05f, 1.0f);
        executionSlowHoldDuration = std::clamp(executionSlowHoldDuration, 0.0f, 1.0f);
        executionGrabRumbleStrength = std::clamp(executionGrabRumbleStrength, 0.0f, 1.0f);
        executionSlamShakeStrength = std::clamp(executionSlamShakeStrength, 0.0f, 0.5f);
        executionSlamShakeDuration = std::clamp(executionSlamShakeDuration, 0.05f, 1.0f);
        executionSlamShakeFrequency = std::clamp(executionSlamShakeFrequency, 1.0f, 60.0f);
        ImGui::TreePop();
    }
    ImGui::Text("Flinch protection: %s / Recovery: %.2f sec", armorApplied ? "ON" : "OFF", recoveryProtectionTimer);
    ImGui::Text("Skill: %s / Phase: %d", execution.GetSkillName().c_str(), execution.GetPhaseIndex());
    if (const auto phase_ = execution.GetCurrentPhase())
    {
        // 終了通知待ちと通常の硬直を区別できるよう、現在Phaseの進行を表示する。
        constexpr const char* END_NAMES[] = { "ANIMATION_END", "DURATION", "ACTION_END", "GROUND_CONTACT" };
        ImGui::Text("Phase: %s / End: %s", phase_->animationState.c_str(), END_NAMES[static_cast<int>(phase_->endCondition)]);
        ImGui::Text("Elapsed: %.2f / Duration: %.2f / Timeout: %.2f", execution.GetPhaseTime(), phase_->duration, phase_->maximumDuration);
        const auto owner_ = GetGameObject();
        if (const auto animation_ = owner_ ? AnimationGraphBridgeComponent::Acquire(owner_) : nullptr)
            ImGui::Text("Animation: %.2f / %.2f", animation_->GetPlaybackTime(), animation_->GetPlaybackDuration());
    }
    ImGui::Text("Result: %d", static_cast<int>(execution.GetResult()));
    ImGui::Text("Windup tracking: %s", windupTracking ? "ON" : "OFF");
    ImGui::Text("Shadow shift: %s / %.2f sec / distance %.2f / %s", shadowShiftActive ? "ON" : "OFF",
        shadowElapsed, shadowTravelDistance, shadowPathBlocked ? "BLOCKED" : shadowArrived ? "ARRIVED" : "MOVING");
    DrawRangedGui();
    ImGui::TextWrapped("%s", status.c_str());
    ImGui::TextWrapped("Validation: %s", validationError.c_str());
}

void HumanBossActionComponent::UpdateHitProtection(float deltaTime_)
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::UpdateHitProtection");
    recoveryProtectionTimer = (std::max)(0.0f, recoveryProtectionTimer - (std::max)(0.0f, deltaTime_));
    if (recoveryProtectionTimer <= 0.0f) recoveryArmor = 0.0f;
    const auto owner_ = GetGameObject();
    const auto animation_ = owner_ ? AnimationGraphBridgeComponent::Acquire(owner_) : nullptr;
    const auto phase_ = execution.GetCurrentPhase();
    const auto state_ = animation_ ? animation_->GetActiveState() : nullptr;
    // 特殊攻撃は移動・予兆も含めて保護し、Animation Eventの有無で中断耐性を失わない。
    const bool ragnarokActive_ = execution.IsActive() && rangedSkill == "SHADOW_RAGNAROK";
    const bool executionCounterActive_ = execution.IsActive() && !executionCapturedPlayer.expired();
    // 予備動作は攻撃イベントの有無に依存せず保護し、終了硬直には持ち越さない。
    bool windingUp_ = execution.IsActive() && phase_ &&
        (phase_->animationState == "HumanBossAttackTell" || windupTracking ||
            (rangedSkill == "SHADOW_CHARGE" && execution.GetPhaseIndex() == 0) ||
            (rangedSkill == "SHADOW_ORB" && phase_->animationState == "Buff_Seq") ||
            (shadowChain && (phase_->suspendGravity || phase_->animationState == "Buff_Seq")));
    bool striking_ = false;
    if (execution.IsActive() && phase_ && state_ && state_->name == phase_->animationState)
    {
        // 判定が閉じた後の終了硬直には耐性を持ち越さず、反撃できる隙を残す。
        float lastHitEnd_ = 0.0f;
        float firstHitStart_ = (std::numeric_limits<float>::max)();
        for (const auto& event_ : state_->events)
            if (event_.eventTemplateType == ANIMATION_EVENT_TEMPLATE_TYPE::ATTACK_TEMPLATE)
            {
                firstHitStart_ = (std::min)(firstHitStart_, event_.startTime);
                lastHitEnd_ = (std::max)(lastHitEnd_, event_.endTime);
            }
        windingUp_ = windingUp_ || (lastHitEnd_ > 0.0f && animation_->GetPlaybackTime() < firstHitStart_);
        striking_ = phase_->preventFlinch && lastHitEnd_ > 0.0f && animation_->GetPlaybackTime() < lastHitEnd_;
    }
    if (windingUp_ || striking_ || recoveryProtectionTimer > 0.0f || ragnarokActive_ || executionCounterActive_ || berserkProtection)
    {
        const auto reaction_ = owner_ ? owner_->GetComponent<HitReactionComponent>() : nullptr;
        if (!reaction_) return;
        if (!armorApplied)
        {
            previousReactionArmor = reaction_->GetArmor();
            protectedReaction = reaction_;
            armorApplied = true;
        }
        // ダメージは受け続け、復帰区間が終われば元の衝撃耐性へ戻す。
        // 特殊攻撃中は衝撃の強さにかかわらずひるみと吹き飛びを抑え、HP減少と死亡処理は維持する。
        const float armor_ = windingUp_ || ragnarokActive_ || executionCounterActive_ || berserkProtection ? (std::numeric_limits<float>::max)() : (std::max)(flinchArmor, recoveryArmor);
        reaction_->SetArmor((std::max)(previousReactionArmor, armor_));
    }
    else if (armorApplied)
    {
        if (const auto reaction_ = protectedReaction.lock()) reaction_->SetArmor(previousReactionArmor);
        protectedReaction.reset();
        armorApplied = false;
    }
}

void HumanBossActionComponent::ProtectRecovery(float duration_, float minimumArmor_)
{
    recoveryProtectionTimer = (std::max)(recoveryProtectionTimer, (std::max)(0.0f, duration_));
    recoveryArmor = (std::max)(recoveryArmor, (std::max)(0.0f, minimumArmor_));
    UpdateHitProtection(0.0f);
}

void HumanBossActionComponent::Serialize(nlohmann::json& json_) const
{
    json_["ExecutionHoldTime"] = executionHoldTime;
    json_["ExecutionTargetDistance"] = executionTargetDistance;
    json_["GuardExecutionReach"] = guardExecutionReach;
    json_["ExecutionCameraAction"] = executionCameraAction;
    json_["ExecutionSlowScale"] = executionSlowScale;
    json_["ExecutionSlowHoldDuration"] = executionSlowHoldDuration;
    json_["ExecutionGrabRumbleStrength"] = executionGrabRumbleStrength;
    json_["ExecutionSlamDamage"] = executionSlamDamage;
    json_["ExecutionSlamShakeStrength"] = executionSlamShakeStrength;
    json_["ExecutionSlamShakeDuration"] = executionSlamShakeDuration;
    json_["ExecutionSlamShakeFrequency"] = executionSlamShakeFrequency;
    SerializeRanged(json_);
    json_["FlinchArmor"] = flinchArmor;
    json_["ShadowApproachDistance"] = shadowApproachDistance;
    json_["ShadowBackDistance"] = shadowBackDistance;
    json_["ShadowSideDistance"] = shadowSideDistance;
    json_["ShadowSwayEnabled"] = shadowSwayEnabled;
    json_["ShadowSwayWidth"] = shadowSwayWidth;
    json_["ShadowSwayStrength"] = shadowSwayStrength;
    json_["ShadowSwayCycles"] = shadowSwayCycles;
    json_["ShadowSwayRandomness"] = shadowSwayRandomness;
    json_["ShadowSwayBias"] = shadowSwayBias;
    json_["ShadowConvergence"] = shadowConvergence;
    json_["ShadowSwayCurve"] = shadowSwayCurve;
    json_["ShadowSpeedCurve"] = shadowSpeedCurve;
    json_["ShadowMaximumPathDistance"] = shadowMaximumPathDistance;
    json_["ShadowReappearDuration"] = shadowReappearDuration;
    json_["ShadowEffectInterval"] = shadowEffectInterval;
    json_["ShadowEffectSpacing"] = shadowEffectSpacing;
    json_["ShadowMoveSpeed"] = shadowMoveSpeed;
    json_["ShadowMoveStartTime"] = shadowMoveStartTime;
    json_["ShadowMoveEndTime"] = shadowMoveEndTime;
    json_["ShadowHideModels"] = shadowHideModels;
    json_["ShadowDangerMaterial"] = shadowDangerMaterial;
    json_["ShadowLandingTrackTime"] = shadowLandingTrackTime;
    json_["AttackAssistEnabled"] = attackAssistEnabled;
}

void HumanBossActionComponent::Deserialize(const nlohmann::json& json_)
{
    // 保存時の設定だけを復元し、実行中の耐性と元のArmorは持ち越さない。
    Stop();
    // 設定の再読み込みでは旧実行中の弾を残さず、追跡参照を消す前に判定を閉じる。
    if (const auto owner_ = GetGameObject(); owner_ && owner_->manager)
        owner_->GetManager().GetEffectManager().StopAttackEffects(owner_);
    orbSpawnCommitted = false;
    ragnarokUsed = false;
    executionHoldTime = std::clamp(json_.value("ExecutionHoldTime", 1.92f), 0.1f, 10.0f);
    executionTargetDistance = std::clamp(json_.value("ExecutionTargetDistance", 1.8f), 0.8f, 4.0f);
    guardExecutionReach = std::clamp(json_.value("GuardExecutionReach", 2.8f), 0.8f, 4.0f);
    // 演出項目を持たない旧Sceneも初期値で復元し、実行中の参照と消費済みフラグは保存しない。
    executionCameraAction = json_.value("ExecutionCameraAction", std::string("EnemyExecution"));
    executionSlowScale = std::clamp(json_.value("ExecutionSlowScale", 0.25f), 0.05f, 1.0f);
    executionSlowHoldDuration = std::clamp(json_.value("ExecutionSlowHoldDuration", 0.2f), 0.0f, 1.0f);
    executionGrabRumbleStrength = std::clamp(json_.value("ExecutionGrabRumbleStrength", 0.7f), 0.0f, 1.0f);
    executionSlamDamage = std::clamp(json_.value("ExecutionSlamDamage", 50.0f), 0.0f, 99999.0f);
    executionSlamShakeStrength = std::clamp(json_.value("ExecutionSlamShakeStrength", 0.12f), 0.0f, 0.5f);
    executionSlamShakeDuration = std::clamp(json_.value("ExecutionSlamShakeDuration", 0.24f), 0.05f, 1.0f);
    executionSlamShakeFrequency = std::clamp(json_.value("ExecutionSlamShakeFrequency", 28.0f), 1.0f, 60.0f);
    ragnarokRound = 0;
    shadowOrbs.clear();
    DeserializeRanged(json_);
    flinchArmor = std::clamp(json_.value("FlinchArmor", 10.0f), 0.0f, 400.0f);
    shadowApproachDistance = std::clamp(json_.value("ShadowApproachDistance", shadowApproachDistance), 0.1f, 32.0f);
    shadowBackDistance = std::clamp(json_.value("ShadowBackDistance", shadowBackDistance), 0.1f, 24.0f);
    shadowSideDistance = std::clamp(json_.value("ShadowSideDistance", shadowSideDistance), 0.1f, 24.0f);
    shadowSwayEnabled = json_.value("ShadowSwayEnabled", shadowSwayEnabled);
    shadowSwayWidth = std::clamp(json_.value("ShadowSwayWidth", shadowSwayWidth), 0.0f, 2.0f);
    shadowSwayStrength = json_.value("ShadowSwayStrength", shadowSwayStrength);
    for (auto& strength_ : shadowSwayStrength) strength_ = std::clamp(strength_, 0.0f, 1.0f);
    shadowSwayCycles = std::clamp(json_.value("ShadowSwayCycles", shadowSwayCycles), 0.5f, 2.5f);
    shadowSwayRandomness = std::clamp(json_.value("ShadowSwayRandomness", shadowSwayRandomness), 0.0f, 0.5f);
    shadowSwayBias = std::clamp(json_.value("ShadowSwayBias", shadowSwayBias), -1.0f, 1.0f);
    shadowConvergence = std::clamp(json_.value("ShadowConvergence", shadowConvergence), 1.5f, 4.0f);
    shadowSwayCurve = json_.value("ShadowSwayCurve", shadowSwayCurve);
    shadowSpeedCurve = json_.value("ShadowSpeedCurve", shadowSpeedCurve);
    shadowMaximumPathDistance = std::clamp(json_.value("ShadowMaximumPathDistance", shadowMaximumPathDistance), 1.0f, 40.0f);
    shadowReappearDuration = std::clamp(json_.value("ShadowReappearDuration", shadowReappearDuration), 0.05f, 1.0f);
    shadowEffectInterval = std::clamp(json_.value("ShadowEffectInterval", shadowEffectInterval), 0.05f, 0.3f);
    shadowEffectSpacing = std::clamp(json_.value("ShadowEffectSpacing", shadowEffectSpacing), 0.5f, 3.0f);
    shadowMoveSpeed = std::clamp(json_.value("ShadowMoveSpeed", shadowMoveSpeed), 1.0f, 60.0f);
    shadowMoveStartTime = std::clamp(json_.value("ShadowMoveStartTime", shadowMoveStartTime), 0.0f, 0.5f);
    shadowMoveEndTime = std::clamp(json_.value("ShadowMoveEndTime", shadowMoveEndTime), 0.1f, 4.0f);
    shadowMoveEndTime = (std::max)(shadowMoveStartTime + 0.05f, shadowMoveEndTime);
    shadowHideModels = json_.value("ShadowHideModels", shadowHideModels);
    shadowDangerMaterial = json_.value("ShadowDangerMaterial", shadowDangerMaterial);
    shadowLandingTrackTime = std::clamp(json_.value("ShadowLandingTrackTime", shadowLandingTrackTime), 0.0f, 0.5f);
    attackAssistEnabled = json_.value("AttackAssistEnabled", true);
}

bool HumanBossActionComponent::ConsumeGuardSuccess()
{
    const bool succeeded_ = guardSucceeded;
    guardSucceeded = false;
    return succeeded_;
}

bool HumanBossActionComponent::ExtendComboDelay(float extension_, float maximum_)
{
    const auto phase_ = execution.GetCurrentPhase();
    if (!phase_ || execution.GetPhaseIndex() <= 0 || phase_->animationState != "SS_Guard") return false;
    return execution.ExtendCurrentDuration(extension_, maximum_);
}
