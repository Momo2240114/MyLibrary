#include "PrecompiledHeader.h"

#include "AttackEffectHitComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/Physics/HitBox.h"

#include <algorithm>
#include <cstdio>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <nlohmann/json.hpp>

ATTACK_HIT_SHAPE NormalizeAttackHitShape(ATTACK_HIT_SHAPE shape_)
{
    // 現在のAttackEffectHitComponentで扱える形状だけを通し、未対応形状はCapsuleへ丸める。
    switch (shape_)
    {
    case ATTACK_HIT_SHAPE::NONE:
    case ATTACK_HIT_SHAPE::SPHERE:
    case ATTACK_HIT_SHAPE::CAPSULE:
    case ATTACK_HIT_SHAPE::SPHERE_GROUP:
        return shape_;
    case ATTACK_HIT_SHAPE::BOX:
    case ATTACK_HIT_SHAPE::FAN:
    case ATTACK_HIT_SHAPE::CONE:
    case ATTACK_HIT_SHAPE::RAY:
    default:
        return ATTACK_HIT_SHAPE::CAPSULE;
    }
}

const std::string& AttackEffectHitComponent::GetName() const
{
    static const std::string name_ = "AttackEffectHitComponent";
    return name_;
}

void AttackEffectHitComponent::Start()
{
    // 保存済みのテンプレートから、実際に判定を持つ子GameObject/HitBoxを作り直す。
    RebuildHitShapes();
}

void AttackEffectHitComponent::Update(float deltaTime_)
{
    if (!playing) return;
    const float safeDeltaTime_ = (std::max)(deltaTime_, 0.0f);
    elapsedTime += safeDeltaTime_;
    hitIntervalTimer -= safeDeltaTime_;

    // Projectile系のように、判定オブジェクト自体が毎フレーム前進する演出に対応する。
    const std::shared_ptr<GameObject> object_ = GetGameObject();
    if (object_ && movementSpeed != 0.0f)
    {
        object_->transform->AddMoveOffset({
            movementDirection.x * movementSpeed * safeDeltaTime_,
            movementDirection.y * movementSpeed * safeDeltaTime_,
            movementDirection.z * movementSpeed * safeDeltaTime_ });
    }

    bool intervalReady_ = false;
    // hitIntervalが来たら命中履歴を消し、持続判定でも一定間隔で再ヒットできるようにする。
    for (const AttackEffectHitShapeTemplate& shape_ : hitShapes)
    {
        if (shape_.hitInterval > 0.0f && hitIntervalTimer <= 0.0f)
        {
            hitIntervalTimer = shape_.hitInterval;
            intervalReady_ = true;
            break;
        }
    }
    if (intervalReady_) ClearHitHistory();

    // 各HitBoxの有効時間をテンプレートのactiveStart/Endに合わせて切り替える。
    for (RuntimeHitShape& runtime_ : runtimeHitShapes)
    {
        if (!runtime_.hitBox || runtime_.templateIndex >= hitShapes.size()) continue;
        const AttackEffectHitShapeTemplate& shape_ = hitShapes[runtime_.templateIndex];
        const bool active_ = shape_.enabled &&
            elapsedTime >= shape_.activeStartTime && elapsedTime <= shape_.activeEndTime;
        if (active_ && !runtime_.hitBox->IsEnabled()) runtime_.hitBox->Activate();
        if (!active_ && runtime_.hitBox->IsEnabled()) runtime_.hitBox->Deactivate();
    }

    if (elapsedTime >= lifeTime)
    {
        // コンポーネント寿命が終わったら判定を止め、必要なら演出用GameObjectごと消す。
        Stop();
        if (destroyOnLifeEnd && object_) object_->GetManager().Destroy(object_);
    }
}

void AttackEffectHitComponent::SetOwner(GameObject* owner_) { ownerObject = owner_; }
void AttackEffectHitComponent::SetAttackInstanceId(uint32_t attackInstanceId_) { attackInstanceId = attackInstanceId_; }
void AttackEffectHitComponent::SetHitGroupId(uint32_t hitGroupId_) { hitGroupId = hitGroupId_; }
void AttackEffectHitComponent::SetDamage(float damage_) { damage = damage_; }
void AttackEffectHitComponent::SetDamageRate(float damageRate_) { damageRate = (std::max)(damageRate_, 0.0f); }
void AttackEffectHitComponent::SetFixedDamage(float fixedDamage_) { fixedDamage = fixedDamage_; }
void AttackEffectHitComponent::SetDamageApplyType(DAMAGE_APPLY_TYPE damageApplyType_) { damageApplyType = damageApplyType_; }
void AttackEffectHitComponent::SetDamageHitInterval(float damageHitInterval_) { damageHitInterval = (std::max)(damageHitInterval_, 0.0f); }
void AttackEffectHitComponent::SetMaxHitCount(int maxHitCount_) { maxHitCount = maxHitCount_; }
void AttackEffectHitComponent::SetImpactPower(float impactPower_) { impactPower = (std::max)(impactPower_, 0.0f); }
void AttackEffectHitComponent::SetHitStopDuration(float hitStopDuration_) { hitStopDuration = hitStopDuration_; }
void AttackEffectHitComponent::SetHitStopScale(float hitStopScale_) { hitStopScale = hitStopScale_; }
void AttackEffectHitComponent::SetKnockBackPower(float knockBackPower_) { knockBackPower = knockBackPower_; }
void AttackEffectHitComponent::SetAirLiftPower(float targetAirLiftPower_, float attackerAirLiftPower_)
{
    targetAirLiftPower = (std::max)(targetAirLiftPower_, 0.0f);
    attackerAirLiftPower = (std::max)(attackerAirLiftPower_, 0.0f);
}

void AttackEffectHitComponent::SetCameraShake(
    bool enabled_,
    float receiveDuration_,
    float receivePositionStrength_,
    float receiveRotationStrength_,
    float attackScale_,
    float frequency_,
    int priority_)
{
    // HitBoxへ渡すカメラシェイク設定をまとめて保持し、生成済み判定へConfigure時に反映する。
    cameraShakeEnabled = enabled_;
    cameraShakeReceiveDuration = (std::max)(receiveDuration_, 0.0f);
    cameraShakeReceivePositionStrength = (std::max)(receivePositionStrength_, 0.0f);
    cameraShakeReceiveRotationStrength = (std::max)(receiveRotationStrength_, 0.0f);
    cameraShakeAttackScale = (std::clamp)(attackScale_, 0.0f, 1.0f);
    cameraShakeFrequency = (std::max)(frequency_, 0.0f);
    cameraShakePriority = priority_;
}

void AttackEffectHitComponent::SetHitParticleAssetPath(const std::string& path_)
{
    hitParticleAssetPath = path_;
    std::printf(
        "[AttackEffectHitComponent::SetHitParticleAssetPath] path='%s'\n",
        hitParticleAssetPath.c_str());
}

void AttackEffectHitComponent::SetHitParticleEmitterName(const std::string& emitterName_)
{
    hitParticleEmitterName = emitterName_;
}

void AttackEffectHitComponent::SetHitParticleLifeTime(float lifeTime_)
{
    hitParticleLifeTime = (std::max)(lifeTime_, 0.0f);
}

void AttackEffectHitComponent::SetHitParticleNormalOffset(float normalOffset_)
{
    hitParticleNormalOffset = normalOffset_;
}

void AttackEffectHitComponent::SetHitParticleImpactAxis(
    bool enabled_,
    float axisToAttackerBlend_)
{
    // ヒット時Particleの向きを、衝突法線と攻撃者方向のどちらへ寄せるかを設定する。
    hitParticleUseImpactAxis = enabled_;
    hitParticleAxisToAttackerBlend = std::clamp(axisToAttackerBlend_, 0.0f, 1.0f);
}

void AttackEffectHitComponent::SetHitParticleTargetAttraction(
    bool enabled_,
    float strength_,
    float range_,
    float falloff_)
{
    // 命中位置をターゲット側へ引き寄せる補正値。大きな敵や広い判定で見た目を合わせるために使う。
    hitParticleUseTargetAttraction = enabled_;
    hitParticleTargetAttractionStrength = strength_;
    hitParticleTargetAttractionRange = (std::max)(range_, 0.0f);
    hitParticleTargetAttractionFalloff = (std::max)(falloff_, 0.001f);
}

void AttackEffectHitComponent::SetHitMeshVFXAssetPath(const std::string& path_)
{
    hitMeshVFXAssetPath = path_;
    std::printf(
        "[AttackEffectHitComponent::SetHitMeshVFXAssetPath] path='%s'\n",
        hitMeshVFXAssetPath.c_str());
}

void AttackEffectHitComponent::SetHitMeshVFXLifeTime(float lifeTime_)
{
    hitMeshVFXLifeTime = (std::max)(lifeTime_, 0.0f);
}

void AttackEffectHitComponent::SetHitMeshVFXNormalOffset(float normalOffset_)
{
    hitMeshVFXNormalOffset = normalOffset_;
}
void AttackEffectHitComponent::SetAllowMultiHit(bool allowMultiHit_) { allowMultiHit = allowMultiHit_; }
void AttackEffectHitComponent::SetCanHitOwner(bool canHitOwner_) { canHitOwner = canHitOwner_; }
void AttackEffectHitComponent::SetEffectType(ATTACK_EFFECT_TYPE effectType_) { effectType = effectType_; }
void AttackEffectHitComponent::SetLifeTime(float lifeTime_) { lifeTime = (std::max)(lifeTime_, 0.0f); }
void AttackEffectHitComponent::SetDestroyOnLifeEnd(bool destroyOnLifeEnd_) { destroyOnLifeEnd = destroyOnLifeEnd_; }
void AttackEffectHitComponent::SetMovement(const DirectX::XMFLOAT3& direction_, float speed_)
{
    movementDirection = direction_;
    movementSpeed = speed_;
}

void AttackEffectHitComponent::SetHitShapes(
    const std::vector<AttackEffectHitShapeTemplate>& hitShapes_)
{
    // テンプレートを差し替えたら、未対応形状を丸めた上でランタイムHitBoxも作り直す。
    hitShapes = hitShapes_;
    for (AttackEffectHitShapeTemplate& shape_ : hitShapes)
    {
        shape_.shape = NormalizeAttackHitShape(shape_.shape);
        if (shape_.hitInterval <= 0.0f && damageHitInterval > 0.0f)
        {
            shape_.hitInterval = damageHitInterval;
        }
    }
    RebuildHitShapes();
}

void AttackEffectHitComponent::Play()
{
    // 再生開始時は時間と命中履歴をリセットし、前回再生のヒット対象を引き継がない。
    ResetRuntime();
    playing = true;
}

void AttackEffectHitComponent::Stop()
{
    // 停止時は生成済みHitBoxをすべて無効化し、残った判定が当たり続けないようにする。
    playing = false;
    for (RuntimeHitShape& runtime_ : runtimeHitShapes)
        if (runtime_.hitBox) runtime_.hitBox->Deactivate();
}

void AttackEffectHitComponent::ResetRuntime()
{
    elapsedTime = 0.0f;
    hitIntervalTimer = 0.0f;
    ClearHitHistory();
}

void AttackEffectHitComponent::SubmitDebugShapes()
{
    // デバッグ描画用に、子判定オブジェクトのTransformとHitBox状態だけを即時更新する。
    for (RuntimeHitShape& runtime_ : runtimeHitShapes)
    {
        if (!runtime_.object || !runtime_.hitBox) continue;
        runtime_.object->UpdateTransform();
        runtime_.hitBox->Update(0.0f);
    }
}

void AttackEffectHitComponent::ClearHitHistory()
{
    // Attack/JustDodge/Guardの各システムが持つ同一攻撃グループの命中履歴をまとめてリセットする。
    const std::shared_ptr<GameObject> object_ = GetGameObject();
    if (object_ && attackInstanceId != 0u)
    {
        object_->GetManager().GetAttackSystem().ClearAttackHitGroup(attackInstanceId, hitGroupId);
        object_->GetManager().GetJustDodgeSystem().ClearAttackHitGroup(attackInstanceId, hitGroupId);
        object_->GetManager().GetGuardSystem().ClearAttackHitGroup(attackInstanceId, hitGroupId);
    }
    for (RuntimeHitShape& runtime_ : runtimeHitShapes)
        if (runtime_.hitBox) runtime_.hitBox->ClearHitTargets();
}

void AttackEffectHitComponent::ConfigureHitBox(
    const std::shared_ptr<HitBox>& hitBox_,
    const AttackEffectHitShapeTemplate& shape_,
    const std::string& name_)
{
    // コンポーネント側の攻撃情報と、形状テンプレート側のサイズ/多段可否をHitBoxへ転写する。
    hitBox_->actionPrefix = name_;
    hitBox_->SetOwner(ownerObject);
    hitBox_->SetAttackInstanceId(attackInstanceId);
    hitBox_->SetHitGroupId(hitGroupId);
    hitBox_->SetDamage(damage);
    hitBox_->SetDamageRate(damageRate);
    hitBox_->SetFixedDamage(fixedDamage);
    hitBox_->SetDamageApplyType(damageApplyType);
    hitBox_->SetDamageHitInterval(damageHitInterval);
    hitBox_->SetMaxHitCount(maxHitCount);
    hitBox_->SetImpactPower(impactPower);
    hitBox_->hitStopDuration = hitStopDuration;
    hitBox_->hitStopScale = hitStopScale;
    hitBox_->SetKnockBackPower(knockBackPower);
    hitBox_->SetAirLiftPower(targetAirLiftPower, attackerAirLiftPower);
    hitBox_->cameraShakeEnabled = cameraShakeEnabled;
    hitBox_->cameraShakeReceiveDuration = cameraShakeReceiveDuration;
    hitBox_->cameraShakeReceivePositionStrength = cameraShakeReceivePositionStrength;
    hitBox_->cameraShakeReceiveRotationStrength = cameraShakeReceiveRotationStrength;
    hitBox_->cameraShakeAttackScale = cameraShakeAttackScale;
    hitBox_->cameraShakeFrequency = cameraShakeFrequency;
    hitBox_->cameraShakePriority = cameraShakePriority;
    hitBox_->hitParticleAssetPath = hitParticleAssetPath;
    hitBox_->hitParticleEmitterName = hitParticleEmitterName;
    hitBox_->hitParticleLifeTime = hitParticleLifeTime;
    hitBox_->hitParticleNormalOffset = hitParticleNormalOffset;
    hitBox_->hitParticleUseImpactAxis = hitParticleUseImpactAxis;
    hitBox_->hitParticleAxisToAttackerBlend = hitParticleAxisToAttackerBlend;
    hitBox_->hitParticleUseTargetAttraction = hitParticleUseTargetAttraction;
    hitBox_->hitParticleTargetAttractionStrength = hitParticleTargetAttractionStrength;
    hitBox_->hitParticleTargetAttractionRange = hitParticleTargetAttractionRange;
    hitBox_->hitParticleTargetAttractionFalloff = hitParticleTargetAttractionFalloff;
    hitBox_->hitMeshVFXAssetPath = hitMeshVFXAssetPath;
    hitBox_->hitMeshVFXLifeTime = hitMeshVFXLifeTime;
    hitBox_->hitMeshVFXNormalOffset = hitMeshVFXNormalOffset;
    hitBox_->SetHitEffectBlackboardOverrideEnabled(false);
    hitBox_->SetAttackDataBlackboardOverrideEnabled(false);
    std::printf(
        "[AttackEffectHitComponent::ConfigureHitBox] event particle='%s' meshVFX='%s'\n",
        hitBox_->hitParticleAssetPath.c_str(),
        hitBox_->hitMeshVFXAssetPath.c_str());
    hitBox_->allowMultiHit = allowMultiHit || shape_.allowMultiHit;
    hitBox_->canHitOwner = canHitOwner;
    hitBox_->radius = (std::max)(shape_.radius, 0.01f);
    const ATTACK_HIT_SHAPE normalizedShape_ = NormalizeAttackHitShape(shape_.shape);
    // Sphereは高さを使わず、Capsule系だけhalfHeightを判定サイズに反映する。
    hitBox_->halfHeight = normalizedShape_ == ATTACK_HIT_SHAPE::SPHERE ? 0.0f : (std::max)(shape_.halfHeight, 0.0f);
    hitBox_->Deactivate();
    hitBox_->RecreatePhysXActor();
}

void AttackEffectHitComponent::RebuildHitShapes()
{
    // 古いランタイム判定を破棄してから、現在のテンプレートを元に子GameObjectを再生成する。
    for (RuntimeHitShape& runtime_ : runtimeHitShapes)
        if (runtime_.object) runtime_.object->GetManager().Destroy(runtime_.object);
    runtimeHitShapes.clear();
    const std::shared_ptr<GameObject> effectObject_ = GetGameObject();
    if (!effectObject_) return;

    for (size_t shapeIndex_ = 0; shapeIndex_ < hitShapes.size(); ++shapeIndex_)
    {
        const AttackEffectHitShapeTemplate& shape_ = hitShapes[shapeIndex_];
        const ATTACK_HIT_SHAPE normalizedShape_ = NormalizeAttackHitShape(shape_.shape);
        if (!shape_.enabled || normalizedShape_ == ATTACK_HIT_SHAPE::NONE) continue;
        // SphereGroupは複数のSphere HitBoxへ展開し、それ以外は1つのHitBoxとして生成する。
        const size_t sphereCount_ = normalizedShape_ == ATTACK_HIT_SHAPE::SPHERE_GROUP
            ? shape_.spheres.size() : 1u;
        for (size_t sphereIndex_ = 0; sphereIndex_ < sphereCount_; ++sphereIndex_)
        {
            const std::shared_ptr<GameObject> hitObject_ = effectObject_->GetManager().Create("RuntimeAttackHitShape");
            if (!hitObject_) continue;
            hitObject_->SetNoSaveObject();
            hitObject_->transform->SetParent(effectObject_->transform);
            DirectX::XMFLOAT3 offset_ = shape_.localOffset;
            float radius_ = shape_.radius;
            if (normalizedShape_ == ATTACK_HIT_SHAPE::SPHERE_GROUP && sphereIndex_ < shape_.spheres.size())
            {
                offset_ = shape_.spheres[sphereIndex_].localOffset;
                radius_ = shape_.spheres[sphereIndex_].radius;
            }
            hitObject_->transform->localPosition = offset_;
            hitObject_->transform->localRotation = shape_.localRotation;
            hitObject_->transform->localScale = shape_.localScale;
            hitObject_->transform->MarkDirty();
            std::shared_ptr<HitBox> hitBox_ = hitObject_->AddComponent<HitBox>();
            AttackEffectHitShapeTemplate runtimeShape_ = shape_;
            runtimeShape_.shape = normalizedShape_;
            runtimeShape_.radius = radius_;
            if (normalizedShape_ == ATTACK_HIT_SHAPE::SPHERE_GROUP)
            {
                // グループ内の個別判定はHitBox側では通常Sphereとして扱う。
                runtimeShape_.shape = ATTACK_HIT_SHAPE::SPHERE;
                runtimeShape_.halfHeight = 0.0f;
            }
            ConfigureHitBox(hitBox_, runtimeShape_, shape_.hitBoxName + std::to_string(sphereIndex_));
            runtimeHitShapes.push_back({ hitObject_, hitBox_, shapeIndex_ });
        }
    }
}

void AttackEffectHitComponent::Gui()
{
    // 攻撃判定の基本値、ヒット演出、形状テンプレートをエディタ上でまとめて調整する。
    int effectType_ = static_cast<int>(effectType);
    const char* effectNames_[] = { "Instant Area", "Projectile", "Area Over Time", "Beam", "Breath" };
    if (ImGui::Combo("Effect Type", &effectType_, effectNames_, IM_ARRAYSIZE(effectNames_)))
        effectType = static_cast<ATTACK_EFFECT_TYPE>(effectType_);
    ImGui::DragFloat("Life Time", &lifeTime, 0.01f, 0.0f);
    ImGui::Text("Elapsed: %.3f", elapsedTime);
    ImGui::Checkbox("Playing", &playing);
    ImGui::DragFloat("Legacy Damage", &damage, 0.1f, 0.0f);
    ImGui::DragFloat("Damage Rate", &damageRate, 0.01f, 0.0f);
    ImGui::DragFloat("Fixed Damage", &fixedDamage, 0.1f, -99999.0f, 99999.0f);
    const char* damageApplyTypeNames_[] = { "Single Hit", "Multi Hit", "Damage Over Time" };
    int damageApplyType_ = static_cast<int>(damageApplyType);
    if (ImGui::Combo("Damage Apply Type", &damageApplyType_, damageApplyTypeNames_, IM_ARRAYSIZE(damageApplyTypeNames_)))
        damageApplyType = static_cast<DAMAGE_APPLY_TYPE>((std::clamp)(damageApplyType_, 0, 2));
    ImGui::DragFloat("Damage Hit Interval", &damageHitInterval, 0.01f, 0.0f);
    ImGui::DragInt("Max Hit Count", &maxHitCount, 1.0f, 0, 9999);
    ImGui::DragFloat("Impact Power", &impactPower, 0.1f, 0.0f);
    ImGui::DragFloat("Hit Stop Duration", &hitStopDuration, 0.001f, 0.0f);
    ImGui::DragFloat("Hit Stop Scale", &hitStopScale, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Knock Back Power", &knockBackPower, 0.1f, 0.0f);
    ImGui::InputText("Hit Particle", &hitParticleAssetPath);
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
    ImGui::InputText("Hit Mesh VFX", &hitMeshVFXAssetPath);
    ImGui::DragFloat3("Movement Direction", &movementDirection.x, 0.01f);
    ImGui::DragFloat("Movement Speed", &movementSpeed, 0.1f);
    if (ImGui::Button("Play")) Play();
    ImGui::SameLine();
    if (ImGui::Button("Stop")) Stop();
    ImGui::SeparatorText("Hit Shapes");
    bool rebuild_ = false;
    if (ImGui::Button("Add Hit Shape"))
    {
        hitShapes.emplace_back();
        rebuild_ = true;
    }
    int removeIndex_ = -1;
    for (int shapeIndex_ = 0; shapeIndex_ < static_cast<int>(hitShapes.size()); ++shapeIndex_)
    {
        AttackEffectHitShapeTemplate& shape_ = hitShapes[shapeIndex_];
        ImGui::PushID(shapeIndex_);
        if (ImGui::TreeNode("HitShape", "Hit Shape %d", shapeIndex_ + 1))
        {
            // GUIで選べる形状も、ランタイムが対応しているものだけに限定する。
            shape_.shape = NormalizeAttackHitShape(shape_.shape);
            rebuild_ |= ImGui::Checkbox("Enabled", &shape_.enabled);
            rebuild_ |= ImGui::InputText("HitBox Name", &shape_.hitBoxName);
            ATTACK_HIT_SHAPE supportedShapes_[] = {
                ATTACK_HIT_SHAPE::NONE,
                ATTACK_HIT_SHAPE::SPHERE,
                ATTACK_HIT_SHAPE::CAPSULE,
                ATTACK_HIT_SHAPE::SPHERE_GROUP
            };
            const char* shapeNames_[] = { "None", "Sphere", "Capsule", "Sphere Group" };
            int shapeType_ = 0;
            for (int index_ = 0; index_ < IM_ARRAYSIZE(supportedShapes_); ++index_)
            {
                if (supportedShapes_[index_] == shape_.shape)
                {
                    shapeType_ = index_;
                    break;
                }
            }
            if (ImGui::Combo("Shape", &shapeType_, shapeNames_, IM_ARRAYSIZE(shapeNames_)))
            {
                shape_.shape = supportedShapes_[shapeType_];
                rebuild_ = true;
            }
            rebuild_ |= ImGui::DragFloat3("Local Offset", &shape_.localOffset.x, 0.01f);
            rebuild_ |= ImGui::DragFloat3("Local Rotation", &shape_.localRotation.x, 0.1f);
            rebuild_ |= ImGui::DragFloat3("Local Scale", &shape_.localScale.x, 0.01f);
            rebuild_ |= ImGui::DragFloat("Radius", &shape_.radius, 0.01f, 0.01f);
            if (shape_.shape == ATTACK_HIT_SHAPE::CAPSULE)
                rebuild_ |= ImGui::DragFloat("Half Height", &shape_.halfHeight, 0.01f, 0.0f);
            ImGui::DragFloat("Active Start", &shape_.activeStartTime, 0.01f, 0.0f);
            ImGui::DragFloat("Active End", &shape_.activeEndTime, 0.01f, shape_.activeStartTime);
            ImGui::DragFloat("Hit Interval", &shape_.hitInterval, 0.01f, 0.0f);
            ImGui::Checkbox("Destroy On Hit", &shape_.destroyOnHit);
            ImGui::Checkbox("Allow Multi Hit", &shape_.allowMultiHit);
            if (shape_.shape == ATTACK_HIT_SHAPE::SPHERE_GROUP)
            {
                // SphereGroupは一つの攻撃テンプレート内に、複数の局所球判定を並べるための設定。
                if (ImGui::Button("Add Sphere"))
                {
                    shape_.spheres.emplace_back();
                    rebuild_ = true;
                }
                int removeSphereIndex_ = -1;
                for (int sphereIndex_ = 0; sphereIndex_ < static_cast<int>(shape_.spheres.size()); ++sphereIndex_)
                {
                    ImGui::PushID(sphereIndex_);
                    ImGui::Text("Sphere %d", sphereIndex_ + 1);
                    ImGui::SameLine();
                    rebuild_ |= ImGui::DragFloat3("Offset", &shape_.spheres[sphereIndex_].localOffset.x, 0.01f);
                    rebuild_ |= ImGui::DragFloat("Sphere Radius", &shape_.spheres[sphereIndex_].radius, 0.01f, 0.01f);
                    if (ImGui::Button("Remove Sphere")) removeSphereIndex_ = sphereIndex_;
                    ImGui::PopID();
                }
                if (removeSphereIndex_ >= 0)
                {
                    shape_.spheres.erase(shape_.spheres.begin() + removeSphereIndex_);
                    rebuild_ = true;
                }
            }
            if (ImGui::Button("Remove Hit Shape")) removeIndex_ = shapeIndex_;
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (removeIndex_ >= 0)
    {
        hitShapes.erase(hitShapes.begin() + removeIndex_);
        rebuild_ = true;
    }
    if (rebuild_) RebuildHitShapes();
}

void AttackEffectHitComponent::Serialize(nlohmann::json& json_) const
{
    // 攻撃共通値とヒット時演出、さらに複数HitShapeのテンプレートをまとめて保存する。
    json_["EffectType"] = static_cast<int>(effectType);
    json_["LifeTime"] = lifeTime;
    json_["Damage"] = damage;
    json_["DamageRate"] = damageRate;
    json_["FixedDamage"] = fixedDamage;
    json_["DamageApplyType"] = static_cast<int>(damageApplyType);
    json_["DamageHitInterval"] = damageHitInterval;
    json_["MaxHitCount"] = maxHitCount;
    json_["ImpactPower"] = impactPower;
    json_["AttackInstanceId"] = attackInstanceId;
    json_["HitGroupId"] = hitGroupId;
    json_["HitStopDuration"] = hitStopDuration;
    json_["HitStopScale"] = hitStopScale;
    json_["KnockBackPower"] = knockBackPower;
    json_["TargetAirLiftPower"] = targetAirLiftPower;
    json_["AttackerAirLiftPower"] = attackerAirLiftPower;
    json_["HitParticleAssetPath"] = hitParticleAssetPath;
    json_["HitParticleEmitterName"] = hitParticleEmitterName;
    json_["HitParticleLifeTime"] = hitParticleLifeTime;
    json_["HitParticleNormalOffset"] = hitParticleNormalOffset;
    json_["HitParticleUseImpactAxis"] = hitParticleUseImpactAxis;
    json_["HitParticleAxisToAttackerBlend"] = hitParticleAxisToAttackerBlend;
    json_["HitParticleUseTargetAttraction"] = hitParticleUseTargetAttraction;
    json_["HitParticleTargetAttractionStrength"] = hitParticleTargetAttractionStrength;
    json_["HitParticleTargetAttractionRange"] = hitParticleTargetAttractionRange;
    json_["HitParticleTargetAttractionFalloff"] = hitParticleTargetAttractionFalloff;
    json_["HitMeshVFXAssetPath"] = hitMeshVFXAssetPath;
    json_["HitMeshVFXLifeTime"] = hitMeshVFXLifeTime;
    json_["HitMeshVFXNormalOffset"] = hitMeshVFXNormalOffset;
    json_["DestroyOnLifeEnd"] = destroyOnLifeEnd;
    json_["CanHitOwner"] = canHitOwner;
    json_["AllowMultiHit"] = allowMultiHit;
    json_["MovementDirection"] = { movementDirection.x, movementDirection.y, movementDirection.z };
    json_["MovementSpeed"] = movementSpeed;
    json_["HitShapes"] = nlohmann::json::array();
    for (const AttackEffectHitShapeTemplate& shape_ : hitShapes)
    {
        nlohmann::json spheres_ = nlohmann::json::array();
        for (const AttackEffectSphereTemplate& sphere_ : shape_.spheres)
            spheres_.push_back({ { "LocalOffset", { sphere_.localOffset.x, sphere_.localOffset.y, sphere_.localOffset.z } }, { "Radius", sphere_.radius } });
        json_["HitShapes"].push_back({
            { "Enabled", shape_.enabled }, { "HitBoxName", shape_.hitBoxName },
            { "Shape", static_cast<int>(NormalizeAttackHitShape(shape_.shape)) },
            { "LocalOffset", { shape_.localOffset.x, shape_.localOffset.y, shape_.localOffset.z } },
            { "LocalRotation", { shape_.localRotation.x, shape_.localRotation.y, shape_.localRotation.z } },
            { "LocalScale", { shape_.localScale.x, shape_.localScale.y, shape_.localScale.z } },
            { "Radius", shape_.radius }, { "HalfHeight", shape_.halfHeight },
            { "Range", shape_.range }, { "Angle", shape_.angle },
            { "ActiveStartTime", shape_.activeStartTime }, { "ActiveEndTime", shape_.activeEndTime },
            { "HitInterval", shape_.hitInterval }, { "DestroyOnHit", shape_.destroyOnHit },
            { "AllowMultiHit", shape_.allowMultiHit }, { "Spheres", spheres_ }
        });
    }
}

void AttackEffectHitComponent::Deserialize(const nlohmann::json& json_)
{
    // JSONから読み込む値は、範囲外の形状や負数になりやすい項目を丸めながら復元する。
    effectType = static_cast<ATTACK_EFFECT_TYPE>((std::clamp)(json_.value("EffectType", 0), 0, 4));
    lifeTime = json_.value("LifeTime", 1.0f);
    damage = json_.value("Damage", 1.0f);
    damageRate = (std::max)(json_.value("DamageRate", damage / 100.0f), 0.0f);
    fixedDamage = json_.value("FixedDamage", 0.0f);
    damageApplyType = static_cast<DAMAGE_APPLY_TYPE>(
        (std::clamp)(json_.value("DamageApplyType", 0), 0, 2));
    damageHitInterval = (std::max)(json_.value("DamageHitInterval", 0.0f), 0.0f);
    maxHitCount = json_.value("MaxHitCount", 1);
    impactPower = (std::max)(json_.value("ImpactPower", damage), 0.0f);
    attackInstanceId = json_.value("AttackInstanceId", 0u);
    hitGroupId = json_.value("HitGroupId", 0u);
    hitStopDuration = json_.value("HitStopDuration", 0.0f);
    hitStopScale = json_.value("HitStopScale", 0.0f);
    knockBackPower = json_.value("KnockBackPower", 0.0f);
    targetAirLiftPower = (std::max)(json_.value("TargetAirLiftPower", 0.0f), 0.0f);
    attackerAirLiftPower = (std::max)(json_.value("AttackerAirLiftPower", 0.0f), 0.0f);
    hitParticleAssetPath = json_.value("HitParticleAssetPath", std::string{});
    hitParticleEmitterName = json_.value("HitParticleEmitterName", std::string{});
    hitParticleLifeTime = (std::max)(json_.value("HitParticleLifeTime", 1.0f), 0.0f);
    hitParticleNormalOffset = json_.value("HitParticleNormalOffset", 0.0f);
    hitParticleUseImpactAxis = json_.value("HitParticleUseImpactAxis", false);
    hitParticleAxisToAttackerBlend = std::clamp(
        json_.value("HitParticleAxisToAttackerBlend", 0.0f),
        0.0f,
        1.0f);
    hitParticleUseTargetAttraction = json_.value("HitParticleUseTargetAttraction", false);
    hitParticleTargetAttractionStrength =
        json_.value("HitParticleTargetAttractionStrength", 0.0f);
    hitParticleTargetAttractionRange =
        (std::max)(json_.value("HitParticleTargetAttractionRange", 0.0f), 0.0f);
    hitParticleTargetAttractionFalloff =
        (std::max)(json_.value("HitParticleTargetAttractionFalloff", 1.0f), 0.001f);
    hitMeshVFXAssetPath = json_.value("HitMeshVFXAssetPath", std::string{});
    hitMeshVFXLifeTime = (std::max)(json_.value("HitMeshVFXLifeTime", 1.0f), 0.0f);
    hitMeshVFXNormalOffset = json_.value("HitMeshVFXNormalOffset", 0.0f);
    destroyOnLifeEnd = json_.value("DestroyOnLifeEnd", true);
    canHitOwner = json_.value("CanHitOwner", false);
    allowMultiHit = json_.value("AllowMultiHit", false);
    movementSpeed = json_.value("MovementSpeed", 0.0f);
    const nlohmann::json movementDirection_ = json_.value("MovementDirection", nlohmann::json::array());
    if (movementDirection_.is_array() && movementDirection_.size() >= 3u)
        movementDirection = {
            movementDirection_[0].get<float>(),
            movementDirection_[1].get<float>(),
            movementDirection_[2].get<float>() };
    hitShapes.clear();
    for (const nlohmann::json& shapeJson_ : json_.value("HitShapes", nlohmann::json::array()))
    {
        // Shapeごとの局所Transformと有効時間を読み、最後にRebuildHitShapesで実体化する。
        AttackEffectHitShapeTemplate shape_{};
        shape_.enabled = shapeJson_.value("Enabled", true);
        shape_.hitBoxName = shapeJson_.value("HitBoxName", std::string("AttackEffectHitBox"));
        shape_.shape = NormalizeAttackHitShape(
            static_cast<ATTACK_HIT_SHAPE>((std::clamp)(shapeJson_.value("Shape", 1), 0, 7)));
        auto readVector_ = [&shapeJson_](const char* key_, const DirectX::XMFLOAT3& fallback_)
            {
                const nlohmann::json value_ = shapeJson_.value(key_, nlohmann::json::array());
                if (!value_.is_array() || value_.size() < 3u) return fallback_;
                return DirectX::XMFLOAT3{ value_[0].get<float>(), value_[1].get<float>(), value_[2].get<float>() };
            };
        shape_.localOffset = readVector_("LocalOffset", shape_.localOffset);
        shape_.localRotation = readVector_("LocalRotation", shape_.localRotation);
        shape_.localScale = readVector_("LocalScale", shape_.localScale);
        shape_.radius = shapeJson_.value("Radius", 0.5f);
        shape_.halfHeight = shapeJson_.value("HalfHeight", 1.0f);
        shape_.range = shapeJson_.value("Range", 5.0f);
        shape_.angle = shapeJson_.value("Angle", 45.0f);
        shape_.activeStartTime = shapeJson_.value("ActiveStartTime", 0.0f);
        shape_.activeEndTime = shapeJson_.value("ActiveEndTime", 0.1f);
        shape_.hitInterval = shapeJson_.value("HitInterval", 0.0f);
        shape_.destroyOnHit = shapeJson_.value("DestroyOnHit", false);
        shape_.allowMultiHit = shapeJson_.value("AllowMultiHit", false);
        for (const nlohmann::json& sphereJson_ : shapeJson_.value("Spheres", nlohmann::json::array()))
        {
            AttackEffectSphereTemplate sphere_{};
            const nlohmann::json offset_ = sphereJson_.value("LocalOffset", nlohmann::json::array());
            if (offset_.is_array() && offset_.size() >= 3u)
                sphere_.localOffset = { offset_[0].get<float>(), offset_[1].get<float>(), offset_[2].get<float>() };
            sphere_.radius = sphereJson_.value("Radius", 0.5f);
            shape_.spheres.push_back(sphere_);
        }
        hitShapes.push_back(shape_);
    }
    RebuildHitShapes();
}

REGISTER_COMPONENT(AttackEffectHitComponent, "AttackEffectHitComponent")
