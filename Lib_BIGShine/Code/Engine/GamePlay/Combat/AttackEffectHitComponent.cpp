#include "AttackEffectHitComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/Physics/HitBox.h"

#include <algorithm>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <nlohmann/json.hpp>

const std::string& AttackEffectHitComponent::GetName() const
{
    static const std::string name_ = "AttackEffectHitComponent";
    return name_;
}

void AttackEffectHitComponent::Start()
{
    RebuildHitShapes();
}

void AttackEffectHitComponent::Update(float deltaTime_)
{
    if (!playing) return;
    const float safeDeltaTime_ = (std::max)(deltaTime_, 0.0f);
    elapsedTime += safeDeltaTime_;
    hitIntervalTimer -= safeDeltaTime_;

    const std::shared_ptr<GameObject> object_ = GetGameObject();
    if (object_ && movementSpeed != 0.0f)
    {
        object_->transform->AddMoveOffset({
            movementDirection.x * movementSpeed * safeDeltaTime_,
            movementDirection.y * movementSpeed * safeDeltaTime_,
            movementDirection.z * movementSpeed * safeDeltaTime_ });
    }

    bool intervalReady_ = false;
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
        Stop();
        if (destroyOnLifeEnd && object_) object_->GetManager().Destroy(object_);
    }
}

void AttackEffectHitComponent::SetOwner(GameObject* owner_) { ownerObject = owner_; }
void AttackEffectHitComponent::SetAttackInstanceId(uint32_t attackInstanceId_) { attackInstanceId = attackInstanceId_; }
void AttackEffectHitComponent::SetHitGroupId(uint32_t hitGroupId_) { hitGroupId = hitGroupId_; }
void AttackEffectHitComponent::SetDamage(float damage_) { damage = damage_; }
void AttackEffectHitComponent::SetHitStopDuration(float hitStopDuration_) { hitStopDuration = hitStopDuration_; }
void AttackEffectHitComponent::SetHitStopScale(float hitStopScale_) { hitStopScale = hitStopScale_; }
void AttackEffectHitComponent::SetKnockBackPower(float knockBackPower_) { knockBackPower = knockBackPower_; }
void AttackEffectHitComponent::SetHitParticleAssetPath(const std::string& path_) { hitParticleAssetPath = path_; }
void AttackEffectHitComponent::SetHitMeshVFXAssetPath(const std::string& path_) { hitMeshVFXAssetPath = path_; }
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
    hitShapes = hitShapes_;
    RebuildHitShapes();
}

void AttackEffectHitComponent::Play()
{
    ResetRuntime();
    playing = true;
}

void AttackEffectHitComponent::Stop()
{
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

void AttackEffectHitComponent::ClearHitHistory()
{
    const std::shared_ptr<GameObject> object_ = GetGameObject();
    if (object_ && attackInstanceId != 0u)
        object_->GetManager().GetAttackSystem().ClearAttackHitGroup(attackInstanceId, hitGroupId);
    for (RuntimeHitShape& runtime_ : runtimeHitShapes)
        if (runtime_.hitBox) runtime_.hitBox->ClearHitTargets();
}

void AttackEffectHitComponent::ConfigureHitBox(
    const std::shared_ptr<HitBox>& hitBox_,
    const AttackEffectHitShapeTemplate& shape_,
    const std::string& name_)
{
    hitBox_->actionPrefix = name_;
    hitBox_->SetOwner(ownerObject);
    hitBox_->SetAttackInstanceId(attackInstanceId);
    hitBox_->SetHitGroupId(hitGroupId);
    hitBox_->SetDamage(damage);
    hitBox_->hitStopDuration = hitStopDuration;
    hitBox_->hitStopScale = hitStopScale;
    hitBox_->SetKnockBackPower(knockBackPower);
    hitBox_->hitParticleAssetPath = hitParticleAssetPath;
    hitBox_->hitMeshVFXAssetPath = hitMeshVFXAssetPath;
    hitBox_->allowMultiHit = allowMultiHit || shape_.allowMultiHit;
    hitBox_->canHitOwner = canHitOwner;
    hitBox_->radius = (std::max)(shape_.radius, 0.01f);
    hitBox_->halfHeight = shape_.shape == ATTACK_HIT_SHAPE::SPHERE ? 0.0f : (std::max)(shape_.halfHeight, 0.0f);
    hitBox_->Deactivate();
    hitBox_->RecreatePhysXActor();
}

void AttackEffectHitComponent::RebuildHitShapes()
{
    for (RuntimeHitShape& runtime_ : runtimeHitShapes)
        if (runtime_.object) runtime_.object->GetManager().Destroy(runtime_.object);
    runtimeHitShapes.clear();
    const std::shared_ptr<GameObject> effectObject_ = GetGameObject();
    if (!effectObject_) return;

    for (size_t shapeIndex_ = 0; shapeIndex_ < hitShapes.size(); ++shapeIndex_)
    {
        const AttackEffectHitShapeTemplate& shape_ = hitShapes[shapeIndex_];
        const size_t sphereCount_ = shape_.shape == ATTACK_HIT_SHAPE::SPHERE_GROUP
            ? shape_.spheres.size() : 1u;
        for (size_t sphereIndex_ = 0; sphereIndex_ < sphereCount_; ++sphereIndex_)
        {
            const std::shared_ptr<GameObject> hitObject_ = effectObject_->GetManager().Create("RuntimeAttackHitShape");
            if (!hitObject_) continue;
            hitObject_->SetNoSaveObject();
            hitObject_->transform->SetParent(effectObject_->transform);
            DirectX::XMFLOAT3 offset_ = shape_.localOffset;
            float radius_ = shape_.radius;
            if (shape_.shape == ATTACK_HIT_SHAPE::SPHERE_GROUP && sphereIndex_ < shape_.spheres.size())
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
            runtimeShape_.radius = radius_;
            ConfigureHitBox(hitBox_, runtimeShape_, shape_.hitBoxName + std::to_string(sphereIndex_));
            runtimeHitShapes.push_back({ hitObject_, hitBox_, shapeIndex_ });
        }
    }
}

void AttackEffectHitComponent::Gui()
{
    int effectType_ = static_cast<int>(effectType);
    const char* effectNames_[] = { "Instant Area", "Projectile", "Area Over Time", "Beam", "Breath" };
    if (ImGui::Combo("Effect Type", &effectType_, effectNames_, IM_ARRAYSIZE(effectNames_)))
        effectType = static_cast<ATTACK_EFFECT_TYPE>(effectType_);
    ImGui::DragFloat("Life Time", &lifeTime, 0.01f, 0.0f);
    ImGui::Text("Elapsed: %.3f", elapsedTime);
    ImGui::Checkbox("Playing", &playing);
    ImGui::DragFloat("Damage", &damage, 0.1f, 0.0f);
    ImGui::DragFloat("Hit Stop Duration", &hitStopDuration, 0.001f, 0.0f);
    ImGui::DragFloat("Hit Stop Scale", &hitStopScale, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Knock Back Power", &knockBackPower, 0.1f, 0.0f);
    ImGui::InputText("Hit Particle", &hitParticleAssetPath);
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
            rebuild_ |= ImGui::Checkbox("Enabled", &shape_.enabled);
            rebuild_ |= ImGui::InputText("HitBox Name", &shape_.hitBoxName);
            int shapeType_ = static_cast<int>(shape_.shape);
            const char* shapeNames_[] = { "None", "Sphere", "Box", "Capsule", "Sphere Group", "Fan", "Cone", "Ray" };
            if (ImGui::Combo("Shape", &shapeType_, shapeNames_, IM_ARRAYSIZE(shapeNames_)))
            {
                shape_.shape = static_cast<ATTACK_HIT_SHAPE>(shapeType_);
                rebuild_ = true;
            }
            rebuild_ |= ImGui::DragFloat3("Local Offset", &shape_.localOffset.x, 0.01f);
            rebuild_ |= ImGui::DragFloat3("Local Rotation", &shape_.localRotation.x, 0.1f);
            rebuild_ |= ImGui::DragFloat3("Local Scale", &shape_.localScale.x, 0.01f);
            rebuild_ |= ImGui::DragFloat("Radius", &shape_.radius, 0.01f, 0.01f);
            rebuild_ |= ImGui::DragFloat("Half Height", &shape_.halfHeight, 0.01f, 0.0f);
            ImGui::DragFloat("Range", &shape_.range, 0.01f, 0.0f);
            ImGui::DragFloat("Angle", &shape_.angle, 0.1f, 0.0f, 360.0f);
            ImGui::DragFloat("Active Start", &shape_.activeStartTime, 0.01f, 0.0f);
            ImGui::DragFloat("Active End", &shape_.activeEndTime, 0.01f, shape_.activeStartTime);
            ImGui::DragFloat("Hit Interval", &shape_.hitInterval, 0.01f, 0.0f);
            ImGui::Checkbox("Destroy On Hit", &shape_.destroyOnHit);
            ImGui::Checkbox("Allow Multi Hit", &shape_.allowMultiHit);
            if (shape_.shape == ATTACK_HIT_SHAPE::SPHERE_GROUP)
            {
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
    json_["EffectType"] = static_cast<int>(effectType);
    json_["LifeTime"] = lifeTime;
    json_["Damage"] = damage;
    json_["AttackInstanceId"] = attackInstanceId;
    json_["HitGroupId"] = hitGroupId;
    json_["HitStopDuration"] = hitStopDuration;
    json_["HitStopScale"] = hitStopScale;
    json_["KnockBackPower"] = knockBackPower;
    json_["HitParticleAssetPath"] = hitParticleAssetPath;
    json_["HitMeshVFXAssetPath"] = hitMeshVFXAssetPath;
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
            { "Shape", static_cast<int>(shape_.shape) },
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
    effectType = static_cast<ATTACK_EFFECT_TYPE>((std::clamp)(json_.value("EffectType", 0), 0, 4));
    lifeTime = json_.value("LifeTime", 1.0f);
    damage = json_.value("Damage", 1.0f);
    attackInstanceId = json_.value("AttackInstanceId", 0u);
    hitGroupId = json_.value("HitGroupId", 0u);
    hitStopDuration = json_.value("HitStopDuration", 0.0f);
    hitStopScale = json_.value("HitStopScale", 0.0f);
    knockBackPower = json_.value("KnockBackPower", 0.0f);
    hitParticleAssetPath = json_.value("HitParticleAssetPath", std::string{});
    hitMeshVFXAssetPath = json_.value("HitMeshVFXAssetPath", std::string{});
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
        AttackEffectHitShapeTemplate shape_{};
        shape_.enabled = shapeJson_.value("Enabled", true);
        shape_.hitBoxName = shapeJson_.value("HitBoxName", std::string("AttackEffectHitBox"));
        shape_.shape = static_cast<ATTACK_HIT_SHAPE>((std::clamp)(shapeJson_.value("Shape", 1), 0, 7));
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
