#include "EffectManager.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Combat/AttackEffectHitComponent.h"
#include "Engine/GamePlay/Combat/AttackTemplateManager.h"
#include "Engine/GamePlay/Transform/ModelAttachmentComponent.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/Decal/DecalComponent.h"
#include "Engine/Graphics/Resource/GLTFModel.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"
#include "Engine/Physics/HitBox.h"
#include "Engine/Platform/WindowsDialogHelper.h"

#include <DirectXMath.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace
{
    constexpr size_t EFFECT_PREWARM_COUNT = 4u;
}

void to_json(nlohmann::json& json_, const SlashEffectSpawnSetting& setting_)
{
    json_ = {
        { "vfxPath", setting_.vfxPath },
        { "spawnTime", setting_.spawnTime },
        { "lifeTime", setting_.lifeTime },
        { "offset", { setting_.localOffset.x, setting_.localOffset.y, setting_.localOffset.z } },
        { "rotation", { setting_.localRotation.x, setting_.localRotation.y, setting_.localRotation.z } },
        { "scale", { setting_.localScale.x, setting_.localScale.y, setting_.localScale.z } },
        { "attachNodeName", setting_.attachNodeName },
        { "playbackSpeed", setting_.playbackSpeed },
        { "hasHitBox", setting_.hasHitBox },
        { "hitBoxName", setting_.hitBoxName },
        { "hitStartTime", setting_.hitStartTime },
        { "hitEndTime", setting_.hitEndTime },
        { "hitBoxRadius", setting_.hitBoxRadius },
        { "hitBoxHalfHeight", setting_.hitBoxHalfHeight },
        { "damage", setting_.damage },
        { "impactPower", setting_.impactPower },
        { "knockBackPower", setting_.knockBackPower },
        { "targetAirLiftPower", setting_.targetAirLiftPower },
        { "attackerAirLiftPower", setting_.attackerAirLiftPower },
        { "hitStopDuration", setting_.hitStopDuration },
        { "hitStopScale", setting_.hitStopScale },
        { "hitEffectPath", setting_.hitEffectPath },
        { "hitGroupId", setting_.hitGroupId },
        { "allowMultiHit", setting_.allowMultiHit },
        { "followOwner", setting_.followOwner },
        { "mirror", setting_.mirror },
    };
}

void from_json(const nlohmann::json& json_, SlashEffectSpawnSetting& setting_)
{
    setting_.vfxPath = json_.value("vfxPath", std::string{});
    setting_.spawnTime = json_.value("spawnTime", 0.0f);
    setting_.lifeTime = json_.value("lifeTime", 0.18f);
    auto readVector3_ = [&json_](const char* key_, const DirectX::XMFLOAT3& fallback_)
        {
            const nlohmann::json value_ = json_.value(key_, nlohmann::json::array());
            if (!value_.is_array() || value_.size() < 3u) return fallback_;
            return DirectX::XMFLOAT3{
                value_[0].get<float>(), value_[1].get<float>(), value_[2].get<float>() };
        };
    setting_.localOffset = readVector3_("offset", { 0.0f, 1.0f, 1.2f });
    setting_.localRotation = readVector3_("rotation", { 0.0f, 0.0f, 0.0f });
    setting_.localScale = readVector3_("scale", { 1.0f, 1.0f, 1.0f });
    setting_.attachNodeName = json_.value("attachNodeName", std::string{});
    setting_.playbackSpeed = (std::max)(json_.value("playbackSpeed", 1.0f), 0.0f);
    setting_.hasHitBox = json_.value("hasHitBox", false);
    setting_.hitBoxName = json_.value("hitBoxName", std::string("EffectHitBox"));
    setting_.hitStartTime = json_.value("hitStartTime", 0.0f);
    setting_.hitEndTime = json_.value("hitEndTime", 0.08f);
    setting_.hitBoxRadius = json_.value("hitBoxRadius", 0.5f);
    setting_.hitBoxHalfHeight = json_.value("hitBoxHalfHeight", 1.0f);
    setting_.damage = json_.value("damage", 10.0f);
    setting_.impactPower = (std::max)(json_.value("impactPower", setting_.damage), 0.0f);
    setting_.knockBackPower = json_.value("knockBackPower", 3.0f);
    setting_.targetAirLiftPower = (std::max)(json_.value("targetAirLiftPower", 0.0f), 0.0f);
    setting_.attackerAirLiftPower = (std::max)(json_.value("attackerAirLiftPower", 0.0f), 0.0f);
    setting_.hitStopDuration = json_.value("hitStopDuration", json_.value("hitStopTime", 0.0f));
    setting_.hitStopScale = json_.value("hitStopScale", 0.0f);
    setting_.hitEffectPath = json_.value("hitEffectPath", std::string{});
    setting_.hitGroupId = json_.value("hitGroupId", 0u);
    setting_.allowMultiHit = json_.value("allowMultiHit", false);
    setting_.followOwner = json_.value("followOwner", false);
    setting_.mirror = json_.value("mirror", false);
}

void EffectManager::Initialize(GameObjectManager* gameObjectManager_)
{
    gameObjectManager = gameObjectManager_;
    RefreshAssetCache();
    BuildAssetNameMap();
    PreloadAllEffects();
}

void EffectManager::Update(float deltaTime_)
{
    const float safeDeltaTime_ = (std::max)(deltaTime_, 0.0f);

    for (ScheduledAttackEffect& scheduledEffect_ : scheduledAttackEffects)
    {
        scheduledEffect_.timeRemaining -= safeDeltaTime_;
    }
    std::vector<AttackEffectSpawnRequest> readyRequests_;
    for (const ScheduledAttackEffect& scheduledEffect_ : scheduledAttackEffects)
    {
        if (scheduledEffect_.timeRemaining <= 0.0f)
        {
            readyRequests_.push_back(scheduledEffect_.request);
        }
    }
    scheduledAttackEffects.erase(
        std::remove_if(
            scheduledAttackEffects.begin(),
            scheduledAttackEffects.end(),
            [](const ScheduledAttackEffect& scheduledEffect_)
            {
                return scheduledEffect_.timeRemaining <= 0.0f;
            }),
        scheduledAttackEffects.end());
    for (AttackEffectSpawnRequest& readyRequest_ : readyRequests_)
    {
        readyRequest_.setting.spawnTime = 0.0f;
        SpawnSlashEffect(readyRequest_);
    }

    for (ManagedEffect& effect_ : managedEffects)
    {
        effect_.lifeTimer -= safeDeltaTime_;
        effect_.elapsedTime += safeDeltaTime_;
        if (effect_.poolType == EFFECT_POOL_TYPE::VFX_SEQUENCE)
        {
            effect_.sequenceTime += safeDeltaTime_;
            UpdateVFXSequence(effect_);
        }
        if (const std::shared_ptr<HitBox> hitBox_ = effect_.hitBox.lock())
        {
            const bool shouldEnable_ =
                effect_.elapsedTime >= effect_.hitStartTime &&
                effect_.elapsedTime <= effect_.hitEndTime;
            if (shouldEnable_ && !hitBox_->IsEnabled()) hitBox_->Activate();
            if (!shouldEnable_ && hitBox_->IsEnabled()) hitBox_->Deactivate();
        }
    }

    managedEffects.erase(
        std::remove_if(
            managedEffects.begin(),
            managedEffects.end(),
            [this](ManagedEffect& effect_)
            {
                const std::shared_ptr<GameObject> object_ = effect_.object.lock();
                if (!object_) return true;

                if (effect_.lifeTimer > 0.0f) return false;

                if (effect_.poolType != EFFECT_POOL_TYPE::NONE)
                {
                    ReleasePooledEffect(effect_);
                }
                else if (gameObjectManager)
                {
                    gameObjectManager->Destroy(object_);
                }
                return true;
            }),
        managedEffects.end());
}

void EffectManager::Clear()
{
    for (ManagedEffect& effect_ : managedEffects)
    {
        if (effect_.poolType != EFFECT_POOL_TYPE::NONE)
        {
            ReleasePooledEffect(effect_);
        }
    }
    managedEffects.clear();
    scheduledAttackEffects.clear();
}

void EffectManager::RefreshAssetCache()
{
    CollectJsonAssets("Data/Particle", particleAssetPaths);
    CollectJsonAssets("Data/VFX", meshVFXAssetPaths);
    CollectJsonAssets("Data/VFXSequence", vfxSequenceAssetPaths);
    CollectJsonAssets("Data/ImpactEffect", impactEffectAssetPaths);
}

void EffectManager::BuildAssetNameMap()
{
    BuildAssetNameMap(
        particleAssetPaths,
        "Data/Particle",
        particleAssets,
        particleAssetKeys,
        particlePathByKey);
    BuildAssetNameMap(
        meshVFXAssetPaths,
        "Data/VFX",
        meshVFXAssets,
        meshVFXAssetKeys,
        meshVFXPathByKey);
    BuildAssetNameMap(
        vfxSequenceAssetPaths,
        "Data/VFXSequence",
        vfxSequenceAssets,
        vfxSequenceAssetKeys,
        vfxSequencePathByKey);
    BuildAssetNameMap(
        impactEffectAssetPaths,
        "Data/ImpactEffect",
        impactEffectAssets,
        impactEffectAssetKeys,
        impactEffectPathByKey);
}

void EffectManager::PreloadAllEffects()
{
    Clear();
    particleAssetCache.clear();
    meshVFXAssetCache.clear();
    vfxSequenceAssetCache.clear();
    impactEffectAssetCache.clear();
    particleObjectPool.clear();
    meshVFXObjectPool.clear();
    vfxSequenceObjectPool.clear();

    for (const EffectAssetEntry& entry_ : particleAssets)
    {
        PreloadParticleAsset(entry_.fullPath);
    }

    for (const EffectAssetEntry& entry_ : meshVFXAssets)
    {
        PreloadMeshVFXAsset(entry_.fullPath);
    }
    for (const EffectAssetEntry& entry_ : vfxSequenceAssets)
    {
        PreloadVFXSequenceAsset(entry_.fullPath);
    }
    for (const EffectAssetEntry& entry_ : impactEffectAssets)
    {
        PreloadImpactEffectAsset(entry_.fullPath);
    }
}

void EffectManager::RebuildRuntimePools()
{
    particleObjectPool.clear();
    meshVFXObjectPool.clear();
    vfxSequenceObjectPool.clear();

    for (const EffectAssetEntry& entry_ : particleAssets)
    {
        const std::string assetName_ = BuildAssetNameFromPath(
            entry_.fullPath,
            "Data/Particle");
        const auto assetIterator_ = particleAssetCache.find(assetName_);
        if (assetIterator_ == particleAssetCache.end()) continue;

        std::vector<std::shared_ptr<GameObject>>& objects_ =
            particleObjectPool[assetName_];
        for (size_t index_ = 0; index_ < EFFECT_PREWARM_COUNT; ++index_)
        {
            std::shared_ptr<GameObject> object_ = CreateParticlePoolObject(
                entry_.fullPath,
                assetIterator_->second);
            if (object_) objects_.push_back(object_);
        }
    }

    for (const EffectAssetEntry& entry_ : meshVFXAssets)
    {
        const std::string assetName_ = BuildAssetNameFromPath(
            entry_.fullPath,
            "Data/VFX");
        const auto assetIterator_ = meshVFXAssetCache.find(assetName_);
        if (assetIterator_ == meshVFXAssetCache.end()) continue;

        std::vector<std::shared_ptr<GameObject>>& objects_ =
            meshVFXObjectPool[assetName_];
        for (size_t index_ = 0; index_ < EFFECT_PREWARM_COUNT; ++index_)
        {
            std::shared_ptr<GameObject> object_ = CreateMeshVFXPoolObject(
                entry_.fullPath,
                assetIterator_->second);
            if (object_) objects_.push_back(object_);
        }
    }

    for (const EffectAssetEntry& entry_ : vfxSequenceAssets)
    {
        const std::string assetName_ = BuildAssetNameFromPath(
            entry_.fullPath,
            "Data/VFXSequence");
        const auto assetIterator_ = vfxSequenceAssetCache.find(assetName_);
        if (assetIterator_ == vfxSequenceAssetCache.end()) continue;

        std::vector<std::shared_ptr<GameObject>>& objects_ =
            vfxSequenceObjectPool[assetName_];
        for (size_t index_ = 0; index_ < EFFECT_PREWARM_COUNT; ++index_)
        {
            std::shared_ptr<GameObject> object_ = CreateVFXSequencePoolObject(
                entry_.fullPath,
                assetIterator_->second);
            if (object_) objects_.push_back(object_);
        }
    }

    std::printf(
        "[EffectManager] Runtime pools rebuilt: particle=%zu meshVFX=%zu sequence=%zu\n",
        particleObjectPool.size(),
        meshVFXObjectPool.size(),
        vfxSequenceObjectPool.size());
}

std::string EffectManager::ResolveParticlePath(const std::string& assetNameOrPath_) const
{
    if (assetNameOrPath_.empty()) return {};

    const auto directIterator_ = particlePathByKey.find(assetNameOrPath_);
    if (directIterator_ != particlePathByKey.end()) return directIterator_->second;

    const std::string assetKey_ = BuildAssetNameFromPath(assetNameOrPath_, "Data/Particle");
    const auto keyIterator_ = particlePathByKey.find(assetKey_);
    if (keyIterator_ != particlePathByKey.end()) return keyIterator_->second;

    const std::filesystem::path path_ =
        WindowsDialogHelper::Utf8ToPath(assetNameOrPath_);
    std::error_code error_;
    if (std::filesystem::is_regular_file(path_, error_))
    {
        return WindowsDialogHelper::PathToUtf8(path_.lexically_normal());
    }

    return assetNameOrPath_;
}

std::string EffectManager::ResolveMeshVFXPath(const std::string& assetNameOrPath_) const
{
    if (assetNameOrPath_.empty()) return {};

    const auto directIterator_ = meshVFXPathByKey.find(assetNameOrPath_);
    if (directIterator_ != meshVFXPathByKey.end()) return directIterator_->second;

    const std::string assetKey_ = BuildAssetNameFromPath(assetNameOrPath_, "Data/VFX");
    const auto keyIterator_ = meshVFXPathByKey.find(assetKey_);
    if (keyIterator_ != meshVFXPathByKey.end()) return keyIterator_->second;

    const std::filesystem::path path_ =
        WindowsDialogHelper::Utf8ToPath(assetNameOrPath_);
    std::error_code error_;
    if (std::filesystem::is_regular_file(path_, error_))
    {
        return WindowsDialogHelper::PathToUtf8(path_.lexically_normal());
    }

    return assetNameOrPath_;
}

std::string EffectManager::ResolveVFXSequencePath(
    const std::string& assetNameOrPath_) const
{
    if (assetNameOrPath_.empty()) return {};

    const auto directIterator_ = vfxSequencePathByKey.find(assetNameOrPath_);
    if (directIterator_ != vfxSequencePathByKey.end()) return directIterator_->second;

    const std::string assetKey_ = BuildAssetNameFromPath(
        assetNameOrPath_,
        "Data/VFXSequence");
    const auto keyIterator_ = vfxSequencePathByKey.find(assetKey_);
    if (keyIterator_ != vfxSequencePathByKey.end()) return keyIterator_->second;

    const std::filesystem::path path_ =
        WindowsDialogHelper::Utf8ToPath(assetNameOrPath_);
    std::error_code error_;
    if (std::filesystem::is_regular_file(path_, error_) &&
        path_.generic_string().find("VFXSequence") != std::string::npos)
    {
        return WindowsDialogHelper::PathToUtf8(path_.lexically_normal());
    }
    return {};
}

std::string EffectManager::ResolveImpactEffectPath(
    const std::string& assetNameOrPath_) const
{
    if (assetNameOrPath_.empty()) return {};
    const auto directIterator_ = impactEffectPathByKey.find(assetNameOrPath_);
    if (directIterator_ != impactEffectPathByKey.end()) return directIterator_->second;

    const std::string assetKey_ = BuildAssetNameFromPath(
        assetNameOrPath_,
        "Data/ImpactEffect");
    const auto keyIterator_ = impactEffectPathByKey.find(assetKey_);
    if (keyIterator_ != impactEffectPathByKey.end()) return keyIterator_->second;

    const std::filesystem::path path_ =
        WindowsDialogHelper::Utf8ToPath(assetNameOrPath_);
    std::error_code error_;
    if (std::filesystem::is_regular_file(path_, error_))
    {
        return WindowsDialogHelper::PathToUtf8(path_.lexically_normal());
    }
    return {};
}

std::shared_ptr<GameObject> EffectManager::SpawnParticle(
    const ParticleSpawnRequest& request_)
{
    if (!gameObjectManager || request_.assetPath.empty()) return nullptr;

    const std::string resolvedAssetPath_ =
        ResolveParticlePath(request_.assetPath);
    std::printf(
        "[EffectManager::SpawnParticle] request='%s' resolved='%s' emitter='%s'\n",
        request_.assetPath.c_str(),
        resolvedAssetPath_.c_str(),
        request_.emitterName.c_str());

    std::shared_ptr<GameObject> object_ =
        AcquireParticleObject(resolvedAssetPath_);

    if (!object_) return nullptr;

    const DirectX::XMFLOAT3 spawnPosition_ = ApplyNormalOffset(
        request_.position,
        request_.normal,
        request_.normalOffset);

    Vector3 position_(spawnPosition_.x, spawnPosition_.y, spawnPosition_.z);
    object_->transform->SetLocalPosition(position_);
    object_->UpdateTransform();

    std::shared_ptr<ParticleComponent> particle_ =
        object_->GetComponent<ParticleComponent>();

    if (!particle_) return object_;
    particle_->SetEnabled(true);
    particle_->ResetRuntime();

    bool played_ = false;
    if (!request_.emitterName.empty())
    {
        played_ = particle_->TriggerEmitterAtPosition(
            request_.emitterName,
            spawnPosition_);
        if (!played_)
        {
            std::printf(
                "[EffectManager::SpawnParticle] emitter '%s' was not found in '%s'; playing all emitters\n",
                request_.emitterName.c_str(),
                resolvedAssetPath_.c_str());
        }
    }
    if (!played_)
    {
        particle_->Play();
    }

    RegisterManagedEffect(
        object_,
        request_.lifeTime,
        nullptr,
        0.0f,
        0.0f,
        EFFECT_POOL_TYPE::PARTICLE,
        resolvedAssetPath_);
    return object_;
}

std::shared_ptr<GameObject> EffectManager::SpawnSlashEffect(
    const AttackEffectSpawnRequest& request_)
{
    if (!gameObjectManager || request_.setting.vfxPath.empty()) return nullptr;
    if (request_.setting.spawnTime > 0.0f)
    {
        ScheduledAttackEffect scheduledEffect_{};
        scheduledEffect_.request = request_;
        scheduledEffect_.timeRemaining = request_.setting.spawnTime;
        scheduledAttackEffects.push_back(scheduledEffect_);
        return nullptr;
    }

    MeshVFXSpawnRequest vfxRequest_{};
    vfxRequest_.assetPath = request_.setting.vfxPath;
    vfxRequest_.lifeTime = request_.setting.lifeTime;
    std::shared_ptr<GameObject> object_ = SpawnMeshVFX(vfxRequest_);
    if (!object_) return nullptr;
    if (std::shared_ptr<MeshVFXComponent> meshVFX_ = object_->GetComponent<MeshVFXComponent>())
    {
        meshVFX_->SetPlaybackSpeed(request_.setting.playbackSpeed);
    }

    using namespace DirectX;
    DirectX::XMFLOAT3 localScale_ = request_.setting.localScale;
    if (request_.setting.mirror)
    {
        localScale_.x *= -1.0f;
    }
    const XMMATRIX localMatrix_ =
        XMMatrixScaling(
            localScale_.x,
            localScale_.y,
            localScale_.z) *
        XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(request_.setting.localRotation.x),
            XMConvertToRadians(request_.setting.localRotation.y),
            XMConvertToRadians(request_.setting.localRotation.z)) *
        XMMatrixTranslation(
            request_.setting.localOffset.x,
            request_.setting.localOffset.y,
            request_.setting.localOffset.z);
    const XMMATRIX ownerWorldMatrix_ = request_.ownerObject && request_.ownerObject->transform
        ? request_.ownerObject->transform->GetWorldMatrix()
        : XMMatrixIdentity();
    if (request_.setting.followOwner && request_.ownerObject && request_.ownerObject->transform)
    {
        object_->transform->SetParent(request_.ownerObject->transform);
        object_->transform->localPosition = request_.setting.localOffset;
        object_->transform->localRotation = request_.setting.localRotation;
        object_->transform->localScale = localScale_;
        object_->transform->MarkDirty();
        if (!request_.setting.attachNodeName.empty())
        {
            std::shared_ptr<GltfModelCommponennt> model_ =
                request_.ownerObject->GetComponent<GltfModelCommponennt>();
            if (model_)
            {
                std::shared_ptr<ModelAttachmentComponent> attachment_ =
                    object_->GetOrAddComponent<ModelAttachmentComponent>();
                if (attachment_)
                {
                    attachment_->SetEnabled(true);
                    attachment_->SetTargetNode(model_, request_.setting.attachNodeName);
                    attachment_->SyncAttachment();
                }
            }
        }
    }
    else
    {
        object_->transform->ApplyWorldMatrix(localMatrix_ * ownerWorldMatrix_);
    }
    object_->UpdateTransform();

    std::shared_ptr<HitBox> hitBox_;
    if (request_.setting.hasHitBox)
    {
        uint32_t attackInstanceId_ = request_.attackInstanceId;
        std::shared_ptr<HitBox> sourceHitBox_;
        if (request_.ownerObject != nullptr)
        {
            sourceHitBox_ = request_.ownerObject->GetComponent<HitBox>();
            if (attackInstanceId_ == 0u && sourceHitBox_)
            {
                attackInstanceId_ = sourceHitBox_->attackInstanceId;
            }
        }
        if (attackInstanceId_ == 0u)
        {
            attackInstanceId_ = gameObjectManager->GetAttackSystem().CreateAttackInstance();
            if (sourceHitBox_) sourceHitBox_->SetAttackInstanceId(attackInstanceId_);
        }

        hitBox_ = object_->GetOrAddComponent<HitBox>();
        if (hitBox_)
        {
            hitBox_->SetEnabled(true);
            hitBox_->actionPrefix = request_.setting.hitBoxName;
            hitBox_->SetOwner(request_.ownerObject);
            hitBox_->SetAttackInstanceId(attackInstanceId_);
            hitBox_->SetHitGroupId(request_.setting.hitGroupId);
            hitBox_->SetDamage(request_.setting.damage);
            hitBox_->SetImpactPower(request_.setting.impactPower);
            hitBox_->SetKnockBackPower(request_.setting.knockBackPower);
            hitBox_->SetAirLiftPower(
                request_.setting.targetAirLiftPower,
                request_.setting.attackerAirLiftPower);
            hitBox_->SetHitEffectPath(request_.setting.hitEffectPath);
            hitBox_->SetHitEffectBlackboardOverrideEnabled(false);
            hitBox_->hitStopDuration = request_.setting.hitStopDuration;
            hitBox_->hitStopScale = request_.setting.hitStopScale;
            hitBox_->allowMultiHit = request_.setting.allowMultiHit;
            hitBox_->radius = request_.setting.hitBoxRadius;
            hitBox_->halfHeight = request_.setting.hitBoxHalfHeight;
            hitBox_->Deactivate();
            hitBox_->RecreatePhysXActor();
        }
    }

    for (ManagedEffect& effect_ : managedEffects)
    {
        if (effect_.object.lock() == object_)
        {
            effect_.hitBox = hitBox_;
            effect_.hitStartTime = (std::max)(request_.setting.hitStartTime, 0.0f);
            effect_.hitEndTime = (std::max)(request_.setting.hitEndTime, effect_.hitStartTime);
            break;
        }
    }
    return object_;
}

std::shared_ptr<GameObject> EffectManager::SpawnAttackEffect(
    const AttackEffectSpawnRequest& request_)
{
    return SpawnSlashEffect(request_);
}

std::shared_ptr<GameObject> EffectManager::SpawnAttackEffect(
    const AttackEffectSpawnTemplate& effectTemplate_,
    const AttackTemplate& attackTemplate_,
    GameObject* ownerObject_,
    uint32_t attackInstanceId_)
{
    if (!gameObjectManager || !effectTemplate_.enabled) return nullptr;
    std::shared_ptr<GameObject> object_ = gameObjectManager->Create("RuntimeAttackEffect");
    if (!object_) return nullptr;
    object_->SetNoSaveObject();

    using namespace DirectX;
    const XMMATRIX localMatrix_ =
        XMMatrixScaling(effectTemplate_.localScale.x, effectTemplate_.localScale.y, effectTemplate_.localScale.z) *
        XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(effectTemplate_.localRotation.x),
            XMConvertToRadians(effectTemplate_.localRotation.y),
            XMConvertToRadians(effectTemplate_.localRotation.z)) *
        XMMatrixTranslation(effectTemplate_.localOffset.x, effectTemplate_.localOffset.y, effectTemplate_.localOffset.z);
    const XMMATRIX ownerMatrix_ = ownerObject_ && ownerObject_->transform
        ? ownerObject_->transform->GetWorldMatrix() : XMMatrixIdentity();
    if (effectTemplate_.followOwner && ownerObject_ && ownerObject_->transform)
    {
        object_->transform->SetParent(ownerObject_->transform);
        object_->transform->localPosition = effectTemplate_.localOffset;
        object_->transform->localRotation = effectTemplate_.localRotation;
        object_->transform->localScale = effectTemplate_.localScale;
        object_->transform->MarkDirty();
    }
    else
    {
        object_->transform->ApplyWorldMatrix(localMatrix_ * ownerMatrix_);
    }
    object_->UpdateTransform();

    if (!effectTemplate_.particlePath.empty())
    {
        const std::string resolvedParticlePath_ =
            ResolveParticlePath(effectTemplate_.particlePath);
        const ParticleEffectAsset* particleAsset_ =
            FindParticleAssetFromRequest(resolvedParticlePath_);
        std::shared_ptr<ParticleComponent> particle_ =
            object_->AddComponent<ParticleComponent>();
        if (particle_ && particleAsset_)
        {
            particle_->SetAsset(*particleAsset_);
            particle_->ResetRuntime();
            particle_->Play();
        }
        else if (!particleAsset_)
        {
            std::printf(
                "[EffectManager::SpawnAttackEffect] Particle cache miss: %s\n",
                resolvedParticlePath_.c_str());
        }
    }
    if (!effectTemplate_.meshVfxPath.empty())
    {
        const std::string resolvedMeshVFXPath_ =
            ResolveMeshVFXPath(effectTemplate_.meshVfxPath);
        const MeshVFXAsset* meshVFXAsset_ =
            FindMeshVFXAssetFromRequest(resolvedMeshVFXPath_);
        std::shared_ptr<MeshVFXComponent> meshVFX_ =
            object_->AddComponent<MeshVFXComponent>();
        if (meshVFX_ && meshVFXAsset_)
        {
            meshVFX_->SetAsset(*meshVFXAsset_);
            meshVFX_->Reset();
            meshVFX_->Play();
        }
        else if (!meshVFXAsset_)
        {
            std::printf(
                "[EffectManager::SpawnAttackEffect] MeshVFX cache miss: %s\n",
                resolvedMeshVFXPath_.c_str());
        }
    }

    std::shared_ptr<AttackEffectHitComponent> attackHit_ = object_->AddComponent<AttackEffectHitComponent>();
    if (attackHit_)
    {
        attackHit_->SetOwner(ownerObject_);
        attackHit_->SetAttackInstanceId(attackInstanceId_);
        attackHit_->SetHitGroupId(attackTemplate_.hitGroupId);
        attackHit_->SetDamage(attackTemplate_.damage);
        attackHit_->SetImpactPower(attackTemplate_.impactPower);
        attackHit_->SetHitStopDuration(attackTemplate_.hitStopDuration);
        attackHit_->SetHitStopScale(attackTemplate_.hitStopScale);
        attackHit_->SetKnockBackPower(attackTemplate_.knockBackPower);
        attackHit_->SetAirLiftPower(
            attackTemplate_.targetAirLiftPower,
            attackTemplate_.attackerAirLiftPower);
        attackHit_->SetHitParticleAssetPath(
            attackTemplate_.hitEffect.spawnOnHit
            ? attackTemplate_.hitEffect.hitParticleAssetPath
            : std::string{});
        attackHit_->SetHitParticleEmitterName(
            attackTemplate_.hitEffect.spawnOnHit
            ? attackTemplate_.hitEffect.hitParticleEmitterName
            : std::string{});
        attackHit_->SetHitParticleLifeTime(attackTemplate_.hitEffect.hitParticleLifeTime);
        attackHit_->SetHitParticleNormalOffset(attackTemplate_.hitEffect.hitParticleNormalOffset);
        attackHit_->SetHitMeshVFXAssetPath(
            attackTemplate_.hitEffect.spawnOnHit
            ? attackTemplate_.hitEffect.hitMeshVfxAssetPath
            : std::string{});
        attackHit_->SetHitMeshVFXLifeTime(attackTemplate_.hitEffect.hitMeshVfxLifeTime);
        attackHit_->SetHitMeshVFXNormalOffset(attackTemplate_.hitEffect.hitMeshVfxNormalOffset);
        attackHit_->SetAllowMultiHit(attackTemplate_.allowMultiHit);
        attackHit_->SetCanHitOwner(attackTemplate_.canHitOwner);
        attackHit_->SetEffectType(effectTemplate_.effectType);
        attackHit_->SetLifeTime(effectTemplate_.lifeTime);
        attackHit_->SetMovement(effectTemplate_.localDirection, effectTemplate_.speed);
        attackHit_->SetHitShapes(effectTemplate_.hitShapes);
        attackHit_->Play();
    }
    return object_;
}

std::shared_ptr<GameObject> EffectManager::SpawnEffectWithHitBox(
    const AttackEffectSpawnRequest& request_)
{
    AttackEffectSpawnRequest hitBoxRequest_ = request_;
    hitBoxRequest_.setting.hasHitBox = true;
    return SpawnSlashEffect(hitBoxRequest_);
}

std::shared_ptr<GameObject> EffectManager::SpawnMeshVFX(
    const MeshVFXSpawnRequest& request_)
{
    if (!gameObjectManager || request_.assetPath.empty()) return nullptr;
    const std::string resolvedSequencePath_ =
        ResolveVFXSequencePath(request_.assetPath);
    if (!resolvedSequencePath_.empty())
    {
        return SpawnVFXSequence(request_, resolvedSequencePath_);
    }
    const std::string resolvedAssetPath_ = ResolveMeshVFXPath(request_.assetPath);

    std::shared_ptr<GameObject> object_ =
        AcquireMeshVFXObject(resolvedAssetPath_);
    if (!object_) return nullptr;

    const DirectX::XMFLOAT3 spawnPosition_ = ApplyNormalOffset(
        request_.position,
        request_.normal,
        request_.normalOffset);
    Vector3 position_(spawnPosition_.x, spawnPosition_.y, spawnPosition_.z);
    object_->transform->SetLocalPosition(position_);
    object_->UpdateTransform();

    std::shared_ptr<MeshVFXComponent> meshVFX_ =
        object_->GetComponent<MeshVFXComponent>();
    if (!meshVFX_) return object_;

    std::printf(
        "[EffectManager::SpawnMeshVFX] request='%s' resolved='%s'\n",
        request_.assetPath.c_str(),
        resolvedAssetPath_.c_str());
    meshVFX_->SetEnabled(true);
    meshVFX_->Reset();
    meshVFX_->Play();

    RegisterManagedEffect(
        object_,
        request_.lifeTime,
        nullptr,
        0.0f,
        0.0f,
        EFFECT_POOL_TYPE::MESH_VFX,
        resolvedAssetPath_);
    return object_;
}

void EffectManager::SpawnForHit(const AttackHitEvent& event_)
{
    if (!event_.hasHitContact) return;

    std::printf(
        "[EffectManager::SpawnForHit] particle='%s' emitter='%s' meshVFX='%s' "
        "position=(%.3f, %.3f, %.3f) normal=(%.3f, %.3f, %.3f)\n",
        event_.hitParticleAssetPath.c_str(),
        event_.hitParticleEmitterName.c_str(),
        event_.hitMeshVFXAssetPath.c_str(),
        event_.hitPosition.x,
        event_.hitPosition.y,
        event_.hitPosition.z,
        event_.hitNormal.x,
        event_.hitNormal.y,
        event_.hitNormal.z);

    if (!event_.hitParticleAssetPath.empty())
    {
        ParticleSpawnRequest request_{};
        request_.assetPath = event_.hitParticleAssetPath;
        request_.emitterName = event_.hitParticleEmitterName;
        request_.position = event_.hitPosition;
        request_.normal = event_.hitNormal;
        request_.lifeTime = event_.hitParticleLifeTime;
        request_.normalOffset = event_.hitParticleNormalOffset;
        SpawnParticle(request_);
    }

    if (!event_.hitMeshVFXAssetPath.empty())
    {
        MeshVFXSpawnRequest request_{};
        request_.assetPath = event_.hitMeshVFXAssetPath;
        request_.position = event_.hitPosition;
        request_.normal = event_.hitNormal;
        request_.lifeTime = event_.hitMeshVFXLifeTime;
        request_.normalOffset = event_.hitMeshVFXNormalOffset;
        SpawnMeshVFX(request_);
    }
}

std::shared_ptr<GameObject> EffectManager::SpawnDecal(
    const DecalSpawnRequest& request_)
{
    if (!gameObjectManager || request_.texturePath.empty()) return nullptr;

    std::shared_ptr<GameObject> object_ =
        gameObjectManager->Create("RuntimeImpactDecal");
    if (!object_) return nullptr;
    object_->SetNoSaveObject();

    const DirectX::XMFLOAT3 position_ = ApplyNormalOffset(
        request_.position,
        request_.normal,
        request_.normalOffset);
    Vector3 localPosition_(position_.x, position_.y, position_.z);
    Vector3 localScale_(request_.scale.x, request_.scale.y, request_.scale.z);
    object_->transform->SetLocalPosition(localPosition_);
    object_->transform->SetLocalScale(localScale_);

    DirectX::XMFLOAT3 normal_ = request_.normal;
    DirectX::XMVECTOR normalVector_ = DirectX::XMLoadFloat3(&normal_);
    if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(normalVector_)) <= 0.000001f)
    {
        normal_ = { 0.0f, 1.0f, 0.0f };
    }
    const float yaw_ = DirectX::XMConvertToDegrees(std::atan2(normal_.x, normal_.z));
    const float horizontalLength_ = std::sqrt(
        normal_.x * normal_.x + normal_.z * normal_.z);
    const float pitch_ = DirectX::XMConvertToDegrees(
        std::atan2(-normal_.y, horizontalLength_));
    object_->transform->SetRotation(Vector3(pitch_, yaw_, 0.0f));
    object_->UpdateTransform();

    const std::shared_ptr<DecalComponent> decal_ =
        object_->AddComponent<DecalComponent>();
    if (!decal_)
    {
        gameObjectManager->Destroy(object_);
        return nullptr;
    }
    decal_->color = request_.color;
    decal_->SetTexture(request_.texturePath);
    decal_->SetLifeTime(request_.lifeTime, request_.fadeOutDuration);
    decal_->SetDestroyOnLifeEnd(true);
    return object_;
}

void EffectManager::SpawnImpactEffect(const ImpactEffectSpawnRequest& request_)
{
    if (request_.spawnParticle && !request_.particle.assetPath.empty())
    {
        ParticleSpawnRequest particle_ = request_.particle;
        particle_.position = request_.position;
        particle_.normal = request_.normal;
        SpawnParticle(particle_);
    }
    if (request_.spawnMeshVFX && !request_.meshVFX.assetPath.empty())
    {
        MeshVFXSpawnRequest meshVFX_ = request_.meshVFX;
        meshVFX_.position = request_.position;
        meshVFX_.normal = request_.normal;
        SpawnMeshVFX(meshVFX_);
    }
    if (request_.spawnDecal && !request_.decal.texturePath.empty())
    {
        DecalSpawnRequest decal_ = request_.decal;
        decal_.position = request_.position;
        decal_.normal = request_.normal;
        SpawnDecal(decal_);
    }
}

void EffectManager::SpawnImpactEffectAsset(
    const std::string& assetNameOrPath_,
    const DirectX::XMFLOAT3& position_,
    const DirectX::XMFLOAT3& normal_)
{
    const std::string resolvedPath_ = ResolveImpactEffectPath(assetNameOrPath_);
    const std::string assetKey_ = BuildAssetNameFromPath(
        resolvedPath_,
        "Data/ImpactEffect");
    const auto iterator_ = impactEffectAssetCache.find(assetKey_);
    if (iterator_ == impactEffectAssetCache.end())
    {
        std::printf(
            "[EffectManager] Impact effect cache miss: %s\n",
            assetNameOrPath_.c_str());
        return;
    }

    for (const ImpactEffectEntry& entry_ : iterator_->second.entries)
    {
        if (!entry_.enabled || entry_.assetPath.empty()) continue;
        ImpactEffectSpawnRequest request_{};
        request_.position = position_;
        request_.normal = normal_;
        if (entry_.type == IMPACT_EFFECT_ENTRY_TYPE::PARTICLE)
        {
            request_.spawnParticle = true;
            request_.particle.assetPath = entry_.assetPath;
            request_.particle.emitterName = entry_.emitterName;
            request_.particle.lifeTime = entry_.lifeTime;
            request_.particle.normalOffset = entry_.normalOffset;
        }
        else if (entry_.type == IMPACT_EFFECT_ENTRY_TYPE::MESH_VFX)
        {
            request_.spawnMeshVFX = true;
            request_.meshVFX.assetPath = entry_.assetPath;
            request_.meshVFX.lifeTime = entry_.lifeTime;
            request_.meshVFX.normalOffset = entry_.normalOffset;
        }
        else
        {
            request_.spawnDecal = true;
            request_.decal.texturePath = entry_.assetPath;
            request_.decal.scale = entry_.scale;
            request_.decal.color = entry_.color;
            request_.decal.lifeTime = entry_.lifeTime;
            request_.decal.fadeOutDuration = entry_.fadeOutDuration;
            request_.decal.normalOffset = entry_.normalOffset;
        }
        SpawnImpactEffect(request_);
    }
}

void EffectManager::StopEffect(const std::shared_ptr<GameObject>& object_)
{
    if (!object_) return;

    const auto iterator_ = std::find_if(
        managedEffects.begin(),
        managedEffects.end(),
        [&object_](const ManagedEffect& effect_)
        {
            return effect_.object.lock() == object_;
        });
    if (iterator_ == managedEffects.end()) return;

    if (iterator_->poolType != EFFECT_POOL_TYPE::NONE)
    {
        ReleasePooledEffect(*iterator_);
    }
    else if (gameObjectManager)
    {
        gameObjectManager->Destroy(object_);
    }
    managedEffects.erase(iterator_);
}

DirectX::XMFLOAT3 EffectManager::ApplyNormalOffset(
    const DirectX::XMFLOAT3& position_,
    const DirectX::XMFLOAT3& normal_,
    float normalOffset_) const
{
    using namespace DirectX;

    XMVECTOR positionVector_ = XMLoadFloat3(&position_);
    XMVECTOR normalVector_ = XMLoadFloat3(&normal_);
    if (XMVectorGetX(XMVector3LengthSq(normalVector_)) <= 0.000001f)
    {
        normalVector_ = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    }
    normalVector_ = XMVector3Normalize(normalVector_);
    positionVector_ = XMVectorAdd(
        positionVector_,
        XMVectorScale(normalVector_, normalOffset_));

    XMFLOAT3 result_{};
    XMStoreFloat3(&result_, positionVector_);
    return result_;
}

void EffectManager::RegisterManagedEffect(
    const std::shared_ptr<GameObject>& object_,
    float lifeTime_,
    const std::shared_ptr<HitBox>& hitBox_,
    float hitStartTime_,
    float hitEndTime_,
    EFFECT_POOL_TYPE poolType_,
    const std::string& assetPath_)
{
    if (!object_) return;

    ManagedEffect managedEffect_{};
    managedEffect_.object = object_;
    managedEffect_.hitBox = hitBox_;
    managedEffect_.assetPath = assetPath_;
    managedEffect_.poolType = poolType_;
    managedEffect_.lifeTimer = (std::max)(lifeTime_, 0.0f);
    if (managedEffect_.lifeTimer <= 0.0f)
    {
        managedEffect_.lifeTimer = 1.0f;
    }
    managedEffect_.hitStartTime = (std::max)(hitStartTime_, 0.0f);
    managedEffect_.hitEndTime = (std::max)(hitEndTime_, managedEffect_.hitStartTime);
    managedEffects.push_back(managedEffect_);
}

std::shared_ptr<GameObject> EffectManager::AcquireParticleObject(
    const std::string& resolvedAssetPath_)
{
    const std::string assetName_ = BuildAssetNameFromPath(
        resolvedAssetPath_,
        "Data/Particle");
    std::vector<std::shared_ptr<GameObject>>& objects_ =
        particleObjectPool[assetName_];
    for (const std::shared_ptr<GameObject>& object_ : objects_)
    {
        const std::shared_ptr<ParticleComponent> particle_ =
            object_ ? object_->GetComponent<ParticleComponent>() : nullptr;
        if (particle_ && !particle_->IsEnabled())
        {
            gameObjectManager->AddObject(object_);
            return object_;
        }
    }

    std::printf(
        "[EffectManager] Particle pool exhausted: %s size=%zu\n",
        resolvedAssetPath_.c_str(),
        objects_.size());
    return nullptr;
}

std::shared_ptr<GameObject> EffectManager::AcquireMeshVFXObject(
    const std::string& resolvedAssetPath_)
{
    const std::string assetName_ = BuildAssetNameFromPath(
        resolvedAssetPath_,
        "Data/VFX");
    std::vector<std::shared_ptr<GameObject>>& objects_ =
        meshVFXObjectPool[assetName_];
    for (const std::shared_ptr<GameObject>& object_ : objects_)
    {
        const std::shared_ptr<MeshVFXComponent> meshVFX_ =
            object_ ? object_->GetComponent<MeshVFXComponent>() : nullptr;
        if (meshVFX_ && !meshVFX_->IsEnabled())
        {
            gameObjectManager->AddObject(object_);
            return object_;
        }
    }

    std::printf(
        "[EffectManager] MeshVFX pool exhausted: %s size=%zu\n",
        resolvedAssetPath_.c_str(),
        objects_.size());
    return nullptr;
}

std::shared_ptr<GameObject> EffectManager::CreateParticlePoolObject(
    const std::string& resolvedAssetPath_,
    const ParticleEffectAsset& asset_)
{
    (void)resolvedAssetPath_;
    if (!gameObjectManager) return nullptr;
    std::shared_ptr<GameObject> object_ =
        gameObjectManager->Create("PooledParticle");
    if (!object_) return nullptr;
    object_->SetNoSaveObject();

    std::shared_ptr<ParticleComponent> particle_ =
        object_->AddComponent<ParticleComponent>();
    if (!particle_) return nullptr;
    particle_->SetAsset(asset_);
    particle_->ResetRuntime();
    if (!particle_->HasRuntimeResources())
    {
        std::printf(
            "[EffectManager] Particle pool resource build failed: %s\n",
            resolvedAssetPath_.c_str());
        gameObjectManager->Destroy(object_);
        return nullptr;
    }
    particle_->SetEnabled(false);
    gameObjectManager->DetachObject(object_);
    return object_;
}

std::shared_ptr<GameObject> EffectManager::CreateMeshVFXPoolObject(
    const std::string& resolvedAssetPath_,
    const MeshVFXAsset& asset_)
{
    (void)resolvedAssetPath_;
    if (!gameObjectManager) return nullptr;
    std::shared_ptr<GameObject> object_ =
        gameObjectManager->Create("PooledMeshVFX");
    if (!object_) return nullptr;
    object_->SetNoSaveObject();

    std::shared_ptr<MeshVFXComponent> meshVFX_ =
        object_->AddComponent<MeshVFXComponent>();
    if (!meshVFX_) return nullptr;
    meshVFX_->SetAsset(asset_);
    if (!meshVFX_->HasRuntimeResources())
    {
        std::printf(
            "[EffectManager] MeshVFX pool resource build failed: %s\n",
            resolvedAssetPath_.c_str());
        gameObjectManager->Destroy(object_);
        return nullptr;
    }
    meshVFX_->Stop();
    meshVFX_->Reset();
    meshVFX_->SetEnabled(false);
    gameObjectManager->DetachObject(object_);
    return object_;
}

std::shared_ptr<GameObject> EffectManager::AcquireVFXSequenceObject(
    const std::string& resolvedAssetPath_)
{
    const std::string assetName_ = BuildAssetNameFromPath(
        resolvedAssetPath_,
        "Data/VFXSequence");
    std::vector<std::shared_ptr<GameObject>>& objects_ =
        vfxSequenceObjectPool[assetName_];
    for (const std::shared_ptr<GameObject>& object_ : objects_)
    {
        if (!object_) continue;
        const std::shared_ptr<ParticleComponent> particle_ =
            object_->GetComponent<ParticleComponent>();
        const std::shared_ptr<MeshVFXComponent> meshVFX_ =
            object_->GetComponent<MeshVFXComponent>();
        const bool particleIdle_ = !particle_ || !particle_->IsEnabled();
        const bool meshIdle_ = !meshVFX_ || !meshVFX_->IsEnabled();
        if (particleIdle_ && meshIdle_)
        {
            gameObjectManager->AddObject(object_);
            return object_;
        }
    }

    std::printf(
        "[EffectManager] VFX sequence pool exhausted: %s size=%zu\n",
        resolvedAssetPath_.c_str(),
        objects_.size());
    return nullptr;
}

std::shared_ptr<GameObject> EffectManager::CreateVFXSequencePoolObject(
    const std::string& resolvedAssetPath_,
    const VFXSequenceAsset& asset_)
{
    if (!gameObjectManager) return nullptr;
    std::shared_ptr<GameObject> object_ =
        gameObjectManager->Create("PooledVFXSequence");
    if (!object_) return nullptr;
    object_->SetNoSaveObject();

    if (asset_.hasParticle)
    {
        const std::shared_ptr<ParticleComponent> particle_ =
            object_->AddComponent<ParticleComponent>();
        if (!particle_)
        {
            gameObjectManager->Destroy(object_);
            return nullptr;
        }
        particle_->SetAsset(asset_.particleAsset);
        particle_->ResetRuntime();
        if (!particle_->HasRuntimeResources())
        {
            std::printf(
                "[EffectManager] VFX sequence particle build failed: %s\n",
                resolvedAssetPath_.c_str());
            gameObjectManager->Destroy(object_);
            return nullptr;
        }
        particle_->SetEnabled(false);
    }

    if (asset_.hasMeshVFX)
    {
        const std::shared_ptr<MeshVFXComponent> meshVFX_ =
            object_->AddComponent<MeshVFXComponent>();
        if (!meshVFX_)
        {
            gameObjectManager->Destroy(object_);
            return nullptr;
        }
        meshVFX_->SetAsset(asset_.meshVFXAsset);
        meshVFX_->Stop();
        meshVFX_->Reset();
        if (!meshVFX_->HasRuntimeResources())
        {
            std::printf(
                "[EffectManager] VFX sequence mesh build failed: %s\n",
                resolvedAssetPath_.c_str());
            gameObjectManager->Destroy(object_);
            return nullptr;
        }
        meshVFX_->SetEnabled(false);
    }

    gameObjectManager->DetachObject(object_);
    return object_;
}

std::shared_ptr<GameObject> EffectManager::SpawnVFXSequence(
    const MeshVFXSpawnRequest& request_,
    const std::string& resolvedAssetPath_)
{
    const std::string assetName_ = BuildAssetNameFromPath(
        resolvedAssetPath_,
        "Data/VFXSequence");
    const auto assetIterator_ = vfxSequenceAssetCache.find(assetName_);
    if (assetIterator_ == vfxSequenceAssetCache.end()) return nullptr;

    std::shared_ptr<GameObject> object_ =
        AcquireVFXSequenceObject(resolvedAssetPath_);
    if (!object_) return nullptr;

    const DirectX::XMFLOAT3 spawnPosition_ = ApplyNormalOffset(
        request_.position,
        request_.normal,
        request_.normalOffset);
    Vector3 position_(spawnPosition_.x, spawnPosition_.y, spawnPosition_.z);
    object_->transform->SetLocalPosition(position_);
    object_->UpdateTransform();

    if (const std::shared_ptr<ParticleComponent> particle_ =
        object_->GetComponent<ParticleComponent>())
    {
        particle_->ResetRuntime();
        particle_->Stop();
        particle_->SetEnabled(true);
    }
    if (const std::shared_ptr<MeshVFXComponent> meshVFX_ =
        object_->GetComponent<MeshVFXComponent>())
    {
        meshVFX_->Stop();
        meshVFX_->Reset();
        meshVFX_->SetEnabled(true);
    }

    const VFXSequenceAsset& asset_ = assetIterator_->second;
    ManagedEffect effect_{};
    effect_.object = object_;
    effect_.assetPath = resolvedAssetPath_;
    effect_.poolType = EFFECT_POOL_TYPE::VFX_SEQUENCE;
    effect_.sequenceDuration = asset_.duration;
    effect_.sequenceLoop = asset_.loop;
    effect_.sequenceTracks = asset_.tracks;
    effect_.lifeTimer = request_.lifeTime > 0.0f
        ? request_.lifeTime
        : asset_.duration;
    effect_.lifeTimer = (std::max)(effect_.lifeTimer, 0.001f);
    managedEffects.push_back(std::move(effect_));
    UpdateVFXSequence(managedEffects.back());
    return object_;
}

void EffectManager::UpdateVFXSequence(ManagedEffect& effect_)
{
    const std::shared_ptr<GameObject> object_ = effect_.object.lock();
    if (!object_ || effect_.sequenceDuration <= 0.0f) return;

    if (effect_.sequenceLoop && effect_.sequenceTime >= effect_.sequenceDuration)
    {
        effect_.sequenceTime = std::fmod(
            effect_.sequenceTime,
            effect_.sequenceDuration);
        for (VFXSequenceTrack& track_ : effect_.sequenceTracks) track_.active = false;
        if (const std::shared_ptr<ParticleComponent> particle_ =
            object_->GetComponent<ParticleComponent>())
        {
            particle_->Stop();
            particle_->ResetRuntime();
        }
        if (const std::shared_ptr<MeshVFXComponent> meshVFX_ =
            object_->GetComponent<MeshVFXComponent>())
        {
            meshVFX_->Stop();
            meshVFX_->Reset();
        }
    }

    bool meshTrackActive_ = false;
    for (VFXSequenceTrack& track_ : effect_.sequenceTracks)
    {
        const bool shouldBeActive_ =
            effect_.sequenceTime >= track_.startTime &&
            effect_.sequenceTime < track_.startTime + track_.duration;
        if (track_.type == 1 && shouldBeActive_) meshTrackActive_ = true;

        if (shouldBeActive_ && !track_.active)
        {
            track_.active = true;
            if (track_.type == 0)
            {
                if (const std::shared_ptr<ParticleComponent> particle_ =
                    object_->GetComponent<ParticleComponent>())
                {
                    if (track_.targetName.empty() ||
                        !particle_->TriggerEmitter(track_.targetName))
                    {
                        particle_->Play();
                    }
                }
            }
            else if (track_.type == 1)
            {
                if (const std::shared_ptr<MeshVFXComponent> meshVFX_ =
                    object_->GetComponent<MeshVFXComponent>())
                {
                    meshVFX_->Reset();
                    meshVFX_->Play();
                }
            }
        }
        else if (!shouldBeActive_ && track_.active)
        {
            track_.active = false;
            if (track_.type == 0)
            {
                if (const std::shared_ptr<ParticleComponent> particle_ =
                    object_->GetComponent<ParticleComponent>())
                {
                    if (track_.targetName.empty()) particle_->Stop();
                    else particle_->StopEmitter(track_.targetName);
                }
            }
        }
    }

    if (!meshTrackActive_)
    {
        if (const std::shared_ptr<MeshVFXComponent> meshVFX_ =
            object_->GetComponent<MeshVFXComponent>())
        {
            meshVFX_->Stop();
        }
    }
}

void EffectManager::ReleasePooledEffect(ManagedEffect& effect_)
{
    const std::shared_ptr<GameObject> object_ = effect_.object.lock();
    if (!object_) return;

    if (const std::shared_ptr<ParticleComponent> particle_ =
        object_->GetComponent<ParticleComponent>())
    {
        particle_->Stop();
        particle_->ResetRuntime();
        particle_->SetEnabled(false);
    }
    if (const std::shared_ptr<MeshVFXComponent> meshVFX_ =
        object_->GetComponent<MeshVFXComponent>())
    {
        meshVFX_->Stop();
        meshVFX_->Reset();
        meshVFX_->SetEnabled(false);
    }
    if (const std::shared_ptr<HitBox> hitBox_ = object_->GetComponent<HitBox>())
    {
        hitBox_->Deactivate();
        hitBox_->SetEnabled(false);
    }
    if (const std::shared_ptr<ModelAttachmentComponent> attachment_ =
        object_->GetComponent<ModelAttachmentComponent>())
    {
        attachment_->SetEnabled(false);
    }
    if (object_->transform)
    {
        object_->transform->SetParent(nullptr);
        object_->transform->localPosition = { 0.0f, 0.0f, 0.0f };
        object_->transform->localRotation = { 0.0f, 0.0f, 0.0f };
        object_->transform->localScale = { 1.0f, 1.0f, 1.0f };
        object_->transform->MarkDirty();
    }
    if (gameObjectManager) gameObjectManager->DetachObject(object_);
}

void EffectManager::CollectJsonAssets(
    const std::string& rootPath_,
    std::vector<std::string>& paths_) const
{
    paths_.clear();

    const std::filesystem::path rootDirectory_ =
        WindowsDialogHelper::Utf8ToPath(rootPath_);
    std::error_code error_;
    if (!std::filesystem::is_directory(rootDirectory_, error_)) return;

    for (const std::filesystem::directory_entry& entry_ :
        std::filesystem::recursive_directory_iterator(rootDirectory_, error_))
    {
        if (error_) break;
        if (!entry_.is_regular_file(error_)) continue;
        if (entry_.path().extension() != ".json") continue;

        paths_.push_back(
            WindowsDialogHelper::PathToUtf8(entry_.path().lexically_normal()));
    }

    std::sort(paths_.begin(), paths_.end());
}

void EffectManager::BuildAssetNameMap(
    const std::vector<std::string>& paths_,
    const std::string& rootPath_,
    std::vector<EffectAssetEntry>& entries_,
    std::vector<std::string>& keys_,
    std::unordered_map<std::string, std::string>& pathByKey_) const
{
    entries_.clear();
    keys_.clear();
    pathByKey_.clear();

    for (const std::string& fullPath_ : paths_)
    {
        const std::filesystem::path filePath_ =
            WindowsDialogHelper::Utf8ToPath(fullPath_);
        const std::string fileName_ =
            WindowsDialogHelper::PathToUtf8(filePath_.filename());
        const std::string fileKey_ =
            WindowsDialogHelper::PathToUtf8(filePath_.stem());
        const std::string relativeKey_ = BuildAssetNameFromPath(
            fullPath_,
            rootPath_);

        std::string key_ = fileKey_;
        const auto duplicateIterator_ = pathByKey_.find(key_);
        if (duplicateIterator_ != pathByKey_.end() &&
            duplicateIterator_->second != fullPath_)
        {
            std::printf(
                "[EffectManager] Duplicate effect key '%s': %s / %s\n",
                key_.c_str(),
                duplicateIterator_->second.c_str(),
                fullPath_.c_str());
            key_ = relativeKey_;
        }

        EffectAssetEntry entry_{};
        entry_.key = key_;
        entry_.fileName = fileName_;
        entry_.fullPath = fullPath_;
        entries_.push_back(entry_);
        keys_.push_back(key_);

        RegisterAssetKey(key_, fullPath_, pathByKey_);
        RegisterAssetKey(relativeKey_, fullPath_, pathByKey_);
        RegisterAssetKey(fileName_, fullPath_, pathByKey_);
        RegisterAssetKey(fullPath_, fullPath_, pathByKey_);
    }
}

void EffectManager::RegisterAssetKey(
    const std::string& key_,
    const std::string& fullPath_,
    std::unordered_map<std::string, std::string>& pathByKey_) const
{
    if (key_.empty() || fullPath_.empty()) return;

    const auto iterator_ = pathByKey_.find(key_);
    if (iterator_ == pathByKey_.end())
    {
        pathByKey_[key_] = fullPath_;
        return;
    }

    if (iterator_->second != fullPath_)
    {
        std::printf(
            "[EffectManager] Effect key alias '%s' already points to %s, ignored %s\n",
            key_.c_str(),
            iterator_->second.c_str(),
            fullPath_.c_str());
    }
}

void EffectManager::PreloadParticleAsset(const std::string& path_)
{
    ParticleEffectAsset asset_{};
    if (!ParticleComponent::LoadAssetDataOnly(path_, asset_))
    {
        std::printf("[EffectManager] Failed to preload particle asset: %s\n", path_.c_str());
        return;
    }

    const std::string assetName_ = BuildAssetNameFromPath(path_, "Data/Particle");
    if (assetName_.empty()) return;

    particleAssetCache[assetName_] = asset_;
    std::vector<std::shared_ptr<GameObject>>& objects_ =
        particleObjectPool[assetName_];
    for (size_t index_ = 0; index_ < EFFECT_PREWARM_COUNT; ++index_)
    {
        std::shared_ptr<GameObject> object_ =
            CreateParticlePoolObject(path_, asset_);
        if (object_) objects_.push_back(object_);
    }
}

void EffectManager::PreloadMeshVFXAsset(const std::string& path_)
{
    MeshVFXComponent component_;
    if (!component_.LoadAsset(path_))
    {
        std::printf("[EffectManager] Failed to preload mesh vfx asset: %s\n", path_.c_str());
        return;
    }

    const std::string assetName_ = BuildAssetNameFromPath(path_, "Data/VFX");
    if (assetName_.empty()) return;

    meshVFXAssetCache[assetName_] = component_.GetAsset();
    const MeshVFXAsset& asset_ = meshVFXAssetCache[assetName_];
    std::vector<std::shared_ptr<GameObject>>& objects_ =
        meshVFXObjectPool[assetName_];
    for (size_t index_ = 0; index_ < EFFECT_PREWARM_COUNT; ++index_)
    {
        std::shared_ptr<GameObject> object_ =
            CreateMeshVFXPoolObject(path_, asset_);
        if (object_) objects_.push_back(object_);
    }
}

void EffectManager::PreloadVFXSequenceAsset(const std::string& path_)
{
    std::ifstream file_(WindowsDialogHelper::Utf8ToPath(path_));
    if (!file_.is_open())
    {
        std::printf("[EffectManager] Failed to open VFX sequence: %s\n", path_.c_str());
        return;
    }

    nlohmann::json json_;
    try
    {
        file_ >> json_;
    }
    catch (const nlohmann::json::exception&)
    {
        std::printf("[EffectManager] Failed to parse VFX sequence: %s\n", path_.c_str());
        return;
    }
    if (json_.value("AssetType", std::string{}) != "VFXSequence") return;

    VFXSequenceAsset asset_{};
    asset_.duration = (std::max)(json_.value("Duration", 1.0f), 0.001f);
    asset_.loop = json_.value("Loop", false);

    if (json_.contains("Particle") && json_["Particle"].is_object())
    {
        ParticleComponent component_;
        component_.Deserialize(json_["Particle"]);
        asset_.particleAsset = component_.GetAsset();
        asset_.hasParticle = !asset_.particleAsset.emitters.empty();
    }
    if (json_.contains("MeshVFX") && json_["MeshVFX"].is_object())
    {
        MeshVFXComponent component_;
        component_.Deserialize(json_["MeshVFX"]);
        asset_.meshVFXAsset = component_.GetAsset();
        asset_.hasMeshVFX = !asset_.meshVFXAsset.parts.empty();
    }
    if (json_.contains("Tracks") && json_["Tracks"].is_array())
    {
        for (const nlohmann::json& trackJson_ : json_["Tracks"])
        {
            VFXSequenceTrack track_{};
            track_.type = std::clamp(trackJson_.value("Type", 0), 0, 1);
            track_.targetName = trackJson_.value("Target", std::string{});
            track_.startTime = (std::max)(trackJson_.value("StartTime", 0.0f), 0.0f);
            track_.duration = (std::max)(trackJson_.value("Duration", 1.0f), 0.001f);
            asset_.tracks.push_back(std::move(track_));
        }
    }
    if (!asset_.hasParticle && !asset_.hasMeshVFX) return;
    if (asset_.tracks.empty())
    {
        if (asset_.hasParticle)
        {
            VFXSequenceTrack track_{};
            track_.type = 0;
            track_.duration = asset_.duration;
            asset_.tracks.push_back(track_);
        }
        if (asset_.hasMeshVFX)
        {
            VFXSequenceTrack track_{};
            track_.type = 1;
            track_.duration = asset_.duration;
            asset_.tracks.push_back(track_);
        }
    }

    const std::string assetName_ = BuildAssetNameFromPath(path_, "Data/VFXSequence");
    if (assetName_.empty()) return;
    vfxSequenceAssetCache[assetName_] = asset_;
    std::vector<std::shared_ptr<GameObject>>& objects_ =
        vfxSequenceObjectPool[assetName_];
    for (size_t index_ = 0; index_ < EFFECT_PREWARM_COUNT; ++index_)
    {
        std::shared_ptr<GameObject> object_ = CreateVFXSequencePoolObject(
            path_,
            asset_);
        if (object_) objects_.push_back(object_);
    }
}

void EffectManager::PreloadImpactEffectAsset(const std::string& path_)
{
    std::ifstream file_(WindowsDialogHelper::Utf8ToPath(path_));
    if (!file_.is_open())
    {
        std::printf("[EffectManager] Failed to open impact effect: %s\n", path_.c_str());
        return;
    }
    try
    {
        nlohmann::json json_;
        file_ >> json_;
        if (json_.value("AssetType", std::string{}) != "ImpactEffect") return;
        const std::string assetKey_ = BuildAssetNameFromPath(path_, "Data/ImpactEffect");
        if (!assetKey_.empty()) impactEffectAssetCache[assetKey_] = json_.get<ImpactEffectAsset>();
    }
    catch (const nlohmann::json::exception&)
    {
        std::printf("[EffectManager] Failed to parse impact effect: %s\n", path_.c_str());
    }
}

std::string EffectManager::BuildAssetNameFromPath(
    const std::string& path_,
    const std::string& rootPath_) const
{
    if (path_.empty()) return {};

    std::string normalizedPath_ = path_;
    std::replace(
        normalizedPath_.begin(),
        normalizedPath_.end(),
        '\\',
        '/');

    while (!normalizedPath_.empty() && normalizedPath_.front() == '/')
    {
        normalizedPath_.erase(normalizedPath_.begin());
    }

    std::string normalizedRootPath_ = rootPath_;
    std::replace(
        normalizedRootPath_.begin(),
        normalizedRootPath_.end(),
        '\\',
        '/');
    while (!normalizedRootPath_.empty() && normalizedRootPath_.front() == '/')
    {
        normalizedRootPath_.erase(normalizedRootPath_.begin());
    }

    const std::string rootPrefix_ = normalizedRootPath_ + "/";
    if (normalizedPath_.rfind(rootPrefix_, 0) == 0)
    {
        normalizedPath_.erase(0, rootPrefix_.size());
    }
    else
    {
        const std::string rootMarker_ = "/" + rootPrefix_;
        const size_t rootPosition_ = normalizedPath_.find(rootMarker_);
        if (rootPosition_ != std::string::npos)
        {
            normalizedPath_.erase(0, rootPosition_ + rootMarker_.size());
        }

        const std::string dataPrefix_ = "Data/";
        if (normalizedPath_.rfind(dataPrefix_, 0) == 0)
        {
            const size_t slashPosition_ = normalizedPath_.find('/', dataPrefix_.size());
            if (slashPosition_ != std::string::npos)
            {
                normalizedPath_.erase(0, slashPosition_ + 1);
            }
        }
    }

    const std::string extension_ = ".json";
    if (normalizedPath_.size() >= extension_.size())
    {
        std::string tail_ = normalizedPath_.substr(normalizedPath_.size() - extension_.size());
        std::transform(
            tail_.begin(),
            tail_.end(),
            tail_.begin(),
            [](unsigned char character_)
            {
                return static_cast<char>(std::tolower(character_));
            });

        if (tail_ == extension_)
        {
            normalizedPath_.erase(normalizedPath_.size() - extension_.size());
        }
    }

    return normalizedPath_;
}

const ParticleEffectAsset* EffectManager::FindParticleAssetFromRequest(
    const std::string& assetPath_) const
{
    const auto directIterator_ = particleAssetCache.find(assetPath_);
    if (directIterator_ != particleAssetCache.end()) return &directIterator_->second;

    const std::string assetName_ = BuildAssetNameFromPath(assetPath_, "Data/Particle");
    const auto iterator_ = particleAssetCache.find(assetName_);
    if (iterator_ == particleAssetCache.end()) return nullptr;

    return &iterator_->second;
}

const MeshVFXAsset* EffectManager::FindMeshVFXAssetFromRequest(
    const std::string& assetPath_) const
{
    const auto directIterator_ = meshVFXAssetCache.find(assetPath_);
    if (directIterator_ != meshVFXAssetCache.end()) return &directIterator_->second;

    const std::string assetName_ = BuildAssetNameFromPath(assetPath_, "Data/VFX");
    const auto iterator_ = meshVFXAssetCache.find(assetName_);
    if (iterator_ == meshVFXAssetCache.end()) return nullptr;

    return &iterator_->second;
}
