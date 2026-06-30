#include "EffectManager.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Combat/AttackEffectHitComponent.h"
#include "Engine/GamePlay/Combat/AttackTemplateManager.h"
#include "Engine/GamePlay/Transform/ModelAttachmentComponent.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/Resource/GLTFModel.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"
#include "Engine/Physics/HitBox.h"
#include "Engine/Platform/WindowsDialogHelper.h"

#include <DirectXMath.h>
#include <algorithm>
#include <filesystem>

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
        { "knockBackPower", setting_.knockBackPower },
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
    setting_.knockBackPower = json_.value("knockBackPower", 3.0f);
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
            [this](const ManagedEffect& effect_)
            {
                const std::shared_ptr<GameObject> object_ = effect_.object.lock();
                if (!object_) return true;

                if (effect_.lifeTimer > 0.0f) return false;

                if (gameObjectManager)
                {
                    gameObjectManager->Destroy(object_);
                }
                return true;
            }),
        managedEffects.end());
}

void EffectManager::Clear()
{
    managedEffects.clear();
    scheduledAttackEffects.clear();
}

void EffectManager::RefreshAssetCache()
{
    CollectJsonAssets("Data/Particle", particleAssetPaths);
    CollectJsonAssets("Data/VFX", meshVFXAssetPaths);
}

std::shared_ptr<GameObject> EffectManager::SpawnParticle(
    const ParticleSpawnRequest& request_)
{
    if (!gameObjectManager || request_.assetPath.empty()) return nullptr;

    std::shared_ptr<GameObject> object_ =
        gameObjectManager->Create("RuntimeParticle");
    if (!object_) return nullptr;

    object_->SetNoSaveObject();

    const DirectX::XMFLOAT3 spawnPosition_ = ApplyNormalOffset(
        request_.position,
        request_.normal,
        request_.normalOffset);
    Vector3 position_(spawnPosition_.x, spawnPosition_.y, spawnPosition_.z);
    object_->transform->SetLocalPosition(position_);
    object_->UpdateTransform();

    std::shared_ptr<ParticleComponent> particle_ =
        object_->AddComponent<ParticleComponent>();
    if (!particle_) return object_;

    if (!particle_->LoadAsset(request_.assetPath))
    {
        gameObjectManager->Destroy(object_);
        return nullptr;
    }

    std::string emitterName_ = request_.emitterName;
    if (emitterName_.empty() && !particle_->GetTracks().empty())
    {
        emitterName_ = particle_->GetTracks().front().name;
    }

    if (!emitterName_.empty())
    {
        particle_->TriggerEmitterAtPosition(emitterName_, spawnPosition_);
    }
    else
    {
        particle_->Play();
    }

    RegisterManagedEffect(object_, request_.lifeTime);
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

        hitBox_ = object_->AddComponent<HitBox>();
        if (hitBox_)
        {
            hitBox_->actionPrefix = request_.setting.hitBoxName;
            hitBox_->SetOwner(request_.ownerObject);
            hitBox_->SetAttackInstanceId(attackInstanceId_);
            hitBox_->SetHitGroupId(request_.setting.hitGroupId);
            hitBox_->SetDamage(request_.setting.damage);
            hitBox_->SetKnockBackPower(request_.setting.knockBackPower);
            hitBox_->SetHitEffectPath(request_.setting.hitEffectPath);
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
        std::shared_ptr<ParticleComponent> particle_ = object_->AddComponent<ParticleComponent>();
        if (particle_ && particle_->LoadAsset(effectTemplate_.particlePath)) particle_->Play();
    }
    if (!effectTemplate_.meshVfxPath.empty())
    {
        std::shared_ptr<MeshVFXComponent> meshVFX_ = object_->AddComponent<MeshVFXComponent>();
        if (meshVFX_ && meshVFX_->LoadAsset(effectTemplate_.meshVfxPath))
        {
            meshVFX_->Reset();
            meshVFX_->Play();
        }
    }

    std::shared_ptr<AttackEffectHitComponent> attackHit_ = object_->AddComponent<AttackEffectHitComponent>();
    if (attackHit_)
    {
        attackHit_->SetOwner(ownerObject_);
        attackHit_->SetAttackInstanceId(attackInstanceId_);
        attackHit_->SetHitGroupId(attackTemplate_.hitGroupId);
        attackHit_->SetDamage(attackTemplate_.damage);
        attackHit_->SetHitStopDuration(attackTemplate_.hitStopDuration);
        attackHit_->SetHitStopScale(attackTemplate_.hitStopScale);
        attackHit_->SetKnockBackPower(attackTemplate_.knockBackPower);
        attackHit_->SetHitParticleAssetPath(attackTemplate_.hitEffect.hitParticleAssetPath);
        attackHit_->SetHitMeshVFXAssetPath(attackTemplate_.hitEffect.hitMeshVfxAssetPath);
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

    std::shared_ptr<GameObject> object_ =
        gameObjectManager->Create("RuntimeMeshVFX");
    if (!object_) return nullptr;

    object_->SetNoSaveObject();

    const DirectX::XMFLOAT3 spawnPosition_ = ApplyNormalOffset(
        request_.position,
        request_.normal,
        request_.normalOffset);
    Vector3 position_(spawnPosition_.x, spawnPosition_.y, spawnPosition_.z);
    object_->transform->SetLocalPosition(position_);
    object_->UpdateTransform();

    std::shared_ptr<MeshVFXComponent> meshVFX_ =
        object_->AddComponent<MeshVFXComponent>();
    if (!meshVFX_) return object_;

    if (!meshVFX_->LoadAsset(request_.assetPath))
    {
        gameObjectManager->Destroy(object_);
        return nullptr;
    }

    meshVFX_->Reset();
    meshVFX_->Play();

    RegisterManagedEffect(object_, request_.lifeTime);
    return object_;
}

void EffectManager::SpawnForHit(const AttackHitEvent& event_)
{
    if (!event_.hasHitContact) return;

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
    float hitEndTime_)
{
    if (!object_) return;

    ManagedEffect managedEffect_{};
    managedEffect_.object = object_;
    managedEffect_.hitBox = hitBox_;
    managedEffect_.lifeTimer = (std::max)(lifeTime_, 0.0f);
    if (managedEffect_.lifeTimer <= 0.0f)
    {
        managedEffect_.lifeTimer = 1.0f;
    }
    managedEffect_.hitStartTime = (std::max)(hitStartTime_, 0.0f);
    managedEffect_.hitEndTime = (std::max)(hitEndTime_, managedEffect_.hitStartTime);
    managedEffects.push_back(managedEffect_);
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
