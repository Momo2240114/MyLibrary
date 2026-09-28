#include "PrecompiledHeader.h"

#include "HurtBox.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "CharacterCollider.h"
#include "HitBox.h"
#include "Engine/Debug/TracyProfiler.h"

#include <algorithm>

HurtBox::HurtBox()
{
    // HurtBoxは物理反応を持たない「攻撃を受けるためのTrigger」として固定する。
    // 色も他Colliderと区別しやすいよう、被弾判定らしい緑系にしている。
    EnforceHurtBoxSettings();
    debugPrimitiveColor = { 0.2f, 1.0f, 0.3f, 1.0f };
}

const std::string& HurtBox::GetName() const
{
    static std::string name = "HurtBox";
    return name;
}

void HurtBox::Start()
{
    // CapsuleColliderとして初期化した後、CharacterColliderがあれば形状同期し、
    // Animation/ActionSequenceから無敵ON/OFFを呼べるようActionを登録する。
    CapsuleCollider::Start();
    ResolveCharacterCollider();
    SyncCharacterColliderShape();
    RegisterActions();
}

void HurtBox::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("HurtBox::Update");
    // Player/Enemyの本体Colliderに追従する設定の場合、毎Frame形状を合わせる。
    // その後、被弾後の短い無敵時間を減らして次のHit受付へ戻す。
    ResolveCharacterCollider();
    SyncCharacterColliderShape();
    CapsuleCollider::Update(deltaTime_);
    invincibilityTimer = (std::max)(
        invincibilityTimer - (std::max)(deltaTime_, 0.0f),
        0.0f);
    grantedInvincibilityTimer = (std::max)(
        grantedInvincibilityTimer - (std::max)(deltaTime_, 0.0f), 0.0f);
}

bool HurtBox::CanReceiveHit(const HitBox& hitBox_) const
{
    // 明示無敵、同じHitBox activationの重複Hit、被弾直後の短い無敵をここでまとめて拒否する。
    // AttackSystem側はこの結果を見て、HitEventを作るかどうかを決める。
    if (IsInvincible()) return false;

    const auto receivedIterator_ = receivedActivationIds.find(&hitBox_);
    if (hitBox_.hitOncePerActivation &&
        receivedIterator_ != receivedActivationIds.end() &&
        receivedIterator_->second == hitBox_.GetActivationId())
    {
        return false;
    }
    return invincibilityTimer <= 0.0f;
}

void HurtBox::SetInvincible(bool invincible_)
{
    // ActionSequenceから完全無敵を切り替えるための入口。
    // 完全無敵に入る時は、残りTimerではなくinvincibleフラグを優先させる。
    invincible = invincible_;
    if (invincible) invincibilityTimer = 0.0f;
}

void HurtBox::AddInvincibilityTime(float duration_)
{
    // Animationの無敵解除や次の回避開始で、成功時の残り時間が失われないよう分離する。
    grantedInvincibilityTimer = (std::max)(grantedInvincibilityTimer, (std::max)(duration_, 0.0f));
}

void HurtBox::RecordHit(const HitBox& hitBox_)
{
    // Hit成立後に呼ばれ、同じHitBoxの同じactivationで再度当たらないよう履歴を残す。
    // あわせて短い無敵Timerを張り、多段判定が1Frameごとに入り続けるのを防ぐ。
    receivedActivationIds[&hitBox_] = hitBox_.GetActivationId();
    invincibilityTimer = invincibilityDuration;
}

void HurtBox::ResetHitHistory()
{
    // DebugやAction切り替え、Deserialize時に、過去のHit履歴とTimerを初期化する。
    receivedActivationIds.clear();
    invincibilityTimer = 0.0f;
    grantedInvincibilityTimer = 0.0f;
}

void HurtBox::Gui()
{
    // 被弾倍率、無敵時間、CharacterCollider同期の状態をEditor上で確認・調整する。
    CapsuleCollider::Gui();
    ImGui::SeparatorText("Damage Reception");
    ImGui::Text("Granted Invincibility: %.3f sec", grantedInvincibilityTimer);
    ImGui::Checkbox(
        "Sync Character Collider Shape",
        &syncCharacterColliderShape);
    ImGui::DragFloat("Damage Multiplier", &damageMultiplier, 0.05f, 0.0f, 10.0f);
    ImGui::DragFloat(
        "Invincibility Duration",
        &invincibilityDuration,
        0.01f,
        0.0f,
        10.0f,
        "%.3f sec");
    ImGui::ProgressBar(
        invincibilityDuration > 0.0f
            ? invincibilityTimer / invincibilityDuration
            : 0.0f,
        ImVec2(-1.0f, 0.0f),
        invincibilityTimer > 0.0f ? "INVINCIBLE" : "READY");
    if (ImGui::Button("Reset Hit History")) ResetHitHistory();
    char hurtTagBuffer_[64];
    strncpy_s(hurtTagBuffer_, hurtTag.c_str(), sizeof(hurtTagBuffer_));
    if (ImGui::InputText("Hurt Tag", hurtTagBuffer_, sizeof(hurtTagBuffer_))) hurtTag = hurtTagBuffer_;
}

void HurtBox::Serialize(nlohmann::json& j_) const
{
    // Capsuleの形状設定に加えて、被弾倍率や部位Tag、無敵時間をScene JSONへ保存する。
    CapsuleCollider::Serialize(j_);
    j_["DamageMultiplier"] = damageMultiplier;
    j_["HurtTag"] = hurtTag;
    j_["InvincibilityDuration"] = invincibilityDuration;
    j_["SyncCharacterColliderShape"] = syncCharacterColliderShape;
}

void HurtBox::Deserialize(const nlohmann::json& j_)
{
    // 保存値を復元した後、Runtime中のHit履歴は持ち越さず初期状態に戻す。
    CapsuleCollider::Deserialize(j_);
    damageMultiplier = j_.value("DamageMultiplier", 1.0f);
    hurtTag = j_.value("HurtTag", std::string("Body"));
    invincibilityDuration = (std::max)(
        j_.value("InvincibilityDuration", 0.08f),
        0.0f);
    syncCharacterColliderShape =
        j_.value("SyncCharacterColliderShape", true);
    ResetHitHistory();
    EnforceHurtBoxSettings();
}

void HurtBox::EnforceHurtBoxSettings()
{
    // HurtBoxは攻撃検知専用なので、Physics反応や重力は使わずTriggerとして扱う。
    collisionRole = CollisionRole::HURT_BOX;
    usePhysicsResponse = false;
    physicsControlsTransform = false;
    useGravity = false;
    isTrigger = true;
}

void HurtBox::RegisterActions()
{
    // 自分自身をweak_ptrとして捕まえ、ObjectのActionRegistryから無敵ON/OFFできるようにする。
    // raw thisを直接captureしないことで、Object削除後にActionが残っても安全に抜けられる。
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    std::weak_ptr<HurtBox> weakHurtBox_;
    for (const std::shared_ptr<Component>& component_ : owner_->colliderComponents)
    {
        const std::shared_ptr<HurtBox> hurtBox_ =
            std::dynamic_pointer_cast<HurtBox>(component_);
        if (hurtBox_.get() == this)
        {
            weakHurtBox_ = hurtBox_;
            break;
        }
    }

    owner_->RegisterAction("Combat", "HurtBoxInvincibleOn", [weakHurtBox_]()
    {
        if (const std::shared_ptr<HurtBox> hurtBox_ = weakHurtBox_.lock())
        {
            hurtBox_->SetInvincible(true);
        }
    });
    owner_->RegisterAction("Combat", "HurtBoxInvincibleOff", [weakHurtBox_]()
    {
        if (const std::shared_ptr<HurtBox> hurtBox_ = weakHurtBox_.lock())
        {
            hurtBox_->SetInvincible(false);
        }
    });
}

void HurtBox::ResolveCharacterCollider()
{
    // CharacterColliderが同じObjectにある場合、HurtBoxを本体Collider形状へ追従させる。
    // weak_ptrが生きている間は再探索せず、Component検索コストを抑える。
    if (!characterCollider.expired()) return;

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (owner_)
    {
        characterCollider = owner_->GetComponent<CharacterCollider>();
    }
}

void HurtBox::SyncCharacterColliderShape()
{
    // Character本体と同じCapsuleで被弾したい時に、半径、高さ、Node追従情報をコピーする。
    // 部位別HurtBoxなど独自形状にしたい場合はsyncCharacterColliderShapeをfalseにする。
    if (!syncCharacterColliderShape) return;

    const std::shared_ptr<CharacterCollider> characterCollider_ =
        characterCollider.lock();
    if (!characterCollider_) return;

    radius = characterCollider_->radius;
    halfHeight = characterCollider_->halfHeight;
    nodeModelObjectName = characterCollider_->nodeModelObjectName;
    startNodeName = characterCollider_->startNodeName;
    endNodeName = characterCollider_->endNodeName;
}
