#include "HitBox.h"

#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"

#include <algorithm>


HitBox::HitBox()
{
    enabled = false;
    EnforceHitBoxSettings();
    debugPrimitiveColor = { 1.0f, 0.2f, 0.2f, 1.0f };
}

const std::string& HitBox::GetName() const
{
    static std::string name = "HitBox";
    return name;
}

void HitBox::SetOwner(GameObject* owner_)
{
    ownerObject = owner_;
}

void HitBox::SetAttackInstanceId(uint32_t attackInstanceId_)
{
    attackInstanceId = attackInstanceId_;
    ClearHitTargets();
}

void HitBox::SetHitGroupId(uint32_t hitGroupId_)
{
    hitGroupId = hitGroupId_;
}

void HitBox::SetDamage(float damage_)
{
    damage = damage_;
}

void HitBox::SetKnockBackPower(float knockBackPower_)
{
    knockBackPower = knockBackPower_;
}

void HitBox::SetHitEffectPath(const std::string& path_)
{
    hitMeshVFXAssetPath = path_;
}

void HitBox::ClearHitTargets()
{
    alreadyHitTargets.clear();
}

bool HitBox::CanHit(GameObject* target_) const
{
    if (target_ == nullptr) return false;
    if (!canHitOwner && ownerObject == target_) return false;
    return allowMultiHit || alreadyHitTargets.count(target_) == 0u;
}

void HitBox::RegisterHit(GameObject* target_)
{
    if (target_ != nullptr) alreadyHitTargets.insert(target_);
}

std::shared_ptr<GameObject> HitBox::GetAttackOwner()
{
    if (ownerObject != nullptr)
    {
        return ownerObject->shared_from_this();
    }
    return GetGameObject();
}

void HitBox::Start()
{
    CapsuleCollider::Start();
    RegisterActions();
    Deactivate();
}

void HitBox::Activate()
{
    ApplyBlackboardValues();
    if (ownerObject == nullptr)
    {
        const std::shared_ptr<GameObject> object_ = GetGameObject();
        if (object_)
        {
            SetAttackInstanceId(
                object_->GetManager().GetAttackSystem().CreateAttackInstance());
        }
    }
    ClearHitTargets();
    ++activationId;
    if (activationId == 0) ++activationId;
    currentTriggers.clear();
    SetEnabled(true);
}

void HitBox::Deactivate()
{
    SetEnabled(false);
    currentTriggers.clear();
    ClearHitTargets();
}

void HitBox::Gui()
{
    CapsuleCollider::Gui();
    ImGui::SeparatorText("Attack");
    ImGui::DragFloat("Damage", &damage, 0.1f, 0.0f, 99999.0f);
    ImGui::DragFloat("Hit Stop Duration", &hitStopDuration, 0.001f, 0.0f, 10.0f, "%.3f s");
    ImGui::DragFloat("Hit Stop Scale", &hitStopScale, 0.01f, 0.0f, 1.0f, "%.2fx");
    ImGui::InputScalar("Hit Stop Layer Mask", ImGuiDataType_U32, &hitStopLayerMask);
    ImGui::InputScalar("Attack Instance ID", ImGuiDataType_U32, &attackInstanceId);
    ImGui::InputScalar("Hit Group ID", ImGuiDataType_U32, &hitGroupId);
    ImGui::Checkbox("Allow Multi Hit", &allowMultiHit);
    ImGui::Checkbox("Can Hit Owner", &canHitOwner);
    ImGui::Text("Hit Targets: %zu", alreadyHitTargets.size());
    ImGui::Text("Owner: %s", ownerObject ? ownerObject->GetName().c_str() : "(Self)");
    ImGui::DragFloat("Knock Back Power", &knockBackPower, 0.1f, 0.0f, 99999.0f);
    ImGui::Checkbox("Hit Once Per Activation", &hitOncePerActivation);
    ImGui::TextColored(
        enabled ? ImVec4(0.2f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.3f, 0.2f, 1.0f),
        enabled ? "HitBox: ACTIVE" : "HitBox: INACTIVE");
    if (ImGui::Button("Activate HitBox (Test)")) Activate();
    ImGui::SameLine();
    if (ImGui::Button("Deactivate HitBox (Test)")) Deactivate();
    char attackTagBuffer_[64];
    strncpy_s(attackTagBuffer_, attackTag.c_str(), sizeof(attackTagBuffer_));
    if (ImGui::InputText("Attack Tag", attackTagBuffer_, sizeof(attackTagBuffer_))) attackTag = attackTagBuffer_;

    ImGui::SeparatorText("Hit Particle");
    char particleAssetBuffer_[256];
    strncpy_s(particleAssetBuffer_, hitParticleAssetPath.c_str(), sizeof(particleAssetBuffer_));
    if (ImGui::InputText("Particle Asset", particleAssetBuffer_, sizeof(particleAssetBuffer_))) hitParticleAssetPath = particleAssetBuffer_;
    char particleEmitterBuffer_[128];
    strncpy_s(particleEmitterBuffer_, hitParticleEmitterName.c_str(), sizeof(particleEmitterBuffer_));
    if (ImGui::InputText("Particle Emitter", particleEmitterBuffer_, sizeof(particleEmitterBuffer_))) hitParticleEmitterName = particleEmitterBuffer_;
    ImGui::DragFloat("Particle Life Time", &hitParticleLifeTime, 0.01f, 0.0f, 30.0f);
    ImGui::DragFloat("Particle Normal Offset", &hitParticleNormalOffset, 0.01f, -10.0f, 10.0f);

    ImGui::SeparatorText("Hit MeshVFX");
    char meshVFXAssetBuffer_[256];
    strncpy_s(meshVFXAssetBuffer_, hitMeshVFXAssetPath.c_str(), sizeof(meshVFXAssetBuffer_));
    if (ImGui::InputText("MeshVFX Asset", meshVFXAssetBuffer_, sizeof(meshVFXAssetBuffer_))) hitMeshVFXAssetPath = meshVFXAssetBuffer_;
    ImGui::DragFloat("MeshVFX Life Time", &hitMeshVFXLifeTime, 0.01f, 0.0f, 30.0f);
    ImGui::DragFloat("MeshVFX Normal Offset", &hitMeshVFXNormalOffset, 0.01f, -10.0f, 10.0f);

    ImGui::SeparatorText("Hit Sound");
    char hitSoundPathBuffer_[256];
    strncpy_s(hitSoundPathBuffer_, hitSoundPath.c_str(), sizeof(hitSoundPathBuffer_));
    if (ImGui::InputText("Hit Sound Path", hitSoundPathBuffer_, sizeof(hitSoundPathBuffer_))) hitSoundPath = hitSoundPathBuffer_;
    ImGui::DragFloat("Hit Sound Volume", &hitSoundVolume, 0.01f, 0.0f, 1.0f);
    ImGui::Checkbox("Hit Sound Muffled", &hitSoundMuffled);
    ImGui::DragFloat("Hit Sound Cutoff", &hitSoundMuffledCutoff, 10.0f, 10.0f, 22000.0f);

    ImGui::SeparatorText("Actions");
    char actionPrefixBuffer_[64];
    strncpy_s(actionPrefixBuffer_, actionPrefix.c_str(), sizeof(actionPrefixBuffer_));
    if (ImGui::InputText("Action Prefix", actionPrefixBuffer_, sizeof(actionPrefixBuffer_)))
    {
        actionPrefix = actionPrefixBuffer_;
    }
    ImGui::Text("Empty Prefix uses GameObject name.");
    ImGui::Text("Action Type Activate   : %s", activateActionName.c_str());
    ImGui::Text("Action Type Deactivate : %s", deactivateActionName.c_str());
    ImGui::SeparatorText("Resolved Action Names");
    ImGui::Text("Activate   : %s", GetActivateActionName().c_str());
    ImGui::Text("Deactivate : %s", GetDeactivateActionName().c_str());
    if (ImGui::Button("Register HitBox Actions"))
    {
        RegisterActions();
    }

    ImGui::SeparatorText("Blackboard Input");
    char categoryBuffer_[64];
    strncpy_s(categoryBuffer_, blackboardCategory.c_str(), sizeof(categoryBuffer_));
    if (ImGui::InputText("Category", categoryBuffer_, sizeof(categoryBuffer_))) blackboardCategory = categoryBuffer_;
    char damageKeyBuffer_[64];
    strncpy_s(damageKeyBuffer_, damageKey.c_str(), sizeof(damageKeyBuffer_));
    if (ImGui::InputText("Damage Key", damageKeyBuffer_, sizeof(damageKeyBuffer_))) damageKey = damageKeyBuffer_;
    char hitStopKeyBuffer_[64];
    strncpy_s(hitStopKeyBuffer_, hitStopDurationKey.c_str(), sizeof(hitStopKeyBuffer_));
    if (ImGui::InputText("Hit Stop Duration Key", hitStopKeyBuffer_, sizeof(hitStopKeyBuffer_))) hitStopDurationKey = hitStopKeyBuffer_;
    char knockBackKeyBuffer_[64];
    strncpy_s(knockBackKeyBuffer_, knockBackPowerKey.c_str(), sizeof(knockBackKeyBuffer_));
    if (ImGui::InputText("Knock Back Key", knockBackKeyBuffer_, sizeof(knockBackKeyBuffer_))) knockBackPowerKey = knockBackKeyBuffer_;
    char hitSoundKeyBuffer_[64];
    strncpy_s(hitSoundKeyBuffer_, hitSoundPathKey.c_str(), sizeof(hitSoundKeyBuffer_));
    if (ImGui::InputText("Hit Sound Path Key", hitSoundKeyBuffer_, sizeof(hitSoundKeyBuffer_))) hitSoundPathKey = hitSoundKeyBuffer_;
}

void HitBox::Serialize(nlohmann::json& j_) const
{
    CapsuleCollider::Serialize(j_);
    j_["Damage"] = damage;
    j_["HitStopDuration"] = hitStopDuration;
    j_["HitStopScale"] = hitStopScale;
    j_["HitStopLayerMask"] = hitStopLayerMask;
    j_["AttackInstanceId"] = attackInstanceId;
    j_["HitGroupId"] = hitGroupId;
    j_["AllowMultiHit"] = allowMultiHit;
    j_["CanHitOwner"] = canHitOwner;
    j_["TeamId"] = teamId;
    j_["SourceAction"] = sourceAction;
    j_["KnockBackPower"] = knockBackPower;
    j_["HitOncePerActivation"] = hitOncePerActivation;
    j_["AttackTag"] = attackTag;
    j_["HitParticleAssetPath"] = hitParticleAssetPath;
    j_["HitParticleEmitterName"] = hitParticleEmitterName;
    j_["HitParticleLifeTime"] = hitParticleLifeTime;
    j_["HitParticleNormalOffset"] = hitParticleNormalOffset;
    j_["HitMeshVFXAssetPath"] = hitMeshVFXAssetPath;
    j_["HitMeshVFXLifeTime"] = hitMeshVFXLifeTime;
    j_["HitMeshVFXNormalOffset"] = hitMeshVFXNormalOffset;
    j_["HitSoundPath"] = hitSoundPath;
    j_["HitSoundVolume"] = hitSoundVolume;
    j_["HitSoundMuffled"] = hitSoundMuffled;
    j_["HitSoundMuffledCutoff"] = hitSoundMuffledCutoff;
    j_["ActionPrefix"] = actionPrefix;
    j_["ActivateActionName"] = activateActionName;
    j_["DeactivateActionName"] = deactivateActionName;
    j_["BlackboardCategory"] = blackboardCategory;
    j_["DamageKey"] = damageKey;
    j_["HitStopDurationKey"] = hitStopDurationKey;
    j_["KnockBackPowerKey"] = knockBackPowerKey;
    j_["HitSoundPathKey"] = hitSoundPathKey;
}

void HitBox::Deserialize(const nlohmann::json& j_)
{
    CapsuleCollider::Deserialize(j_);
    damage = j_.value("Damage", 1.0f);
    hitStopDuration = j_.value("HitStopDuration", j_.value("HitStopTime", 0.0f));
    hitStopScale = (std::clamp)(j_.value("HitStopScale", j_.value("HitStopDir", 0.0f)), 0.0f, 1.0f);
    hitStopLayerMask = j_.value("HitStopLayerMask", 3u);
    attackInstanceId = j_.value("AttackInstanceId", 0u);
    hitGroupId = j_.value("HitGroupId", 0u);
    allowMultiHit = j_.value("AllowMultiHit", false);
    canHitOwner = j_.value("CanHitOwner", false);
    teamId = j_.value("TeamId", 0);
    sourceAction = j_.value("SourceAction", std::string{});
    knockBackPower = j_.value("KnockBackPower", 0.0f);
    hitOncePerActivation = j_.value("HitOncePerActivation", true);
    attackTag = j_.value("AttackTag", std::string("Attack"));
    hitParticleAssetPath = j_.value("HitParticleAssetPath", std::string{});
    hitParticleEmitterName = j_.value("HitParticleEmitterName", std::string{});
    hitParticleLifeTime = (std::max)(j_.value("HitParticleLifeTime", 1.0f), 0.0f);
    hitParticleNormalOffset = j_.value("HitParticleNormalOffset", 0.0f);
    hitMeshVFXAssetPath = j_.value("HitMeshVFXAssetPath", std::string{});
    hitMeshVFXLifeTime = (std::max)(j_.value("HitMeshVFXLifeTime", 1.0f), 0.0f);
    hitMeshVFXNormalOffset = j_.value("HitMeshVFXNormalOffset", 0.0f);
    hitSoundPath = j_.value("HitSoundPath", std::string{});
    hitSoundVolume = (std::max)(j_.value("HitSoundVolume", 1.0f), 0.0f);
    hitSoundMuffled = j_.value("HitSoundMuffled", false);
    hitSoundMuffledCutoff = (std::max)(j_.value("HitSoundMuffledCutoff", 1200.0f), 10.0f);
    actionPrefix = j_.value("ActionPrefix", std::string("HitBox"));
    activateActionName = "EnableHitBox";
    deactivateActionName = "DisableHitBox";
    blackboardCategory = j_.value("BlackboardCategory", std::string("Attack"));
    damageKey = j_.value("DamageKey", std::string("Damage"));
    hitStopDurationKey = j_.value("HitStopDurationKey", std::string{});
    if (hitStopDurationKey.empty())
    {
        hitStopDurationKey = j_.value("HitStopTimeKey", std::string("HitStopDuration"));
        if (hitStopDurationKey == "HitStopTime") hitStopDurationKey = "HitStopDuration";
    }
    knockBackPowerKey = j_.value("KnockBackPowerKey", std::string("KnockBackPower"));
    hitSoundPathKey = j_.value("HitSoundPathKey", std::string("HitSoundPath"));
    EnforceHitBoxSettings();
}

std::string HitBox::GetActionPrefix() const
{
    if (!actionPrefix.empty())
    {
        return actionPrefix;
    }

    auto owner_ = gameObject;
    if (owner_.lock())
    {
        return owner_.lock()->GetName();
    }

    return "HitBox";
}

std::string HitBox::GetActivateActionName() const
{
    const std::string prefix_ = GetActionPrefix();

    if (prefix_.empty())
    {
        return activateActionName;
    }

    return prefix_ + "_" + activateActionName;
}

std::string HitBox::GetDeactivateActionName() const
{
    const std::string prefix_ = GetActionPrefix();

    if (prefix_.empty())
    {
        return deactivateActionName;
    }

    return prefix_ + "_" + deactivateActionName;
}

void HitBox::ApplyBlackboardValues()
{
    std::shared_ptr<GameObject> owner_ = GetGameObject();
    std::shared_ptr<BlackboardComponent> blackboard_;
    while (owner_ && owner_->transform)
    {
        blackboard_ = owner_->GetComponent<BlackboardComponent>();
        if (blackboard_) break;

        const std::shared_ptr<Transform> parentTransform_ = owner_->transform->parent.lock();
        if (!parentTransform_) break;
        const std::shared_ptr<GameObject> parentObject_ = parentTransform_->gameObject.lock();
        if (!parentObject_) break;
        owner_ = parentObject_;
    }
    if (!blackboard_) return;

    float value_ = 0.0f;
    int intValue_ = 0;
    if (attackDataBlackboardOverrideEnabled)
    {
        if (!damageKey.empty() && blackboard_->GetFloat(blackboardCategory, damageKey, value_))
        {
            damage = value_;
        }
        if (!hitStopDurationKey.empty() && blackboard_->GetFloat(blackboardCategory, hitStopDurationKey, value_))
        {
            hitStopDuration = value_;
        }
        else if (blackboard_->GetFloat(blackboardCategory, "HitStopTime", value_))
        {
            hitStopDuration = value_;
        }
        if (blackboard_->GetFloat(blackboardCategory, "HitStopScale", value_) ||
            blackboard_->GetFloat(blackboardCategory, "HitStopDir", value_))
        {
            hitStopScale = (std::clamp)(value_, 0.0f, 1.0f);
        }
        if (blackboard_->GetInt(blackboardCategory, "HitStopLayerMask", intValue_))
        {
            hitStopLayerMask = static_cast<uint32_t>((std::max)(intValue_, 0));
        }
        if (blackboard_->GetInt(blackboardCategory, "AttackInstanceId", intValue_))
        {
            attackInstanceId = static_cast<uint32_t>((std::max)(intValue_, 0));
        }
        if (blackboard_->GetInt(blackboardCategory, "HitGroupId", intValue_))
        {
            hitGroupId = static_cast<uint32_t>((std::max)(intValue_, 0));
        }
        if (!knockBackPowerKey.empty() && blackboard_->GetFloat(blackboardCategory, knockBackPowerKey, value_))
        {
            knockBackPower = value_;
        }
    }

    std::string stringValue_;
    if (hitEffectBlackboardOverrideEnabled &&
        blackboard_->GetString(blackboardCategory, "HitParticleAssetPath", stringValue_))
    {
        hitParticleAssetPath = stringValue_;
    }
    if (hitEffectBlackboardOverrideEnabled &&
        blackboard_->GetString(blackboardCategory, "HitParticleEmitterName", stringValue_))
    {
        hitParticleEmitterName = stringValue_;
    }
    if (hitEffectBlackboardOverrideEnabled &&
        blackboard_->GetString(blackboardCategory, "HitMeshVFXAssetPath", stringValue_))
    {
        hitMeshVFXAssetPath = stringValue_;
    }
    if (!hitSoundPathKey.empty() && blackboard_->GetString(blackboardCategory, hitSoundPathKey, stringValue_))
    {
        hitSoundPath = stringValue_;
    }
    if (hitEffectBlackboardOverrideEnabled &&
        blackboard_->GetFloat(blackboardCategory, "HitParticleLifeTime", value_))
    {
        hitParticleLifeTime = (std::max)(value_, 0.0f);
    }
    if (hitEffectBlackboardOverrideEnabled &&
        blackboard_->GetFloat(blackboardCategory, "HitParticleNormalOffset", value_))
    {
        hitParticleNormalOffset = value_;
    }
    if (hitEffectBlackboardOverrideEnabled &&
        blackboard_->GetFloat(blackboardCategory, "HitMeshVFXLifeTime", value_))
    {
        hitMeshVFXLifeTime = (std::max)(value_, 0.0f);
    }
    if (hitEffectBlackboardOverrideEnabled &&
        blackboard_->GetFloat(blackboardCategory, "HitMeshVFXNormalOffset", value_))
    {
        hitMeshVFXNormalOffset = value_;
    }
    if (blackboard_->GetFloat(blackboardCategory, "HitSoundVolume", value_))
    {
        hitSoundVolume = (std::max)(value_, 0.0f);
    }
    if (blackboard_->GetFloat(blackboardCategory, "HitSoundMuffledCutoff", value_))
    {
        hitSoundMuffledCutoff = (std::max)(value_, 10.0f);
    }
    bool boolValue_ = false;
    if (blackboard_->GetBool(blackboardCategory, "HitSoundMuffled", boolValue_))
    {
        hitSoundMuffled = boolValue_;
    }
}

void HitBox::RegisterActions()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    std::weak_ptr<HitBox> weakHitBox_;
    for (const std::shared_ptr<Component>& component_ : owner_->colliderComponents)
    {
        const std::shared_ptr<HitBox> hitBox_ = std::dynamic_pointer_cast<HitBox>(component_);
        if (hitBox_.get() == this)
        {
            weakHitBox_ = hitBox_;
            break;
        }
    }

    const std::string activateName_ = GetActivateActionName();
    const std::string deactivateName_ = GetDeactivateActionName();

    if (!activateName_.empty())
    {
        owner_->RegisterAction("Combat", activateName_, [weakHitBox_]()
            {
                if (const std::shared_ptr<HitBox> hitBox_ = weakHitBox_.lock())
                {
                    hitBox_->Activate();
                }
            });
    }

    if (!deactivateName_.empty())
    {
        owner_->RegisterAction("Combat", deactivateName_, [weakHitBox_]()
            {
                if (const std::shared_ptr<HitBox> hitBox_ = weakHitBox_.lock())
                {
                    hitBox_->Deactivate();
                }
            });
    }

    const std::string actionPrefix_ = GetActionPrefix();
    owner_->RegisterAction("Combat", actionPrefix_ + "_NewAttackInstance", [weakHitBox_]()
        {
            if (const std::shared_ptr<HitBox> hitBox_ = weakHitBox_.lock())
            {
                const std::shared_ptr<GameObject> object_ = hitBox_->GetGameObject();
                if (!object_) return;
                hitBox_->SetAttackInstanceId(
                    object_->GetManager().GetAttackSystem().CreateAttackInstance());
            }
        });
    owner_->RegisterAction("Combat", actionPrefix_ + "_ClearAttackHits", [weakHitBox_]()
        {
            if (const std::shared_ptr<HitBox> hitBox_ = weakHitBox_.lock())
            {
                hitBox_->ClearHitTargets();
                const std::shared_ptr<GameObject> object_ = hitBox_->GetGameObject();
                if (object_)
                {
                    object_->GetManager().GetAttackSystem().ClearAttackInstance(
                        hitBox_->attackInstanceId);
                }
            }
        });
}

void HitBox::EnforceHitBoxSettings()
{
    collisionRole = CollisionRole::HIT_BOX;
    usePhysicsResponse = false;
    physicsControlsTransform = false;
    useGravity = false;
    isTrigger = false;
}


