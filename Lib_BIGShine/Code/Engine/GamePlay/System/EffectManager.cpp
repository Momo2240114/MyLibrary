#include "EffectManager.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"
#include "Engine/Platform/WindowsDialogHelper.h"

#include <DirectXMath.h>
#include <algorithm>
#include <filesystem>

void EffectManager::Initialize(GameObjectManager* gameObjectManager_)
{
    gameObjectManager = gameObjectManager_;
    RefreshAssetCache();
}

void EffectManager::Update(float deltaTime_)
{
    const float safeDeltaTime_ = (std::max)(deltaTime_, 0.0f);

    for (ManagedEffect& effect_ : managedEffects)
    {
        effect_.lifeTimer -= safeDeltaTime_;
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
    float lifeTime_)
{
    if (!object_) return;

    ManagedEffect managedEffect_{};
    managedEffect_.object = object_;
    managedEffect_.lifeTimer = (std::max)(lifeTime_, 0.0f);
    if (managedEffect_.lifeTimer <= 0.0f)
    {
        managedEffect_.lifeTimer = 1.0f;
    }
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
