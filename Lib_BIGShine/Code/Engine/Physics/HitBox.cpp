#include "PrecompiledHeader.h"

#include "HitBox.h"

#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"

#include <algorithm>


HitBox::HitBox()
{
    // HitBoxは攻撃Action中の受付窓だけ有効にするColliderなので、生成時は無効にしておく。
    // 物理反応を持たないTriggerとして固定し、色は攻撃判定らしく赤系にする。
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
    // 武器や子ObjectにHitBoxが付いていても、攻撃者として扱うObjectを明示できるようにする。
    ownerObject = owner_;
}

void HitBox::SetAttackInstanceId(uint32_t attackInstanceId_)
{
    // 攻撃Actionが切り替わったら、過去にHitした対象履歴は別攻撃として扱うため消す。
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

void HitBox::SetImpactPower(float impactPower_)
{
    impactPower = (std::max)(impactPower_, 0.0f);
}

void HitBox::SetKnockBackPower(float knockBackPower_)
{
    knockBackPower = knockBackPower_;
}

bool HitBox::ShouldNotifyAttackerOnGuard() const
{
    // 近接攻撃だけ、Guardされた時に攻撃者側へ硬直/キャンセルなどの反応通知を返す。
    // ProjectileやRemoteは発生源と攻撃者が離れているため、攻撃者本人へ返さない。
    return attackSourceType == ATTACK_SOURCE_TYPE::MELEE;
}

void HitBox::SetAirLiftPower(float targetAirLiftPower_, float attackerAirLiftPower_)
{
    targetAirLiftPower = (std::max)(targetAirLiftPower_, 0.0f);
    attackerAirLiftPower = (std::max)(attackerAirLiftPower_, 0.0f);
}

void HitBox::SetHitEffectPath(const std::string& path_)
{
    // 外部のAttackTemplateなどから、Hit時に出すMeshVFXだけを差し替えるための簡易Setter。
    hitMeshVFXAssetPath = path_;
}

void HitBox::ClearHitTargets()
{
    // 同一activation中に既にHitした対象を忘れる。
    // 新しい攻撃IDやDebug操作で、対象へ再Hitさせたい時に呼ぶ。
    alreadyHitTargets.clear();
}

bool HitBox::CanHit(GameObject* target_) const
{
    // ownerへの自爆判定と、同一Activation内の多重Hitをここで止める。
    // AttackSystem側のattackInstanceId履歴とあわせて、HitBox単体でも最低限の重複防止を持つ。
    if (target_ == nullptr) return false;
    if (!canHitOwner && ownerObject == target_) return false;
    return allowMultiHit || alreadyHitTargets.count(target_) == 0u;
}

void HitBox::RegisterHit(GameObject* target_)
{
    // Hit成立後に対象を記録し、allowMultiHitがfalseなら同じ受付窓で再Hitしないようにする。
    if (target_ != nullptr) alreadyHitTargets.insert(target_);
}

std::shared_ptr<GameObject> HitBox::GetAttackOwner()
{
    // 明示Owner、CollisionOwner、HitBox自身のObjectの順で攻撃者を解決する。
    // 武器や子Colliderから発生したHitでも、親のPlayer/Enemyを攻撃者として扱えるようにする。
    if (ownerObject != nullptr)
    {
        return ownerObject->shared_from_this();
    }
    if (const std::shared_ptr<GameObject> collisionOwner_ = ResolveCollisionOwnerObject())
    {
        return collisionOwner_;
    }
    return GetGameObject();
}

void HitBox::Start()
{
    // CapsuleColliderとして初期化し、ActionSequenceからHitBoxを開閉するActionを登録する。
    // Load直後は必ずDeactivateし、攻撃判定が出っぱなしになるのを防ぐ。
    CapsuleCollider::Start();
    RegisterActions();
    Deactivate();
}

void HitBox::Activate()
{
    // SequencerやActionから有効化される直前に、Blackboardで上書きされた攻撃値を反映する。
    // これにより同じHitBox Componentを使い回しつつ、AttackTemplateごとにDamage/VFX/SEを変えられる。
    ApplyBlackboardValues();
    if (ownerObject == nullptr)
    {
        // 明示OwnerがないHitBoxは、このActivateを1つの新しい攻撃としてAttackSystemからIDをもらう。
        // Action側でIDを先に設定している場合は、そのIDを使って多段Hitや複数HitBoxをまとめられる。
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
    // Trigger状態を消してから有効化し、前回受付窓の接触状態を引きずらないようにする。
    currentTriggers.clear();
    SetEnabled(true);
}

void HitBox::Deactivate()
{
    // 攻撃受付終了時はColliderを無効化し、Trigger状態とHit済み対象を破棄する。
    SetEnabled(false);
    currentTriggers.clear();
    ClearHitTargets();
}

void HitBox::Gui()
{
    // 攻撃判定のDamage、HitStop、VFX、SE、CameraShake、Action名、Blackboard参照先をEditorで調整する。
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
    ImGui::DragFloat("Impact Power", &impactPower, 0.1f, 0.0f, 99999.0f);
    ImGui::DragFloat("Knock Back Power", &knockBackPower, 0.1f, 0.0f, 99999.0f);
    const char* attackSourceTypeNames_[] = { "Melee", "Projectile", "Remote" };
    int attackSourceTypeIndex_ = static_cast<int>(attackSourceType);
    if (ImGui::Combo(
        "Attack Source Type",
        &attackSourceTypeIndex_,
        attackSourceTypeNames_,
        IM_ARRAYSIZE(attackSourceTypeNames_)))
    {
        attackSourceType = static_cast<ATTACK_SOURCE_TYPE>(
            (std::clamp)(attackSourceTypeIndex_, 0, 2));
    }
    ImGui::DragFloat("Target Air Lift Power", &targetAirLiftPower, 0.1f, 0.0f, 99999.0f);
    ImGui::DragFloat("Attacker Air Lift Power", &attackerAirLiftPower, 0.1f, 0.0f, 99999.0f);
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
    ImGui::Checkbox("Use Particle Impact Axis", &hitParticleUseImpactAxis);
    ImGui::BeginDisabled(!hitParticleUseImpactAxis);
    ImGui::SliderFloat("Axis To Attacker Blend", &hitParticleAxisToAttackerBlend, 0.0f, 1.0f);
    ImGui::EndDisabled();
    ImGui::Checkbox("Use Particle Target Pull", &hitParticleUseTargetAttraction);
    ImGui::BeginDisabled(!hitParticleUseTargetAttraction);
    ImGui::DragFloat("Target Pull Strength", &hitParticleTargetAttractionStrength, 0.01f, -100.0f, 100.0f);
    ImGui::DragFloat("Target Pull Range", &hitParticleTargetAttractionRange, 0.01f, 0.0f, 100.0f);
    ImGui::DragFloat("Target Pull Falloff", &hitParticleTargetAttractionFalloff, 0.01f, 0.001f, 8.0f);
    ImGui::EndDisabled();

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

    ImGui::SeparatorText("Camera Shake");
    ImGui::Checkbox("Camera Shake Enabled", &cameraShakeEnabled);
    ImGui::DragFloat("Receive Duration", &cameraShakeReceiveDuration, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("Receive Position Strength", &cameraShakeReceivePositionStrength, 0.001f, 0.0f, 2.0f);
    ImGui::DragFloat("Receive Rotation Strength", &cameraShakeReceiveRotationStrength, 0.01f, 0.0f, 20.0f);
    ImGui::DragFloat("Attack Scale", &cameraShakeAttackScale, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Frequency##CameraShake", &cameraShakeFrequency, 0.1f, 0.0f, 120.0f);
    ImGui::DragInt("Priority##CameraShake", &cameraShakePriority);

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
    // Capsule形状に加えて、攻撃結果、演出、Action名、Blackboard参照Keyを保存する。
    // activationIdやHit済み対象などFrame中の履歴は保存しない。
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
    j_["ImpactPower"] = impactPower;
    j_["KnockBackPower"] = knockBackPower;
    j_["AttackSourceType"] = static_cast<int>(attackSourceType);
    j_["TargetAirLiftPower"] = targetAirLiftPower;
    j_["AttackerAirLiftPower"] = attackerAirLiftPower;
    j_["HitOncePerActivation"] = hitOncePerActivation;
    j_["AttackTag"] = attackTag;
    j_["HitParticleAssetPath"] = hitParticleAssetPath;
    j_["HitParticleEmitterName"] = hitParticleEmitterName;
    j_["HitParticleLifeTime"] = hitParticleLifeTime;
    j_["HitParticleNormalOffset"] = hitParticleNormalOffset;
    j_["HitParticleUseImpactAxis"] = hitParticleUseImpactAxis;
    j_["HitParticleAxisToAttackerBlend"] = hitParticleAxisToAttackerBlend;
    j_["HitParticleUseTargetAttraction"] = hitParticleUseTargetAttraction;
    j_["HitParticleTargetAttractionStrength"] = hitParticleTargetAttractionStrength;
    j_["HitParticleTargetAttractionRange"] = hitParticleTargetAttractionRange;
    j_["HitParticleTargetAttractionFalloff"] = hitParticleTargetAttractionFalloff;
    j_["HitMeshVFXAssetPath"] = hitMeshVFXAssetPath;
    j_["HitMeshVFXLifeTime"] = hitMeshVFXLifeTime;
    j_["HitMeshVFXNormalOffset"] = hitMeshVFXNormalOffset;
    j_["HitSoundPath"] = hitSoundPath;
    j_["HitSoundVolume"] = hitSoundVolume;
    j_["HitSoundMuffled"] = hitSoundMuffled;
    j_["HitSoundMuffledCutoff"] = hitSoundMuffledCutoff;
    j_["CameraShakeEnabled"] = cameraShakeEnabled;
    j_["CameraShakeReceiveDuration"] = cameraShakeReceiveDuration;
    j_["CameraShakeReceivePositionStrength"] = cameraShakeReceivePositionStrength;
    j_["CameraShakeReceiveRotationStrength"] = cameraShakeReceiveRotationStrength;
    j_["CameraShakeAttackScale"] = cameraShakeAttackScale;
    j_["CameraShakeFrequency"] = cameraShakeFrequency;
    j_["CameraShakePriority"] = cameraShakePriority;
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
    // 保存値を復元しつつ、古いKey名にもFallbackして既存Sceneデータを読めるようにする。
    // Runtime履歴ではなく設定値だけを復元し、最後にHitBox固定設定を再適用する。
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
    impactPower = (std::max)(j_.value("ImpactPower", damage), 0.0f);
    knockBackPower = j_.value("KnockBackPower", 0.0f);
    attackSourceType = static_cast<ATTACK_SOURCE_TYPE>(
        (std::clamp)(j_.value("AttackSourceType", 0), 0, 2));
    targetAirLiftPower = (std::max)(j_.value("TargetAirLiftPower", 0.0f), 0.0f);
    attackerAirLiftPower = (std::max)(j_.value("AttackerAirLiftPower", 0.0f), 0.0f);
    hitOncePerActivation = j_.value("HitOncePerActivation", true);
    attackTag = j_.value("AttackTag", std::string("Attack"));
    hitParticleAssetPath = j_.value("HitParticleAssetPath", std::string{});
    hitParticleEmitterName = j_.value("HitParticleEmitterName", std::string{});
    hitParticleLifeTime = (std::max)(j_.value("HitParticleLifeTime", 1.0f), 0.0f);
    hitParticleNormalOffset = j_.value("HitParticleNormalOffset", 0.0f);
    hitParticleUseImpactAxis = j_.value("HitParticleUseImpactAxis", false);
    hitParticleAxisToAttackerBlend =
        (std::clamp)(j_.value("HitParticleAxisToAttackerBlend", 0.0f), 0.0f, 1.0f);
    hitParticleUseTargetAttraction = j_.value("HitParticleUseTargetAttraction", false);
    hitParticleTargetAttractionStrength = j_.value("HitParticleTargetAttractionStrength", 0.0f);
    hitParticleTargetAttractionRange =
        (std::max)(j_.value("HitParticleTargetAttractionRange", 0.0f), 0.0f);
    hitParticleTargetAttractionFalloff =
        (std::max)(j_.value("HitParticleTargetAttractionFalloff", 1.0f), 0.001f);
    hitMeshVFXAssetPath = j_.value("HitMeshVFXAssetPath", std::string{});
    hitMeshVFXLifeTime = (std::max)(j_.value("HitMeshVFXLifeTime", 1.0f), 0.0f);
    hitMeshVFXNormalOffset = j_.value("HitMeshVFXNormalOffset", 0.0f);
    hitSoundPath = j_.value("HitSoundPath", std::string{});
    hitSoundVolume = (std::max)(j_.value("HitSoundVolume", 1.0f), 0.0f);
    hitSoundMuffled = j_.value("HitSoundMuffled", false);
    hitSoundMuffledCutoff = (std::max)(j_.value("HitSoundMuffledCutoff", 1200.0f), 10.0f);
    cameraShakeEnabled = j_.value("CameraShakeEnabled", true);
    cameraShakeReceiveDuration =
        (std::max)(j_.value("CameraShakeReceiveDuration", 0.12f), 0.0f);
    cameraShakeReceivePositionStrength =
        (std::max)(j_.value("CameraShakeReceivePositionStrength", 0.025f), 0.0f);
    cameraShakeReceiveRotationStrength =
        (std::max)(j_.value("CameraShakeReceiveRotationStrength", 0.18f), 0.0f);
    cameraShakeAttackScale =
        (std::clamp)(j_.value("CameraShakeAttackScale", 0.5f), 0.0f, 1.0f);
    cameraShakeFrequency =
        (std::max)(j_.value("CameraShakeFrequency", 24.0f), 0.0f);
    cameraShakePriority = j_.value("CameraShakePriority", 5);
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
    // Prefix未指定ならOwner名を使い、複数ObjectのHitBox Action名が衝突しないようにする。
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
    // Prefix付きの有効化Action名を作り、Animation/ActionSequenceからこのHitBoxを開けるようにする。
    const std::string prefix_ = GetActionPrefix();

    if (prefix_.empty())
    {
        return activateActionName;
    }

    return prefix_ + "_" + activateActionName;
}

std::string HitBox::GetDeactivateActionName() const
{
    // 有効化と同じPrefix規則で、攻撃受付終了用Action名を作る。
    const std::string prefix_ = GetActionPrefix();

    if (prefix_.empty())
    {
        return deactivateActionName;
    }

    return prefix_ + "_" + deactivateActionName;
}

void HitBox::ApplyBlackboardValues()
{
    // 攻撃Action実行時にBlackboardへ流された一時データを読み取り、HitBoxの現在値へ反映する。
    // ComponentのScene保存値は基礎値として残し、実際の攻撃ごとの差分はBlackboardから上書きする設計。
    std::shared_ptr<GameObject> owner_ = GetGameObject();
    std::shared_ptr<BlackboardComponent> blackboard_;

    // HitBoxが武器などの子Objectに付いていても、親側Blackboardの攻撃情報を拾えるように上へたどる。
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
        // ActionSequenceやAttackTemplateRuntimeが書いた一時値で、DamageやHitStopを上書きする。
        // Key名を設定で持たせているため、攻撃ごとにBlackboard構成が少し違っても使える。
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
        if (blackboard_->GetFloat(blackboardCategory, "ImpactPower", value_))
        {
            impactPower = (std::max)(value_, 0.0f);
        }
        if (blackboard_->GetFloat(blackboardCategory, "TargetAirLiftPower", value_))
        {
            targetAirLiftPower = (std::max)(value_, 0.0f);
        }
        if (blackboard_->GetFloat(blackboardCategory, "AttackerAirLiftPower", value_))
        {
            attackerAirLiftPower = (std::max)(value_, 0.0f);
        }
    }

    std::string stringValue_;
    // Hit演出もBlackboardで上書きできる。AttackTemplateからParticle/MeshVFX/SEを差し替えるための経路。
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
        blackboard_->GetFloat(blackboardCategory, "HitParticleAxisToAttackerBlend", value_))
    {
        hitParticleAxisToAttackerBlend = (std::clamp)(value_, 0.0f, 1.0f);
    }
    if (hitEffectBlackboardOverrideEnabled &&
        blackboard_->GetFloat(blackboardCategory, "HitParticleTargetAttractionStrength", value_))
    {
        hitParticleTargetAttractionStrength = value_;
    }
    if (hitEffectBlackboardOverrideEnabled &&
        blackboard_->GetFloat(blackboardCategory, "HitParticleTargetAttractionRange", value_))
    {
        hitParticleTargetAttractionRange = (std::max)(value_, 0.0f);
    }
    if (hitEffectBlackboardOverrideEnabled &&
        blackboard_->GetFloat(blackboardCategory, "HitParticleTargetAttractionFalloff", value_))
    {
        hitParticleTargetAttractionFalloff = (std::max)(value_, 0.001f);
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
    if (hitEffectBlackboardOverrideEnabled &&
        blackboard_->GetBool(blackboardCategory, "HitParticleUseImpactAxis", boolValue_))
    {
        hitParticleUseImpactAxis = boolValue_;
    }
    if (hitEffectBlackboardOverrideEnabled &&
        blackboard_->GetBool(blackboardCategory, "HitParticleUseTargetAttraction", boolValue_))
    {
        hitParticleUseTargetAttraction = boolValue_;
    }
}

void HitBox::RegisterActions()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    // Animation Sequencerから名前でHitBoxをON/OFFできるよう、Local Actionとして公開する。
    // lambdaはweak_ptr経由にし、Object削除後にActionが呼ばれても安全に抜けるようにする。
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
            // 複数HitBoxや複数受付窓を同じ攻撃として扱いたい時、Actionから明示的に新しい攻撃IDを作る。
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
            // 攻撃IDに紐づくHit/Guard/JustDodge履歴をまとめて消し、同じAction内で再判定できる状態に戻す。
            if (const std::shared_ptr<HitBox> hitBox_ = weakHitBox_.lock())
            {
                hitBox_->ClearHitTargets();
                const std::shared_ptr<GameObject> object_ = hitBox_->GetGameObject();
                if (object_)
                {
                    object_->GetManager().GetAttackSystem().ClearAttackInstance(
                        hitBox_->attackInstanceId);
                    object_->GetManager().GetJustDodgeSystem().ClearAttackInstance(
                        hitBox_->attackInstanceId);
                    object_->GetManager().GetGuardSystem().ClearAttackInstance(
                        hitBox_->attackInstanceId);
                }
            }
        });
}

void HitBox::EnforceHitBoxSettings()
{
    // HitBoxは物理的に押し返さず、TriggerとしてCombat SystemへHit候補を渡す。
    collisionRole = CollisionRole::HIT_BOX;
    usePhysicsResponse = false;
    physicsControlsTransform = false;
    useGravity = false;
    isTrigger = true;
}


