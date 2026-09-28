#include "PrecompiledHeader.h"

#include "JustGuardSystem.h"

#include "Engine/Common/DebugLog.h"
#include "Engine/GamePlay/Camera/CameraActionDirectorComponent.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Camera/CameraManager.h"
#include "Engine/GamePlay/Combat/JustGuardReactionComponent.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/System/EffectManager.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Physics/GuardCollider.h"
#include "Engine/Physics/HitBox.h"

#include <string>
#include <utility>

namespace
{
    void PlayJustGuardCameraAction(const JustGuardEvent& event_)
    {
        // ジャストガード成立位置を実カメラのCameraActionへ渡し、演出側の設定で再生する。
        const std::shared_ptr<GameObject> defender_ = event_.defender.lock();
        if (!defender_ || !defender_->manager) return;

        CameraManager* cameraManager_ = defender_->manager->GetCameraManager();
        if (!cameraManager_) return;

        const std::shared_ptr<CameraComponent> camera_ =
            cameraManager_->GetSourceCamera();
        if (!camera_) return;

        const std::shared_ptr<GameObject> cameraObject_ =
            camera_->GetGameObject();
        if (!cameraObject_) return;

        const std::shared_ptr<CameraActionDirectorComponent> cameraAction_ =
            cameraObject_->GetComponent<CameraActionDirectorComponent>();
        if (!cameraAction_) return;

        DirectX::XMFLOAT3 focusPosition_ = event_.guardPosition;
        if (!event_.hasContact && defender_->transform)
        {
            focusPosition_ = defender_->transform->GetWorldPosition();
        }

        cameraAction_->PlayActionAtPosition(
            "JustGuard",
            focusPosition_,
            event_.attackNormal);
    }

    void PlayJustGuardImpactEffect(const JustGuardEvent& event_)
    {
        // ジャストガード成功位置に専用ImpactEffectを出し、通常Hitと別の成功演出にする。
        const std::shared_ptr<GameObject> defender_ = event_.defender.lock();
        if (!defender_ || !defender_->manager) return;

        DirectX::XMFLOAT3 position_ = event_.guardPosition;
        if (!event_.hasContact && defender_->transform)
        {
            position_ = defender_->transform->GetWorldPosition();
        }

        defender_->manager->GetEffectManager().SpawnImpactEffectAsset(
            "JustGuardImpact",
            position_,
            event_.attackNormal);
    }
}

void JustGuardSystem::BeginFrame()
{
    frameJustGuardEvents.clear();
}

void JustGuardSystem::Clear()
{
    frameJustGuardEvents.clear();
    registeredGuardTargets.clear();
}

void JustGuardSystem::ClearAttackInstance(uint32_t attackInstanceId_)
{
    if (attackInstanceId_ == 0u) return;

    // 攻撃IDが終わったら、その攻撃で成立済みのジャストガード履歴をまとめて消す。
    for (auto iterator_ = registeredGuardTargets.begin();
        iterator_ != registeredGuardTargets.end();)
    {
        if (static_cast<uint32_t>(iterator_->first >> 32u) == attackInstanceId_)
        {
            iterator_ = registeredGuardTargets.erase(iterator_);
        }
        else
        {
            ++iterator_;
        }
    }
}

void JustGuardSystem::ClearAttackHitGroup(
    uint32_t attackInstanceId_,
    uint32_t hitGroupId_)
{
    if (attackInstanceId_ == 0u) return;

    const uint64_t guardKey_ =
        (static_cast<uint64_t>(attackInstanceId_) << 32u) |
        static_cast<uint64_t>(hitGroupId_);

    registeredGuardTargets.erase(guardKey_);
}

bool JustGuardSystem::HandleJustGuard(
    HitBox& hitBox_,
    GuardCollider& guardCollider_)
{
    JustGuardContact contact_{};
    return HandleJustGuard(hitBox_, guardCollider_, contact_);
}

bool JustGuardSystem::HandleJustGuard(
    HitBox& hitBox_,
    GuardCollider& guardCollider_,
    const JustGuardContact& contact_)
{
    BS_PROFILE_SCOPE("JustGuardSystem::HandleJustGuard");

    const std::shared_ptr<GameObject> attacker_ = hitBox_.GetAttackOwner();
    const std::shared_ptr<GameObject> defender_ = guardCollider_.GetGuardOwner();

    BS_LOG_INFO(
        LOG_CATEGORY::GAME,
        "[JustGuard] HandleJustGuard entered. attackInstance="
        + std::to_string(hitBox_.attackInstanceId)
        + " hitGroup="
        + std::to_string(hitBox_.hitGroupId));

    if (!attacker_ || !defender_)
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[JustGuard] Rejected: attacker or defender missing.");
        return false;
    }
    if (!hitBox_.IsEnabled() || !guardCollider_.IsEnabled())
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[JustGuard] Rejected: HitBox or GuardCollider disabled.");
        return false;
    }
    if (!guardCollider_.IsJustGuardEnabled())
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[JustGuard] Rejected: JustGuard window is closed.");
        return false;
    }
    if (IsSameHierarchy(attacker_, defender_))
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[JustGuard] Rejected: same hierarchy.");
        return false;
    }
    if (!guardCollider_.CanGuardAttack(hitBox_))
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[JustGuard] Rejected: collider cannot guard this attack.");
        return false;
    }
    if (!RegisterJustGuard(
        hitBox_.attackInstanceId,
        hitBox_.hitGroupId,
        defender_.get(),
        false))
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[JustGuard] Rejected: already registered for this attack.");
        return false;
    }

    guardCollider_.RecordGuardedAttack(hitBox_);

    JustGuardEvent event_{};
    event_.defender = defender_;
    event_.attacker = attacker_;
    event_.guardCollider = &guardCollider_;
    event_.hitBox = &hitBox_;
    event_.guardPosition = contact_.position;
    event_.attackNormal = contact_.normal;
    event_.hasContact = contact_.isValid;
    event_.attackInstanceId = hitBox_.attackInstanceId;
    event_.hitGroupId = hitBox_.hitGroupId;
    event_.teamId = hitBox_.teamId;
    event_.sourceAction = hitBox_.sourceAction;
    event_.attackTag = hitBox_.attackTag;

    frameJustGuardEvents.push_back(event_);
    BS_LOG_SUCCESS(LOG_CATEGORY::GAME, "[JustGuard] Event accepted. Publishing blackboard/action.");
    PublishJustGuardToBlackboard(event_);
    PublishJustGuardToAttacker(event_);
    PlayJustGuardImpactEffect(event_);
    PlayJustGuardCameraAction(event_);

    for (const JustGuardCallback& callback_ : justGuardCallbacks)
    {
        if (callback_) callback_(event_);
    }

    return true;
}

void JustGuardSystem::AddJustGuardCallback(JustGuardCallback callback_)
{
    if (callback_) justGuardCallbacks.push_back(std::move(callback_));
}

bool JustGuardSystem::RegisterJustGuard(
    uint32_t attackInstanceId_,
    uint32_t hitGroupId_,
    const GameObject* defender_,
    bool allowMultiGuard_)
{
    if (allowMultiGuard_ || attackInstanceId_ == 0u || defender_ == nullptr)
    {
        return true;
    }

    const uint64_t guardKey_ =
        (static_cast<uint64_t>(attackInstanceId_) << 32u) |
        static_cast<uint64_t>(hitGroupId_);

    // 同じ攻撃判定で同じ防御者へ何度も成功通知しないよう履歴を持つ。
    return registeredGuardTargets[guardKey_].insert(defender_).second;
}

bool JustGuardSystem::IsSameHierarchy(
    const std::shared_ptr<GameObject>& objectA_,
    const std::shared_ptr<GameObject>& objectB_) const
{
    return GetHierarchyRoot(objectA_) == GetHierarchyRoot(objectB_);
}

std::shared_ptr<GameObject> JustGuardSystem::GetHierarchyRoot(
    std::shared_ptr<GameObject> object_) const
{
    while (object_ && object_->transform)
    {
        const std::shared_ptr<Transform> parentTransform_ =
            object_->transform->parent.lock();
        if (!parentTransform_) break;

        const std::shared_ptr<GameObject> parentObject_ =
            parentTransform_->gameObject.lock();
        if (!parentObject_) break;

        object_ = parentObject_;
    }

    return object_;
}

void JustGuardSystem::PublishJustGuardToBlackboard(
    const JustGuardEvent& event_)
{
    BS_PROFILE_SCOPE("JustGuardSystem::PublishJustGuardToBlackboard");

    const std::shared_ptr<GameObject> defender_ = event_.defender.lock();
    const std::shared_ptr<GameObject> receiver_ = FindBlackboardReceiver(defender_);
    if (!receiver_) return;

    const std::shared_ptr<BlackboardComponent> blackboard_ =
        receiver_->GetOrAddComponent<BlackboardComponent>();
    if (!blackboard_) return;

    blackboard_->SetBool("Guard", "JustGuardSucceeded", true);
    blackboard_->SetTrigger("Guard", "JustGuardTrigger");
    blackboard_->SetBool("Guard", "CanCounter", true);
    blackboard_->SetString("Guard", "GuardedAttackTag", event_.attackTag);
    blackboard_->SetString("Guard", "GuardedSourceAction", event_.sourceAction);
    blackboard_->SetInt("Guard", "AttackInstanceId", static_cast<int>(event_.attackInstanceId));
    blackboard_->SetInt("Guard", "HitGroupId", static_cast<int>(event_.hitGroupId));
    blackboard_->SetVector3("Guard", "Position", event_.guardPosition);
    blackboard_->SetVector3("Guard", "Normal", event_.attackNormal);

    bool onJustGuardCalled_ = false;
    std::shared_ptr<GameObject> actionReceiver_ = defender_;
    while (actionReceiver_)
    {
        const std::string actionName_ = actionReceiver_->BuildActionName("OnJustGuard");
        if (actionReceiver_->localRegistry.HasAction(actionName_) ||
            actionReceiver_->localRegistry.HasAction("OnJustGuard"))
        {
            BS_LOG_SUCCESS(
                LOG_CATEGORY::GAME,
                "[JustGuard] Calling OnJustGuard on "
                + actionReceiver_->GetName());
            actionReceiver_->ExecuteAction("OnJustGuard");
            onJustGuardCalled_ = true;
            break;
        }

        if (!actionReceiver_->transform) break;

        const std::shared_ptr<Transform> parentTransform_ =
            actionReceiver_->transform->parent.lock();
        if (!parentTransform_) break;

        actionReceiver_ = parentTransform_->gameObject.lock();
    }

    if (!onJustGuardCalled_)
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[JustGuard] OnJustGuard action receiver not found.");
    }

    if (const std::shared_ptr<JustGuardReactionComponent> reaction_ =
        receiver_->GetComponent<JustGuardReactionComponent>())
    {
        reaction_->ExecuteReaction();
    }
}

void JustGuardSystem::PublishJustGuardToAttacker(const JustGuardEvent& event_)
{
    BS_PROFILE_SCOPE("JustGuardSystem::PublishJustGuardToAttacker");

    if (!event_.hitBox || event_.hitBox->justGuardedActionName.empty()) return;

    std::shared_ptr<GameObject> actionReceiver_ = event_.hitBox->GetGameObject();
    const std::shared_ptr<GameObject> attacker_ = event_.attacker.lock();
    const std::shared_ptr<GameObject> defender_ = event_.defender.lock();
    while (actionReceiver_)
    {
        const std::string resolvedActionName_ =
            actionReceiver_->BuildActionName(event_.hitBox->justGuardedActionName);
        if (actionReceiver_->GetLocalRegistry().HasAction(resolvedActionName_) ||
            actionReceiver_->GetLocalRegistry().HasAction(event_.hitBox->justGuardedActionName))
        {
            const std::shared_ptr<BlackboardComponent> blackboard_ =
                actionReceiver_->GetOrAddComponent<BlackboardComponent>();
            if (blackboard_)
            {
                blackboard_->SetBool("GuardReaction", "AttackGuarded", false);
                blackboard_->SetBool("GuardReaction", "AttackJustGuarded", true);
                blackboard_->SetTrigger("GuardReaction", "AttackJustGuardedTrigger");
                blackboard_->SetVector3("GuardReaction", "JustGuardPosition", event_.guardPosition);
                blackboard_->SetVector3("GuardReaction", "JustGuardNormal", event_.attackNormal);
                if (attacker_ && attacker_->transform)
                {
                    blackboard_->SetVector3(
                        "GuardReaction",
                        "ReflectTargetPosition",
                        attacker_->transform->GetWorldPosition());
                }
                if (defender_)
                {
                    blackboard_->SetString("GuardReaction", "ReflectOwnerName", defender_->GetName());
                }
            }

            // HitBoxを持つRuntime攻撃Objectへ通知し、Projectile反射などの攻撃別処理を実行する。
            actionReceiver_->ExecuteAction(event_.hitBox->justGuardedActionName);
            return;
        }

        if (!actionReceiver_->transform) break;
        const std::shared_ptr<Transform> parentTransform_ =
            actionReceiver_->transform->parent.lock();
        if (!parentTransform_) break;
        actionReceiver_ = parentTransform_->gameObject.lock();
    }
}

std::shared_ptr<GameObject> JustGuardSystem::FindBlackboardReceiver(
    std::shared_ptr<GameObject> object_) const
{
    if (!object_) return nullptr;

    // Colliderが子Objectに付いていても、親側のBlackboardを優先して結果を届ける。
    std::shared_ptr<GameObject> fallback_ = object_;
    while (object_)
    {
        if (object_->GetComponent<BlackboardComponent>())
        {
            return object_;
        }

        if (!object_->transform) break;

        const std::shared_ptr<Transform> parentTransform_ =
            object_->transform->parent.lock();
        if (!parentTransform_) break;

        const std::shared_ptr<GameObject> parentObject_ =
            parentTransform_->gameObject.lock();
        if (!parentObject_) break;

        fallback_ = parentObject_;
        object_ = parentObject_;
    }

    return fallback_;
}
