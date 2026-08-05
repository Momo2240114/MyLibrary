#include "PrecompiledHeader.h"

#include "GuardCollider.h"

#include "HitBox.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"

#include <algorithm>
#include <imgui.h>

GuardCollider::GuardCollider()
{
    // GuardColliderも通常時は無効にしておき、Guard中やJustGuard受付中だけActionから有効化する。
    // Guard専用Roleを固定し、Combat判定以外の物理反応を持たないColliderとして扱う。
    enabled = false;
    EnforceGuardSettings();
    debugPrimitiveColor = { 0.2f, 0.8f, 1.0f, 1.0f };
}

const std::string& GuardCollider::GetName() const
{
    static std::string name_ = "GuardCollider";
    return name_;
}

void GuardCollider::Start()
{
    // CapsuleColliderとして初期化し、ActionSequenceからGuard受付を開閉できるActionを登録する。
    // Load直後に受付が開きっぱなしにならないよう、最後にDeactivateして状態を揃える。
    CapsuleCollider::Start();
    RegisterActions();
    Deactivate();
}

void GuardCollider::Activate()
{
    // Guard受付開始時は、前回の攻撃履歴とTrigger状態を消してから有効化する。
    // これにより同じObjectを再利用しても、古い接触状態で即Guard成立しないようにする。
    ClearGuardedAttackHistory();
    currentTriggers.clear();
    SetEnabled(true);
}

void GuardCollider::Deactivate()
{
    // Guard受付を閉じる時はJustGuard受付も必ず閉じる。
    // Trigger状態と履歴も破棄し、次回のGuard開始を新しい受付窓として扱う。
    justGuardEnabled = false;
    SetEnabled(false);
    currentTriggers.clear();
    ClearGuardedAttackHistory();
}

bool GuardCollider::CanGuardAttack(const HitBox& hitBox_) const
{
    // GuardSystemへ渡す前に、所有者攻撃、Team、同一activationの重複Guardを除外する。
    // 実際に通常Guard/JustGuardのどちらになるかは、justGuardEnabledを見てGuardSystem側で決まる。
    const std::shared_ptr<GameObject> owner_ = GetGuardOwner();
    const std::shared_ptr<GameObject> attackOwner_ =
        const_cast<HitBox&>(hitBox_).GetAttackOwner();

    if (!canGuardOwnerAttack &&
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

    const auto iterator_ = guardedAttackActivationIds.find(&hitBox_);
    if (oncePerAttackActivation &&
        iterator_ != guardedAttackActivationIds.end() &&
        iterator_->second == hitBox_.GetActivationId())
    {
        return false;
    }

    return true;
}

void GuardCollider::RecordGuardedAttack(const HitBox& hitBox_)
{
    // 成立済みのHitBox activationを記録し、同じ攻撃判定が何度もGuard成立しないようにする。
    guardedAttackActivationIds[&hitBox_] = hitBox_.GetActivationId();
}

void GuardCollider::ClearGuardedAttackHistory()
{
    // Guard受付を開き直す時やDeserialize時に、過去のGuard履歴をリセットする。
    guardedAttackActivationIds.clear();
}

void GuardCollider::SetGuardOwner(GameObject* guardOwner_)
{
    // 子ObjectにColliderを置く場合でも、明示的に防御主体を差し替えられるようにする。
    guardOwner = guardOwner_;
}

std::shared_ptr<GameObject> GuardCollider::GetGuardOwner() const
{
    // 明示Ownerがあればそれを使い、なければ親階層からBlackboard/Guard Actionを持つObjectを探す。
    if (guardOwner != nullptr)
    {
        return guardOwner->shared_from_this();
    }

    return ResolveGuardOwnerFromHierarchy();
}

void GuardCollider::SetJustGuardEnabled(bool enabled_)
{
    // JustGuard受付はGuardCollider自体が有効な時だけONにできる。
    // Collider無効時にJustGuardだけ残る状態を防ぐ。
    justGuardEnabled = enabled_ && IsEnabled();
}

void GuardCollider::Gui()
{
    // Guard受付状態、JustGuard受付状態、Team、Action名をEditor上で確認・調整する。
    CapsuleCollider::Gui();

    ImGui::SeparatorText("Guard Collider");
    ImGui::TextColored(
        enabled ? ImVec4(0.2f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.3f, 0.2f, 1.0f),
        enabled ? "GuardCollider: ACTIVE" : "GuardCollider: INACTIVE");
    ImGui::Text("Just Guard: %s", justGuardEnabled ? "ON" : "OFF");

    if (ImGui::Button("Activate GuardCollider")) Activate();
    ImGui::SameLine();
    if (ImGui::Button("Deactivate GuardCollider")) Deactivate();
    if (ImGui::Button("Enable JustGuard")) SetJustGuardEnabled(true);
    ImGui::SameLine();
    if (ImGui::Button("Disable JustGuard")) SetJustGuardEnabled(false);

    ImGui::Checkbox("Can Guard Owner Attack", &canGuardOwnerAttack);
    ImGui::Checkbox("Once Per Attack Activation", &oncePerAttackActivation);
    ImGui::DragInt("Team ID", &teamId, 1, 0, 999);

    ImGui::SeparatorText("Actions");
    char actionPrefixBuffer_[64];
    strncpy_s(actionPrefixBuffer_, actionPrefix.c_str(), sizeof(actionPrefixBuffer_));
    if (ImGui::InputText("Action Prefix", actionPrefixBuffer_, sizeof(actionPrefixBuffer_)))
    {
        actionPrefix = actionPrefixBuffer_;
    }

    ImGui::Text("Activate          : %s", GetActivateActionName().c_str());
    ImGui::Text("Deactivate        : %s", GetDeactivateActionName().c_str());
    ImGui::Text("Enable JustGuard  : %s", GetEnableJustGuardActionName().c_str());
    ImGui::Text("Disable JustGuard : %s", GetDisableJustGuardActionName().c_str());

    if (ImGui::Button("Register Guard Actions"))
    {
        RegisterActions();
    }
}

void GuardCollider::Serialize(nlohmann::json& json_) const
{
    // Capsule形状に加えて、Guard受付Action名と反応条件を保存する。
    // 現在受付中かどうかやGuard履歴はFrame中の状態なので保存しない。
    CapsuleCollider::Serialize(json_);

    json_["ActionPrefix"] = actionPrefix;
    json_["ActivateActionName"] = activateActionName;
    json_["DeactivateActionName"] = deactivateActionName;
    json_["EnableJustGuardActionName"] = enableJustGuardActionName;
    json_["DisableJustGuardActionName"] = disableJustGuardActionName;
    json_["CanGuardOwnerAttack"] = canGuardOwnerAttack;
    json_["OncePerAttackActivation"] = oncePerAttackActivation;
    json_["TeamId"] = teamId;
}

void GuardCollider::Deserialize(const nlohmann::json& json_)
{
    // 設定だけを復元し、Runtime状態のJustGuard受付や重複履歴は初期化する。
    // CollisionRoleなどは保存値に左右されないよう最後に固定設定を再適用する。
    CapsuleCollider::Deserialize(json_);

    actionPrefix = json_.value("ActionPrefix", std::string{});
    activateActionName = json_.value(
        "ActivateActionName",
        std::string("EnableGuardCollider"));
    deactivateActionName = json_.value(
        "DeactivateActionName",
        std::string("DisableGuardCollider"));
    enableJustGuardActionName = json_.value(
        "EnableJustGuardActionName",
        std::string("EnableJustGuard"));
    disableJustGuardActionName = json_.value(
        "DisableJustGuardActionName",
        std::string("DisableJustGuard"));
    canGuardOwnerAttack = json_.value("CanGuardOwnerAttack", false);
    oncePerAttackActivation = json_.value("OncePerAttackActivation", true);
    teamId = json_.value("TeamId", 0);

    justGuardEnabled = false;
    ClearGuardedAttackHistory();
    EnforceGuardSettings();
}

std::string GuardCollider::GetActionPrefix() const
{
    // Prefix未指定ならOwner名を使い、複数ObjectのGuard Action名衝突を避ける。
    if (!actionPrefix.empty())
    {
        return actionPrefix;
    }

    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (owner_)
    {
        return owner_->GetName();
    }

    return "Guard";
}

std::string GuardCollider::GetActivateActionName() const
{
    // Prefix付きのAction名を作り、ActionSequenceから個別のGuard受付を開けるようにする。
    const std::string prefix_ = GetActionPrefix();
    if (prefix_.empty()) return activateActionName;
    return prefix_ + "_" + activateActionName;
}

std::string GuardCollider::GetDeactivateActionName() const
{
    // Guard受付終了用Action名も同じPrefix規則で生成する。
    const std::string prefix_ = GetActionPrefix();
    if (prefix_.empty()) return deactivateActionName;
    return prefix_ + "_" + deactivateActionName;
}

std::string GuardCollider::GetEnableJustGuardActionName() const
{
    // JustGuard受付開始は通常Guard受付とは別Actionにし、短い受付窓を作れるようにする。
    const std::string prefix_ = GetActionPrefix();
    if (prefix_.empty()) return enableJustGuardActionName;
    return prefix_ + "_" + enableJustGuardActionName;
}

std::string GuardCollider::GetDisableJustGuardActionName() const
{
    // JustGuard受付終了Action名。GuardCollider自体は開いたまま通常Guardへ戻す用途に使う。
    const std::string prefix_ = GetActionPrefix();
    if (prefix_.empty()) return disableJustGuardActionName;
    return prefix_ + "_" + disableJustGuardActionName;
}

void GuardCollider::EnforceGuardSettings()
{
    // Guard判定は物理的に押し返さず、HitBoxとのTrigger重なりだけを見る。
    collisionRole = CollisionRole::GUARD_BOX;
    usePhysicsResponse = false;
    physicsControlsTransform = false;
    useGravity = false;
    isTrigger = true;
}

void GuardCollider::RegisterActions()
{
    // colliderComponents内から自分のshared_ptrを探し、Action lambdaへweak_ptrで渡す。
    // Guard受付、受付終了、JustGuard開始/終了をActionSequenceから制御できるようにする。
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    std::weak_ptr<GuardCollider> weakCollider_;
    for (const std::shared_ptr<Component>& component_ : owner_->colliderComponents)
    {
        const std::shared_ptr<GuardCollider> collider_ =
            std::dynamic_pointer_cast<GuardCollider>(component_);
        if (collider_.get() == this)
        {
            weakCollider_ = collider_;
            break;
        }
    }

    owner_->RegisterAction("Combat", GetActivateActionName(), [weakCollider_]()
    {
        if (const std::shared_ptr<GuardCollider> collider_ = weakCollider_.lock())
        {
            collider_->Activate();
        }
    });
    owner_->RegisterAction("Combat", GetDeactivateActionName(), [weakCollider_]()
    {
        if (const std::shared_ptr<GuardCollider> collider_ = weakCollider_.lock())
        {
            collider_->Deactivate();
        }
    });
    owner_->RegisterAction("Combat", GetEnableJustGuardActionName(), [weakCollider_]()
    {
        if (const std::shared_ptr<GuardCollider> collider_ = weakCollider_.lock())
        {
            collider_->SetJustGuardEnabled(true);
        }
    });
    owner_->RegisterAction("Combat", GetDisableJustGuardActionName(), [weakCollider_]()
    {
        if (const std::shared_ptr<GuardCollider> collider_ = weakCollider_.lock())
        {
            collider_->SetJustGuardEnabled(false);
        }
    });
}

std::shared_ptr<GameObject> GuardCollider::ResolveGuardOwnerFromHierarchy() const
{
    // Colliderが手や盾など子Objectに付いている場合でも、親のBlackboardやGuard Actionへ結果を返したい。
    // そのため親方向へたどり、Guardを受け取れそうなObjectを防御主体として返す。
    std::shared_ptr<GameObject> object_ = gameObject.lock();
    std::shared_ptr<GameObject> rootObject_ = object_;

    while (object_)
    {
        const std::string guardActionName_ = object_->BuildActionName("OnGuardSucceeded");
        const std::string justGuardActionName_ = object_->BuildActionName("OnJustGuardSucceeded");
        if (object_->GetComponent<BlackboardComponent>() ||
            object_->localRegistry.HasAction(guardActionName_) ||
            object_->localRegistry.HasAction(justGuardActionName_) ||
            object_->localRegistry.HasAction("OnGuardSucceeded") ||
            object_->localRegistry.HasAction("OnJustGuardSucceeded"))
        {
            // BlackboardかGuard用Actionを持つObjectを、Guard結果の受け取り先として優先する。
            return object_;
        }

        if (!object_->transform) break;

        const std::shared_ptr<Transform> parentTransform_ =
            object_->transform->parent.lock();
        if (!parentTransform_) break;

        const std::shared_ptr<GameObject> parentObject_ =
            parentTransform_->gameObject.lock();
        if (!parentObject_) break;

        rootObject_ = parentObject_;
        object_ = parentObject_;
    }

    return rootObject_;
}
