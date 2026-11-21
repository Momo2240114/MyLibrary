#include "PrecompiledHeader.h"

#include "JustDodgeCollider.h"

#include "HitBox.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"

#include <algorithm>
#include <imgui.h>

JustDodgeCollider::JustDodgeCollider()
{
    // JustDodgeColliderは常時有効ではなく、Dodge中の受付窓だけActionから有効化する。
    // 生成時は無効にしておき、誤って通常状態でJustDodge判定が出ないようにする。
    enabled = false;
    EnforceJustDodgeSettings();
    debugPrimitiveColor = { 0.2f, 0.6f, 1.0f, 1.0f };
}

const std::string& JustDodgeCollider::GetName() const
{
    static std::string name_ = "JustDodgeCollider";
    return name_;
}

void JustDodgeCollider::Start()
{
    // CapsuleColliderとしてPhysics/Debug設定を初期化したあと、ActionRegistryへ開閉Actionを登録する。
    // 最後にDeactivateして、Load直後の状態を必ず「受付なし」に揃える。
    CapsuleCollider::Start();
    RegisterActions();
    Deactivate();
}

void JustDodgeCollider::Activate()
{
    // 受付窓開始時は、前回の重複判定履歴とTrigger中情報を消してから有効化する。
    // これにより、古い攻撃との接触状態が残っていても新しい受付窓として判定できる。
    ClearDodgedAttackHistory();
    currentTriggers.clear();
    SetEnabled(true);
}

void JustDodgeCollider::Deactivate()
{
    // 受付窓終了時はColliderを無効化し、Trigger状態と成功履歴を破棄する。
    // 次のDodgeでは必ず新しい攻撃受付として始める。
    SetEnabled(false);
    currentTriggers.clear();
    ClearDodgedAttackHistory();
}

bool JustDodgeCollider::CanReactToAttack(const HitBox& hitBox_) const
{
    // JustDodgeSystemへ成功候補として渡す前に、所有者攻撃、Team、同一activationを除外する。
    // ここでは「このColliderがこのHitBoxへ反応してよいか」だけを判断する。
    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    const std::shared_ptr<GameObject> attackOwner_ =
        const_cast<HitBox&>(hitBox_).GetAttackOwner();

    if (!canReactToOwnerAttack &&
        owner_ &&
        attackOwner_ &&
        owner_.get() == attackOwner_.get())
    {
        return false;
    }

    if (teamId != 0 && hitBox_.teamId != 0 && teamId == hitBox_.teamId)
    {
        return false;
    }

    const auto iterator_ = dodgedAttackActivationIds.find(&hitBox_);
    if (oncePerAttackActivation &&
        iterator_ != dodgedAttackActivationIds.end() &&
        iterator_->second == hitBox_.GetActivationId())
    {
        return false;
    }

    return true;
}

void JustDodgeCollider::RecordDodgedAttack(const HitBox& hitBox_)
{
    // 成功済みのHitBox activationを記録し、同じ攻撃判定から複数回JustDodge通知が出ないようにする。
    dodgedAttackActivationIds[&hitBox_] = hitBox_.GetActivationId();
}

void JustDodgeCollider::ClearDodgedAttackHistory()
{
    // 受付窓を開き直す時やLoad復元時に、過去の成功履歴をリセットする。
    dodgedAttackActivationIds.clear();
}

void JustDodgeCollider::Gui()
{
    // 受付状態、Team、重複反応設定、Action名をEditor上で確認・調整する。
    CapsuleCollider::Gui();

    ImGui::SeparatorText("Just Dodge Collider");

    ImGui::TextColored(
        enabled ? ImVec4(0.2f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.3f, 0.2f, 1.0f),
        enabled ? "JustDodgeCollider: ACTIVE" : "JustDodgeCollider: INACTIVE");

    if (ImGui::Button("Activate JustDodgeCollider"))
    {
        Activate();
    }
    ImGui::SameLine();
    if (ImGui::Button("Deactivate JustDodgeCollider"))
    {
        Deactivate();
    }

    ImGui::Checkbox("Can React To Owner Attack", &canReactToOwnerAttack);
    ImGui::Checkbox("Once Per Attack Activation", &oncePerAttackActivation);
    ImGui::DragInt("Team ID", &teamId, 1, 0, 999);

    ImGui::SeparatorText("Actions");

    char actionPrefixBuffer_[64];
    strncpy_s(actionPrefixBuffer_, actionPrefix.c_str(), sizeof(actionPrefixBuffer_));
    if (ImGui::InputText("Action Prefix", actionPrefixBuffer_, sizeof(actionPrefixBuffer_)))
    {
        actionPrefix = actionPrefixBuffer_;
    }

    ImGui::Text("Activate   : %s", GetActivateActionName().c_str());
    ImGui::Text("Deactivate : %s", GetDeactivateActionName().c_str());

    if (ImGui::Button("Register Just Dodge Actions"))
    {
        RegisterActions();
    }
}

void JustDodgeCollider::Serialize(nlohmann::json& json_) const
{
    // Capsule形状に加えて、Action名と反応条件を保存する。
    // 現在有効かどうかや成功履歴はFrame中の状態なので保存しない。
    CapsuleCollider::Serialize(json_);

    json_["ActionPrefix"] = actionPrefix;
    json_["ActivateActionName"] = activateActionName;
    json_["DeactivateActionName"] = deactivateActionName;
    json_["CanReactToOwnerAttack"] = canReactToOwnerAttack;
    json_["OncePerAttackActivation"] = oncePerAttackActivation;
    json_["TeamId"] = teamId;
}

void JustDodgeCollider::Deserialize(const nlohmann::json& json_)
{
    // 設定だけを復元し、Runtime履歴は初期化する。
    // CollisionRoleなどは保存値に左右されないよう最後に固定設定を再適用する。
    CapsuleCollider::Deserialize(json_);

    actionPrefix = json_.value("ActionPrefix", std::string("JustDodge"));
    activateActionName = json_.value(
        "ActivateActionName",
        std::string("EnableJustDodgeCollider"));
    deactivateActionName = json_.value(
        "DeactivateActionName",
        std::string("DisableJustDodgeCollider"));

    canReactToOwnerAttack = json_.value("CanReactToOwnerAttack", false);
    oncePerAttackActivation = json_.value("OncePerAttackActivation", true);
    teamId = json_.value("TeamId", 0);

    ClearDodgedAttackHistory();
    EnforceJustDodgeSettings();
}

std::string JustDodgeCollider::GetActionPrefix() const
{
    // Prefix未指定ならOwner名を使い、複数ObjectのAction名衝突を避ける。
    if (!actionPrefix.empty())
    {
        return actionPrefix;
    }

    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (owner_)
    {
        return owner_->GetName();
    }

    return "JustDodge";
}

std::string JustDodgeCollider::GetActivateActionName() const
{
    // Object名/Prefix付きのAction名を作り、ActionSequenceから個別Colliderを開けるようにする。
    const std::string prefix_ = GetActionPrefix();
    if (prefix_.empty())
    {
        return activateActionName;
    }
    return prefix_ + "_" + activateActionName;
}

std::string JustDodgeCollider::GetDeactivateActionName() const
{
    // 有効化と同じ命名規則で、受付終了用Action名を作る。
    const std::string prefix_ = GetActionPrefix();
    if (prefix_.empty())
    {
        return deactivateActionName;
    }
    return prefix_ + "_" + deactivateActionName;
}

void JustDodgeCollider::EnforceJustDodgeSettings()
{
    // JustDodge判定は物理的に押し返さず、HitBoxとのTrigger重なりだけを見る。
    collisionRole = CollisionRole::JUST_DODGE_BOX;
    usePhysicsResponse = false;
    physicsControlsTransform = false;
    useGravity = false;
    isTrigger = true;
}

void JustDodgeCollider::RegisterActions()
{
    // colliderComponents内から自分のshared_ptrを探し、Action lambdaへweak_ptrで渡す。
    // これによりActionSequenceからEnable/Disableを呼んでも、削除済みColliderを触らない。
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }

    std::weak_ptr<JustDodgeCollider> weakCollider_;
    for (const std::shared_ptr<Component>& component_ : owner_->colliderComponents)
    {
        const std::shared_ptr<JustDodgeCollider> collider_ =
            std::dynamic_pointer_cast<JustDodgeCollider>(component_);

        if (collider_.get() == this)
        {
            weakCollider_ = collider_;
            break;
        }
    }

    owner_->RegisterAction("Combat", GetActivateActionName(), [weakCollider_]()
        {
            if (const std::shared_ptr<JustDodgeCollider> collider_ = weakCollider_.lock())
            {
                collider_->Activate();
            }
        });

    owner_->RegisterAction("Combat", GetDeactivateActionName(), [weakCollider_]()
        {
            if (const std::shared_ptr<JustDodgeCollider> collider_ = weakCollider_.lock())
            {
                collider_->Deactivate();
            }
        });
}
