#include "PrecompiledHeader.h"

#include "EffectEmitterComponent.h"

#include "Editor/AssetSlotGui.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/System/AttackSystem.h"
#include "Engine/GamePlay/System/EffectManager.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <nlohmann/json.hpp>

namespace
{
    DirectX::XMFLOAT3 ReadVector3(
        const nlohmann::json& json_,
        const char* key_,
        const DirectX::XMFLOAT3& fallback_)
    {
        const nlohmann::json value_ = json_.value(key_, nlohmann::json::array());
        if (!value_.is_array() || value_.size() < 3u) return fallback_;
        return { value_[0].get<float>(), value_[1].get<float>(), value_[2].get<float>() };
    }

    nlohmann::json WriteVector3(const DirectX::XMFLOAT3& value_)
    {
        return { value_.x, value_.y, value_.z };
    }

    DirectX::XMFLOAT3 NormalizeDirection(const DirectX::XMFLOAT3& direction_)
    {
        DirectX::XMVECTOR vector_ = DirectX::XMLoadFloat3(&direction_);
        if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(vector_)) <= 0.000001f)
        {
            return { 0.0f, 0.0f, 1.0f };
        }
        DirectX::XMFLOAT3 result_{};
        DirectX::XMStoreFloat3(&result_, DirectX::XMVector3Normalize(vector_));
        return result_;
    }

    DirectX::XMFLOAT3 TransformForward(const DirectX::XMMATRIX& matrix_)
    {
        DirectX::XMFLOAT3 direction_{};
        DirectX::XMStoreFloat3(
            &direction_,
            DirectX::XMVector3TransformNormal(
                DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
                matrix_));
        return NormalizeDirection(direction_);
    }

    DirectX::XMFLOAT3 ExtractPosition(const DirectX::XMMATRIX& matrix_)
    {
        DirectX::XMFLOAT3 position_{};
        DirectX::XMStoreFloat3(&position_, matrix_.r[3]);
        return position_;
    }

    const char* GetEntryTypeName(EFFECT_EMITTER_ENTRY_TYPE type_)
    {
        static const char* names_[] = { "Particle", "Mesh VFX / Sequence", "Impact Effect" };
        return names_[std::clamp(static_cast<int>(type_), 0, 2)];
    }

    const char* GetShapeTypeName(EFFECT_EMITTER_SHAPE_TYPE type_)
    {
        static const char* names_[] = { "Area", "Projectile", "Beam", "Fan" };
        return names_[std::clamp(static_cast<int>(type_), 0, 3)];
    }
}

void to_json(nlohmann::json& json_, const EffectEmitterEntryTemplate& entry_)
{
    json_ =
    {
        { "Type", static_cast<int>(entry_.type) },
        { "Enabled", entry_.enabled },
        { "AssetPath", entry_.assetPath },
        { "EmitterName", entry_.emitterName },
        { "LocalOffset", WriteVector3(entry_.localOffset) },
        { "LocalRotation", WriteVector3(entry_.localRotation) },
        { "LocalScale", WriteVector3(entry_.localScale) },
        { "LifeTime", entry_.lifeTime },
        { "NormalOffset", entry_.normalOffset },
        { "PlaybackSpeed", entry_.playbackSpeed },
        { "FollowOwner", entry_.followOwner },
        { "OverrideMeshVFXColorRamp", entry_.overrideMeshVFXColorRamp },
        { "ColorRampTexturePath", entry_.colorRampTexturePath },
        { "ForceAdditiveBlend", entry_.forceAdditiveBlend },
        { "GlowMultiplier", entry_.glowMultiplier },
    };
}

void from_json(const nlohmann::json& json_, EffectEmitterEntryTemplate& entry_)
{
    entry_.type = static_cast<EFFECT_EMITTER_ENTRY_TYPE>(std::clamp(json_.value("Type", 0), 0, 2));
    entry_.enabled = json_.value("Enabled", true);
    entry_.assetPath = json_.value("AssetPath", std::string{});
    entry_.emitterName = json_.value("EmitterName", std::string{});
    entry_.localOffset = ReadVector3(json_, "LocalOffset", entry_.localOffset);
    entry_.localRotation = ReadVector3(json_, "LocalRotation", entry_.localRotation);
    entry_.localScale = ReadVector3(json_, "LocalScale", entry_.localScale);
    entry_.lifeTime = (std::max)(json_.value("LifeTime", 1.0f), 0.01f);
    entry_.normalOffset = json_.value("NormalOffset", 0.0f);
    entry_.playbackSpeed = (std::max)(json_.value("PlaybackSpeed", 1.0f), 0.0f);
    entry_.followOwner = json_.value("FollowOwner", false);
    entry_.overrideMeshVFXColorRamp = json_.value("OverrideMeshVFXColorRamp", false);
    entry_.colorRampTexturePath = json_.value("ColorRampTexturePath", std::string{});
    entry_.forceAdditiveBlend = json_.value("ForceAdditiveBlend", false);
    entry_.glowMultiplier = (std::max)(json_.value("GlowMultiplier", 1.0f), 0.0f);
}

void to_json(nlohmann::json& json_, const EffectEmitterHitTemplate& hit_)
{
    json_ =
    {
        { "Enabled", hit_.enabled },
        { "Damage", hit_.damage },
        { "ImpactPower", hit_.impactPower },
        { "KnockBackPower", hit_.knockBackPower },
        { "TargetAirLiftPower", hit_.targetAirLiftPower },
        { "AttackerAirLiftPower", hit_.attackerAirLiftPower },
        { "HitStopDuration", hit_.hitStopDuration },
        { "HitStopScale", hit_.hitStopScale },
        { "HitGroupId", hit_.hitGroupId },
        { "AllowMultiHit", hit_.allowMultiHit },
        { "CanHitOwner", hit_.canHitOwner },
        { "DestroyOnHit", hit_.destroyOnHit },
        { "HitParticleAssetPath", hit_.hitParticleAssetPath },
        { "HitParticleEmitterName", hit_.hitParticleEmitterName },
        { "HitParticleLifeTime", hit_.hitParticleLifeTime },
        { "HitParticleNormalOffset", hit_.hitParticleNormalOffset },
        { "HitParticleUseImpactAxis", hit_.hitParticleUseImpactAxis },
        { "HitParticleAxisToAttackerBlend", hit_.hitParticleAxisToAttackerBlend },
        { "HitParticleUseTargetAttraction", hit_.hitParticleUseTargetAttraction },
        { "HitParticleTargetAttractionStrength", hit_.hitParticleTargetAttractionStrength },
        { "HitParticleTargetAttractionRange", hit_.hitParticleTargetAttractionRange },
        { "HitParticleTargetAttractionFalloff", hit_.hitParticleTargetAttractionFalloff },
        { "HitMeshVFXAssetPath", hit_.hitMeshVFXAssetPath },
        { "HitMeshVFXLifeTime", hit_.hitMeshVFXLifeTime },
        { "HitMeshVFXNormalOffset", hit_.hitMeshVFXNormalOffset },
        { "CameraShakeEnabled", hit_.cameraShakeEnabled },
        { "CameraShakeReceiveDuration", hit_.cameraShakeReceiveDuration },
        { "CameraShakeReceivePositionStrength", hit_.cameraShakeReceivePositionStrength },
        { "CameraShakeReceiveRotationStrength", hit_.cameraShakeReceiveRotationStrength },
        { "CameraShakeAttackScale", hit_.cameraShakeAttackScale },
        { "CameraShakeFrequency", hit_.cameraShakeFrequency },
        { "CameraShakePriority", hit_.cameraShakePriority },
    };
}

void from_json(const nlohmann::json& json_, EffectEmitterHitTemplate& hit_)
{
    hit_.enabled = json_.value("Enabled", true);
    hit_.damage = json_.value("Damage", 10.0f);
    hit_.impactPower = (std::max)(json_.value("ImpactPower", 10.0f), 0.0f);
    hit_.knockBackPower = json_.value("KnockBackPower", 3.0f);
    hit_.targetAirLiftPower = json_.value("TargetAirLiftPower", 0.0f);
    hit_.attackerAirLiftPower = json_.value("AttackerAirLiftPower", 0.0f);
    hit_.hitStopDuration = (std::max)(json_.value("HitStopDuration", 0.0f), 0.0f);
    hit_.hitStopScale = std::clamp(json_.value("HitStopScale", 0.0f), 0.0f, 1.0f);
    hit_.hitGroupId = json_.value("HitGroupId", 0u);
    hit_.allowMultiHit = json_.value("AllowMultiHit", false);
    hit_.canHitOwner = json_.value("CanHitOwner", false);
    hit_.destroyOnHit = json_.value("DestroyOnHit", false);
    hit_.hitParticleAssetPath = json_.value("HitParticleAssetPath", std::string{});
    hit_.hitParticleEmitterName = json_.value("HitParticleEmitterName", std::string{});
    hit_.hitParticleLifeTime = (std::max)(json_.value("HitParticleLifeTime", 1.0f), 0.01f);
    hit_.hitParticleNormalOffset = json_.value("HitParticleNormalOffset", 0.0f);
    hit_.hitParticleUseImpactAxis = json_.value("HitParticleUseImpactAxis", false);
    hit_.hitParticleAxisToAttackerBlend =
        std::clamp(json_.value("HitParticleAxisToAttackerBlend", 0.0f), 0.0f, 1.0f);
    hit_.hitParticleUseTargetAttraction = json_.value("HitParticleUseTargetAttraction", false);
    hit_.hitParticleTargetAttractionStrength =
        json_.value("HitParticleTargetAttractionStrength", 0.0f);
    hit_.hitParticleTargetAttractionRange =
        (std::max)(json_.value("HitParticleTargetAttractionRange", 0.0f), 0.0f);
    hit_.hitParticleTargetAttractionFalloff =
        (std::max)(json_.value("HitParticleTargetAttractionFalloff", 1.0f), 0.001f);
    hit_.hitMeshVFXAssetPath = json_.value("HitMeshVFXAssetPath", std::string{});
    hit_.hitMeshVFXLifeTime = (std::max)(json_.value("HitMeshVFXLifeTime", 1.0f), 0.01f);
    hit_.hitMeshVFXNormalOffset = json_.value("HitMeshVFXNormalOffset", 0.0f);
    hit_.cameraShakeEnabled = json_.value("CameraShakeEnabled", true);
    hit_.cameraShakeReceiveDuration = (std::max)(json_.value("CameraShakeReceiveDuration", 0.12f), 0.0f);
    hit_.cameraShakeReceivePositionStrength = (std::max)(json_.value("CameraShakeReceivePositionStrength", 0.025f), 0.0f);
    hit_.cameraShakeReceiveRotationStrength = (std::max)(json_.value("CameraShakeReceiveRotationStrength", 0.18f), 0.0f);
    hit_.cameraShakeAttackScale = (std::max)(json_.value("CameraShakeAttackScale", 0.5f), 0.0f);
    hit_.cameraShakeFrequency = (std::max)(json_.value("CameraShakeFrequency", 24.0f), 0.0f);
    hit_.cameraShakePriority = json_.value("CameraShakePriority", 5);
}

void to_json(nlohmann::json& json_, const EffectEmitterTemplate& template_)
{
    json_ =
    {
        { "Name", template_.name },
        { "ActionName", template_.actionName },
        { "ShapeType", static_cast<int>(template_.shapeType) },
        { "SourceNodeName", template_.sourceNodeName },
        { "LocalOffset", WriteVector3(template_.localOffset) },
        { "LocalRotation", WriteVector3(template_.localRotation) },
        { "LocalScale", WriteVector3(template_.localScale) },
        { "Range", template_.range },
        { "Radius", template_.radius },
        { "Speed", template_.speed },
        { "LifeTime", template_.lifeTime },
        { "FanAngleDegrees", template_.fanAngleDegrees },
        { "FanSampleCount", template_.fanSampleCount },
        { "FanRangeSteps", template_.fanRangeSteps },
        { "SpawnHit", template_.spawnHit },
        { "Entries", template_.entries },
        { "Hit", template_.hit },
    };
}

void from_json(const nlohmann::json& json_, EffectEmitterTemplate& template_)
{
    template_.name = json_.value("Name", std::string{ "EffectEmitter" });
    template_.actionName = json_.value("ActionName", std::string{ "EffectEmitter_Fire" });
    template_.shapeType = static_cast<EFFECT_EMITTER_SHAPE_TYPE>(std::clamp(json_.value("ShapeType", 1), 0, 3));
    template_.sourceNodeName = json_.value("SourceNodeName", std::string{});
    template_.localOffset = ReadVector3(json_, "LocalOffset", template_.localOffset);
    template_.localRotation = ReadVector3(json_, "LocalRotation", template_.localRotation);
    template_.localScale = ReadVector3(json_, "LocalScale", template_.localScale);
    template_.range = (std::max)(json_.value("Range", 8.0f), 0.01f);
    template_.radius = (std::max)(json_.value("Radius", 0.5f), 0.01f);
    template_.speed = (std::max)(json_.value("Speed", 8.0f), 0.0f);
    template_.lifeTime = (std::max)(json_.value("LifeTime", 1.0f), 0.01f);
    template_.fanAngleDegrees = std::clamp(json_.value("FanAngleDegrees", 45.0f), 0.0f, 180.0f);
    template_.fanSampleCount = std::clamp(json_.value("FanSampleCount", 5), 1, 31);
    template_.fanRangeSteps = std::clamp(json_.value("FanRangeSteps", 3), 1, 16);
    template_.spawnHit = json_.value("SpawnHit", true);
    template_.entries = json_.value("Entries", std::vector<EffectEmitterEntryTemplate>{});
    template_.hit = json_.value("Hit", EffectEmitterHitTemplate{});
}

void EffectEmitterComponent::Start()
{
    BS_PROFILE_SCOPE("EffectEmitterComponent::Start");
    if (templates.empty())
    {
        EffectEmitterTemplate template_{};
        template_.entries.push_back({});
        templates.push_back(template_);
    }
    RegisterActions();
}

void EffectEmitterComponent::RegisterActions()
{
    BS_PROFILE_SCOPE("EffectEmitterComponent::RegisterActions");
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    for (size_t index_ = 0; index_ < templates.size(); ++index_)
    {
        const std::string indexActionName_ = "EffectEmitter_" + std::to_string(index_);
        owner_->RegisterAction("EffectEmitter", indexActionName_, [this, index_]() { Emit(index_); });

        if (!templates[index_].actionName.empty())
        {
            owner_->RegisterAction(
                "EffectEmitter",
                templates[index_].actionName,
                [this, index_]() { Emit(index_); });
        }
        if (!templates[index_].name.empty())
        {
            owner_->RegisterAction(
                "EffectEmitter",
                templates[index_].name,
                [this, index_]() { Emit(index_); });
        }
    }
}

void EffectEmitterComponent::Emit(size_t templateIndex_)
{
    BS_PROFILE_SCOPE("EffectEmitterComponent::Emit");
    if (templateIndex_ >= templates.size()) return;
    const EffectEmitterTemplate& template_ = templates[templateIndex_];
    const EmitFrame frame_ = BuildEmitFrame(template_);
    std::shared_ptr<GameObject> hitObject_;
    if (template_.spawnHit && template_.hit.enabled)
    {
        hitObject_ = SpawnHitObject(template_, frame_);
    }

    for (const EffectEmitterEntryTemplate& entry_ : template_.entries)
    {
        SpawnEntry(template_, entry_, frame_, hitObject_);
    }
}

void EffectEmitterComponent::EmitByName(const std::string& templateName_)
{
    for (size_t index_ = 0; index_ < templates.size(); ++index_)
    {
        if (templates[index_].name == templateName_ ||
            templates[index_].actionName == templateName_)
        {
            Emit(index_);
            return;
        }
    }
}

EffectEmitterComponent::EmitFrame EffectEmitterComponent::BuildEmitFrame(
    const EffectEmitterTemplate& template_) const
{
    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_ || !owner_->transform) return {};

    DirectX::XMMATRIX sourceMatrix_ = owner_->transform->GetWorldMatrix();
    if (!template_.sourceNodeName.empty())
    {
        std::shared_ptr<GltfModelCommponennt> model_ = owner_->GetComponent<GltfModelCommponennt>();
        if (model_ && model_->IsModelReady())
        {
            const int nodeIndex_ = model_->GetNodeIndex(template_.sourceNodeName.c_str());
            if (nodeIndex_ >= 0 && nodeIndex_ < static_cast<int>(model_->GetNodes().size()))
            {
                sourceMatrix_ = DirectX::XMLoadFloat4x4(&model_->GetNodes()[nodeIndex_].worldTransform);
            }
        }
    }

    const DirectX::XMMATRIX localMatrix_ = BuildLocalMatrix(
        template_.localOffset,
        template_.localRotation,
        template_.localScale);
    EmitFrame frame_{};
    frame_.sourceMatrix = localMatrix_ * sourceMatrix_;
    frame_.position = ExtractPosition(frame_.sourceMatrix);
    frame_.direction = TransformForward(frame_.sourceMatrix);
    return frame_;
}

DirectX::XMMATRIX EffectEmitterComponent::BuildLocalMatrix(
    const DirectX::XMFLOAT3& position_,
    const DirectX::XMFLOAT3& rotation_,
    const DirectX::XMFLOAT3& scale_) const
{
    return DirectX::XMMatrixScaling(scale_.x, scale_.y, scale_.z) *
        DirectX::XMMatrixRotationRollPitchYaw(
            DirectX::XMConvertToRadians(rotation_.x),
            DirectX::XMConvertToRadians(rotation_.y),
            DirectX::XMConvertToRadians(rotation_.z)) *
        DirectX::XMMatrixTranslation(position_.x, position_.y, position_.z);
}

void EffectEmitterComponent::SpawnEntry(
    const EffectEmitterTemplate& template_,
    const EffectEmitterEntryTemplate& entry_,
    const EmitFrame& frame_,
    const std::shared_ptr<GameObject>& followTarget_)
{
    if (!entry_.enabled || entry_.assetPath.empty()) return;

    EffectManager& effectManager_ = GetManager().GetEffectManager();
    const DirectX::XMMATRIX entryMatrix_ = BuildLocalMatrix(
        entry_.localOffset,
        entry_.localRotation,
        entry_.localScale) * frame_.sourceMatrix;
    const DirectX::XMFLOAT3 position_ = ExtractPosition(entryMatrix_);
    const DirectX::XMFLOAT3 direction_ = TransformForward(entryMatrix_);
    std::shared_ptr<GameObject> spawnedObject_;

    if (entry_.type == EFFECT_EMITTER_ENTRY_TYPE::PARTICLE)
    {
        ParticleSpawnRequest request_{};
        request_.assetPath = entry_.assetPath;
        request_.emitterName = entry_.emitterName;
        request_.position = position_;
        request_.normal = direction_;
        request_.lifeTime = entry_.lifeTime;
        request_.normalOffset = entry_.normalOffset;
        spawnedObject_ = effectManager_.SpawnParticle(request_);
    }
    else if (entry_.type == EFFECT_EMITTER_ENTRY_TYPE::MESH_VFX)
    {
        MeshVFXSpawnRequest request_{};
        request_.assetPath = entry_.assetPath;
        request_.position = position_;
        request_.normal = direction_;
        request_.lifeTime = entry_.lifeTime;
        request_.normalOffset = entry_.normalOffset;
        spawnedObject_ = effectManager_.SpawnMeshVFX(request_);
        ApplyMeshVFXOverrides(spawnedObject_, entry_);
    }
    else if (entry_.type == EFFECT_EMITTER_ENTRY_TYPE::IMPACT_EFFECT)
    {
        effectManager_.SpawnImpactEffectAsset(entry_.assetPath, position_, direction_);
    }

    if (spawnedObject_ && spawnedObject_->transform)
    {
        if (template_.shapeType == EFFECT_EMITTER_SHAPE_TYPE::PROJECTILE &&
            followTarget_ &&
            followTarget_->transform)
        {
            spawnedObject_->transform->SetParent(followTarget_->transform);
        }
        spawnedObject_->transform->ApplyWorldMatrix(entryMatrix_);
        spawnedObject_->UpdateTransform();
        if (std::shared_ptr<MeshVFXComponent> meshVFX_ = spawnedObject_->GetComponent<MeshVFXComponent>())
        {
            meshVFX_->SetPlaybackSpeed(entry_.playbackSpeed);
        }
        if (std::shared_ptr<ParticleComponent> particle_ = spawnedObject_->GetComponent<ParticleComponent>())
        {
            if (!entry_.emitterName.empty()) particle_->PlayEmitter(entry_.emitterName);
        }
    }
}

std::shared_ptr<GameObject> EffectEmitterComponent::SpawnHitObject(
    const EffectEmitterTemplate& template_,
    const EmitFrame& frame_)
{
    BS_PROFILE_SCOPE("EffectEmitterComponent::SpawnHitObject");
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return nullptr;

    std::shared_ptr<GameObject> hitObject_ = GetManager().Create("RuntimeEffectEmitterHit");
    if (!hitObject_) return nullptr;
    hitObject_->SetNoSaveObject();
    hitObject_->transform->ApplyWorldMatrix(frame_.sourceMatrix);
    hitObject_->UpdateTransform();

    std::shared_ptr<AttackEffectHitComponent> attackHit_ =
        hitObject_->AddComponent<AttackEffectHitComponent>();
    if (!attackHit_) return hitObject_;

    const EffectEmitterHitTemplate& hit_ = template_.hit;
    attackHit_->SetOwner(owner_.get());
    attackHit_->SetAttackInstanceId(GetManager().GetAttackSystem().CreateAttackInstance());
    attackHit_->SetHitGroupId(hit_.hitGroupId);
    attackHit_->SetDamage(hit_.damage);
    attackHit_->SetImpactPower(hit_.impactPower);
    attackHit_->SetHitStopDuration(hit_.hitStopDuration);
    attackHit_->SetHitStopScale(hit_.hitStopScale);
    attackHit_->SetKnockBackPower(hit_.knockBackPower);
    attackHit_->SetAirLiftPower(hit_.targetAirLiftPower, hit_.attackerAirLiftPower);
    attackHit_->SetCameraShake(
        hit_.cameraShakeEnabled,
        hit_.cameraShakeReceiveDuration,
        hit_.cameraShakeReceivePositionStrength,
        hit_.cameraShakeReceiveRotationStrength,
        hit_.cameraShakeAttackScale,
        hit_.cameraShakeFrequency,
        hit_.cameraShakePriority);
    attackHit_->SetHitParticleAssetPath(hit_.hitParticleAssetPath);
    attackHit_->SetHitParticleEmitterName(hit_.hitParticleEmitterName);
    attackHit_->SetHitParticleLifeTime(hit_.hitParticleLifeTime);
    attackHit_->SetHitParticleNormalOffset(hit_.hitParticleNormalOffset);
    attackHit_->SetHitParticleImpactAxis(
        hit_.hitParticleUseImpactAxis,
        hit_.hitParticleAxisToAttackerBlend);
    attackHit_->SetHitParticleTargetAttraction(
        hit_.hitParticleUseTargetAttraction,
        hit_.hitParticleTargetAttractionStrength,
        hit_.hitParticleTargetAttractionRange,
        hit_.hitParticleTargetAttractionFalloff);
    attackHit_->SetHitMeshVFXAssetPath(hit_.hitMeshVFXAssetPath);
    attackHit_->SetHitMeshVFXLifeTime(hit_.hitMeshVFXLifeTime);
    attackHit_->SetHitMeshVFXNormalOffset(hit_.hitMeshVFXNormalOffset);
    attackHit_->SetAllowMultiHit(hit_.allowMultiHit);
    attackHit_->SetCanHitOwner(hit_.canHitOwner);
    attackHit_->SetEffectType(template_.shapeType == EFFECT_EMITTER_SHAPE_TYPE::PROJECTILE
        ? ATTACK_EFFECT_TYPE::PROJECTILE
        : template_.shapeType == EFFECT_EMITTER_SHAPE_TYPE::BEAM
        ? ATTACK_EFFECT_TYPE::BEAM
        : template_.shapeType == EFFECT_EMITTER_SHAPE_TYPE::AREA
        ? ATTACK_EFFECT_TYPE::AREA_OVER_TIME
        : ATTACK_EFFECT_TYPE::BREATH);
    attackHit_->SetLifeTime(template_.lifeTime);
    attackHit_->SetDestroyOnLifeEnd(true);
    attackHit_->SetMovement(frame_.direction, template_.shapeType == EFFECT_EMITTER_SHAPE_TYPE::PROJECTILE ? template_.speed : 0.0f);
    attackHit_->SetHitShapes(BuildHitShapes(template_));
    attackHit_->Play();
    return hitObject_;
}

std::vector<AttackEffectHitShapeTemplate> EffectEmitterComponent::BuildHitShapes(
    const EffectEmitterTemplate& template_) const
{
    std::vector<AttackEffectHitShapeTemplate> shapes_;
    AttackEffectHitShapeTemplate shape_{};
    shape_.enabled = true;
    shape_.hitBoxName = template_.name.empty() ? "EffectEmitterHit" : template_.name;
    shape_.radius = (std::max)(template_.radius, 0.01f);
    shape_.activeStartTime = 0.0f;
    shape_.activeEndTime = template_.lifeTime;
    shape_.destroyOnHit = template_.hit.destroyOnHit;
    shape_.allowMultiHit = template_.hit.allowMultiHit;

    if (template_.shapeType == EFFECT_EMITTER_SHAPE_TYPE::AREA)
    {
        shape_.shape = ATTACK_HIT_SHAPE::SPHERE;
        shapes_.push_back(shape_);
    }
    else if (template_.shapeType == EFFECT_EMITTER_SHAPE_TYPE::PROJECTILE)
    {
        shape_.shape = ATTACK_HIT_SHAPE::SPHERE;
        shape_.localOffset = { 0.0f, 0.0f, 0.0f };
        shapes_.push_back(shape_);
    }
    else if (template_.shapeType == EFFECT_EMITTER_SHAPE_TYPE::BEAM)
    {
        shape_.shape = ATTACK_HIT_SHAPE::CAPSULE;
        shape_.localOffset = { 0.0f, 0.0f, template_.range * 0.5f };
        shape_.localRotation = { 90.0f, 0.0f, 0.0f };
        shape_.halfHeight = template_.range * 0.5f;
        shapes_.push_back(shape_);
    }
    else
    {
        shape_.shape = ATTACK_HIT_SHAPE::SPHERE_GROUP;
        const int sampleCount_ = std::clamp(template_.fanSampleCount, 1, 31);
        const int rangeSteps_ = std::clamp(template_.fanRangeSteps, 1, 16);
        const float halfAngle_ = DirectX::XMConvertToRadians(template_.fanAngleDegrees * 0.5f);
        for (int rangeIndex_ = 1; rangeIndex_ <= rangeSteps_; ++rangeIndex_)
        {
            const float rangeRate_ = static_cast<float>(rangeIndex_) / static_cast<float>(rangeSteps_);
            const float distance_ = template_.range * rangeRate_;
            for (int sampleIndex_ = 0; sampleIndex_ < sampleCount_; ++sampleIndex_)
            {
                const float sampleRate_ = sampleCount_ == 1
                    ? 0.5f
                    : static_cast<float>(sampleIndex_) / static_cast<float>(sampleCount_ - 1);
                const float angle_ = -halfAngle_ + halfAngle_ * 2.0f * sampleRate_;
                AttackEffectSphereTemplate sphere_{};
                sphere_.localOffset =
                {
                    std::sin(angle_) * distance_,
                    0.0f,
                    std::cos(angle_) * distance_,
                };
                sphere_.radius = template_.radius * (0.75f + 0.35f * rangeRate_);
                shape_.spheres.push_back(sphere_);
            }
        }
        shapes_.push_back(shape_);
    }
    return shapes_;
}

void EffectEmitterComponent::ApplyMeshVFXOverrides(
    const std::shared_ptr<GameObject>& object_,
    const EffectEmitterEntryTemplate& entry_) const
{
    if (!object_) return;
    std::shared_ptr<MeshVFXComponent> meshVFX_ = object_->GetComponent<MeshVFXComponent>();
    if (!meshVFX_) return;
    if (!entry_.overrideMeshVFXColorRamp && !entry_.forceAdditiveBlend && entry_.glowMultiplier == 1.0f) return;

    MeshVFXAsset asset_ = meshVFX_->GetAsset();
    for (MeshVFXPart& part_ : asset_.parts)
    {
        if (entry_.overrideMeshVFXColorRamp)
        {
            part_.gradientTexturePath = entry_.colorRampTexturePath;
            part_.useGradientTexture = !entry_.colorRampTexturePath.empty();
        }
        if (entry_.forceAdditiveBlend)
        {
            part_.blendMode = MeshVFXBlendMode::ADDITIVE;
        }
        part_.emissiveStrength *= (std::max)(entry_.glowMultiplier, 0.0f);
    }
    meshVFX_->SetAsset(asset_);
    meshVFX_->Reset();
    meshVFX_->Play();
}

void EffectEmitterComponent::Gui()
{
    BS_PROFILE_SCOPE("EffectEmitterComponent::Gui");
    if (ImGui::Button("Register Actions")) RegisterActions();
    ImGui::SameLine();
    if (ImGui::Button("Add Template"))
    {
        EffectEmitterTemplate template_{};
        template_.name = "EffectEmitter" + std::to_string(templates.size());
        template_.actionName = "EffectEmitter_" + std::to_string(templates.size());
        template_.entries.push_back({});
        templates.push_back(template_);
        RegisterActions();
    }

    int removeTemplateIndex_ = -1;
    static const char* shapeNames_[] = { "Area", "Projectile", "Beam", "Fan" };
    static const char* entryTypeNames_[] = { "Particle", "Mesh VFX / Sequence", "Impact Effect" };
    for (int templateIndex_ = 0; templateIndex_ < static_cast<int>(templates.size()); ++templateIndex_)
    {
        EffectEmitterTemplate& template_ = templates[templateIndex_];
        ImGui::PushID(templateIndex_);
        if (ImGui::TreeNode("Template", "%d: %s [%s]", templateIndex_, template_.name.c_str(), GetShapeTypeName(template_.shapeType)))
        {
            if (ImGui::Button("Emit")) Emit(static_cast<size_t>(templateIndex_));
            ImGui::SameLine();
            if (ImGui::Button("Remove")) removeTemplateIndex_ = templateIndex_;
            ImGui::InputText("Name", &template_.name);
            if (ImGui::InputText("Action Name", &template_.actionName)) RegisterActions();
            int shapeType_ = static_cast<int>(template_.shapeType);
            if (ImGui::Combo("Shape Type", &shapeType_, shapeNames_, IM_ARRAYSIZE(shapeNames_)))
            {
                template_.shapeType = static_cast<EFFECT_EMITTER_SHAPE_TYPE>(shapeType_);
            }
            ImGui::InputText("Source Node Name", &template_.sourceNodeName);
            ImGui::DragFloat3("Local Offset", &template_.localOffset.x, 0.01f);
            ImGui::DragFloat3("Local Rotation", &template_.localRotation.x, 0.1f);
            ImGui::DragFloat3("Local Scale", &template_.localScale.x, 0.01f, 0.001f, 100.0f);
            ImGui::DragFloat("Range", &template_.range, 0.1f, 0.01f, 1000.0f);
            ImGui::DragFloat("Radius", &template_.radius, 0.01f, 0.01f, 100.0f);
            ImGui::DragFloat("Speed", &template_.speed, 0.1f, 0.0f, 1000.0f);
            ImGui::DragFloat("Life Time", &template_.lifeTime, 0.01f, 0.01f, 120.0f);
            if (template_.shapeType == EFFECT_EMITTER_SHAPE_TYPE::FAN)
            {
                ImGui::DragFloat("Fan Angle", &template_.fanAngleDegrees, 0.5f, 0.0f, 180.0f);
                ImGui::SliderInt("Fan Samples", &template_.fanSampleCount, 1, 31);
                ImGui::SliderInt("Fan Range Steps", &template_.fanRangeSteps, 1, 16);
            }

            if (ImGui::TreeNode("Entries"))
            {
                int removeEntryIndex_ = -1;
                for (int entryIndex_ = 0; entryIndex_ < static_cast<int>(template_.entries.size()); ++entryIndex_)
                {
                    EffectEmitterEntryTemplate& entry_ = template_.entries[entryIndex_];
                    ImGui::PushID(entryIndex_);
                    if (ImGui::TreeNode("Entry", "%d: %s", entryIndex_, GetEntryTypeName(entry_.type)))
                    {
                        ImGui::Checkbox("Enabled", &entry_.enabled);
                        int entryType_ = static_cast<int>(entry_.type);
                        if (ImGui::Combo("Type", &entryType_, entryTypeNames_, IM_ARRAYSIZE(entryTypeNames_)))
                        {
                            entry_.type = static_cast<EFFECT_EMITTER_ENTRY_TYPE>(entryType_);
                            entry_.assetPath.clear();
                        }
                        AssetSlotGui::Draw("Effect JSON", AssetSlotType::JSON, entry_.assetPath);
                        if (entry_.type == EFFECT_EMITTER_ENTRY_TYPE::PARTICLE)
                        {
                            ImGui::InputText("Emitter Name", &entry_.emitterName);
                        }
                        ImGui::DragFloat3("Local Offset", &entry_.localOffset.x, 0.01f);
                        ImGui::DragFloat3("Local Rotation", &entry_.localRotation.x, 0.1f);
                        ImGui::DragFloat3("Local Scale", &entry_.localScale.x, 0.01f, 0.001f, 100.0f);
                        ImGui::DragFloat("Life Time", &entry_.lifeTime, 0.01f, 0.01f, 120.0f);
                        ImGui::DragFloat("Normal Offset", &entry_.normalOffset, 0.001f, -100.0f, 100.0f);
                        ImGui::DragFloat("Playback Speed", &entry_.playbackSpeed, 0.01f, 0.0f, 10.0f);
                        if (entry_.type == EFFECT_EMITTER_ENTRY_TYPE::MESH_VFX)
                        {
                            ImGui::Checkbox("Override Color Ramp", &entry_.overrideMeshVFXColorRamp);
                            if (entry_.overrideMeshVFXColorRamp)
                            {
                                AssetSlotGui::Draw("Color Ramp", AssetSlotType::TEXTURE, entry_.colorRampTexturePath);
                            }
                            ImGui::Checkbox("Force Additive Blend", &entry_.forceAdditiveBlend);
                            ImGui::DragFloat("Glow Multiplier", &entry_.glowMultiplier, 0.05f, 0.0f, 20.0f);
                        }
                        if (ImGui::Button("Remove Entry")) removeEntryIndex_ = entryIndex_;
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
                if (removeEntryIndex_ >= 0)
                {
                    template_.entries.erase(template_.entries.begin() + removeEntryIndex_);
                }
                if (ImGui::Button("Add Particle")) template_.entries.push_back({});
                ImGui::SameLine();
                if (ImGui::Button("Add Mesh VFX"))
                {
                    EffectEmitterEntryTemplate entry_{};
                    entry_.type = EFFECT_EMITTER_ENTRY_TYPE::MESH_VFX;
                    template_.entries.push_back(entry_);
                }
                ImGui::SameLine();
                if (ImGui::Button("Add Impact Effect"))
                {
                    EffectEmitterEntryTemplate entry_{};
                    entry_.type = EFFECT_EMITTER_ENTRY_TYPE::IMPACT_EFFECT;
                    template_.entries.push_back(entry_);
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Hit"))
            {
                ImGui::Checkbox("Spawn Hit", &template_.spawnHit);
                ImGui::Checkbox("Enabled", &template_.hit.enabled);
                ImGui::DragFloat("Damage", &template_.hit.damage, 0.1f, 0.0f, 10000.0f);
                ImGui::DragFloat("Impact Power", &template_.hit.impactPower, 0.1f, 0.0f, 10000.0f);
                ImGui::DragFloat("Knock Back Power", &template_.hit.knockBackPower, 0.1f, 0.0f, 10000.0f);
                ImGui::DragFloat("Target Air Lift", &template_.hit.targetAirLiftPower, 0.1f, -1000.0f, 1000.0f);
                ImGui::DragFloat("Attacker Air Lift", &template_.hit.attackerAirLiftPower, 0.1f, -1000.0f, 1000.0f);
                ImGui::DragFloat("Hit Stop Duration", &template_.hit.hitStopDuration, 0.001f, 0.0f, 10.0f);
                ImGui::DragFloat("Hit Stop Scale", &template_.hit.hitStopScale, 0.01f, 0.0f, 1.0f);
                ImGui::InputScalar("Hit Group ID", ImGuiDataType_U32, &template_.hit.hitGroupId);
                ImGui::Checkbox("Allow Multi Hit", &template_.hit.allowMultiHit);
                ImGui::Checkbox("Can Hit Owner", &template_.hit.canHitOwner);
                ImGui::Checkbox("Destroy On Hit", &template_.hit.destroyOnHit);
                AssetSlotGui::Draw("Hit Particle", AssetSlotType::JSON, template_.hit.hitParticleAssetPath);
                ImGui::InputText("Hit Emitter Name", &template_.hit.hitParticleEmitterName);
                ImGui::DragFloat("Hit Particle Life Time", &template_.hit.hitParticleLifeTime, 0.01f, 0.0f, 30.0f);
                ImGui::DragFloat("Hit Particle Normal Offset", &template_.hit.hitParticleNormalOffset, 0.01f, -10.0f, 10.0f);
                ImGui::Checkbox("Use Particle Impact Axis", &template_.hit.hitParticleUseImpactAxis);
                ImGui::BeginDisabled(!template_.hit.hitParticleUseImpactAxis);
                ImGui::SliderFloat("Axis To Attacker Blend", &template_.hit.hitParticleAxisToAttackerBlend, 0.0f, 1.0f);
                ImGui::EndDisabled();
                ImGui::Checkbox("Use Particle Target Pull", &template_.hit.hitParticleUseTargetAttraction);
                ImGui::BeginDisabled(!template_.hit.hitParticleUseTargetAttraction);
                ImGui::DragFloat("Target Pull Strength", &template_.hit.hitParticleTargetAttractionStrength, 0.01f, -100.0f, 100.0f);
                ImGui::DragFloat("Target Pull Range", &template_.hit.hitParticleTargetAttractionRange, 0.01f, 0.0f, 100.0f);
                ImGui::DragFloat("Target Pull Falloff", &template_.hit.hitParticleTargetAttractionFalloff, 0.01f, 0.001f, 8.0f);
                ImGui::EndDisabled();
                AssetSlotGui::Draw("Hit Mesh VFX", AssetSlotType::JSON, template_.hit.hitMeshVFXAssetPath);
                ImGui::Checkbox("Camera Shake", &template_.hit.cameraShakeEnabled);
                ImGui::DragFloat("Shake Duration", &template_.hit.cameraShakeReceiveDuration, 0.001f, 0.0f, 10.0f);
                ImGui::DragFloat("Shake Position", &template_.hit.cameraShakeReceivePositionStrength, 0.001f, 0.0f, 10.0f);
                ImGui::DragFloat("Shake Rotation", &template_.hit.cameraShakeReceiveRotationStrength, 0.001f, 0.0f, 10.0f);
                ImGui::DragFloat("Shake Attack Scale", &template_.hit.cameraShakeAttackScale, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Shake Frequency", &template_.hit.cameraShakeFrequency, 0.1f, 0.0f, 240.0f);
                ImGui::InputInt("Shake Priority", &template_.hit.cameraShakePriority);
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (removeTemplateIndex_ >= 0)
    {
        templates.erase(templates.begin() + removeTemplateIndex_);
        RegisterActions();
    }
}

void EffectEmitterComponent::Serialize(nlohmann::json& json_) const
{
    json_["ComponentName"] = componentName;
    json_["Templates"] = templates;
}

void EffectEmitterComponent::Deserialize(const nlohmann::json& json_)
{
    templates = json_.value("Templates", std::vector<EffectEmitterTemplate>{});
    if (templates.empty())
    {
        EffectEmitterTemplate template_{};
        template_.entries.push_back({});
        templates.push_back(template_);
    }
    RegisterActions();
}

REGISTER_COMPONENT(EffectEmitterComponent, "EffectEmitterComponent")
