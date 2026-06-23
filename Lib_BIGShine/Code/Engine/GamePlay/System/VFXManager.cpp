#include "VFXManager.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"

#include <DirectXMath.h>
#include <algorithm>

void VFXManager::Initialize(GameObjectManager* gameObjectManager_)
{
    gameObjectManager = gameObjectManager_;
}

void VFXManager::Update(float deltaTime_)
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

void VFXManager::Clear()
{
    managedEffects.clear();
}

std::shared_ptr<GameObject> VFXManager::SpawnParticle(
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

    const std::string assetPath_ = ResolveParticleAssetPath(request_.assetPath);

    if (!particle_->LoadAsset(assetPath_))
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

std::shared_ptr<GameObject> VFXManager::SpawnMeshVFX(
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

    const std::string assetPath_ = ResolveMeshVFXAssetPath(request_.assetPath);

    if (!meshVFX_->LoadAsset(assetPath_))
    {
        gameObjectManager->Destroy(object_);
        return nullptr;
    }

    meshVFX_->Reset();
    meshVFX_->Play();

    RegisterManagedEffect(object_, request_.lifeTime);
    return object_;
}

void VFXManager::SpawnForHit(const AttackHitEvent& event_)
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

DirectX::XMFLOAT3 VFXManager::ApplyNormalOffset(
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

void VFXManager::RegisterManagedEffect(
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

std::string VFXManager::AddJsonExtensionIfNeeded(const std::string& assetPath_) const
{
    if (assetPath_.empty()) return assetPath_;

    const size_t slashPosition_ = assetPath_.find_last_of('/');
    const size_t dotPosition_ = assetPath_.find_last_of('.');

    // ÅŒã‚Ì / ‚æ‚èŒã‚ë‚É . ‚ª‚ ‚ê‚ÎA‰½‚©‚µ‚çŠg’£Žq‚ ‚è‚Æ”»’f
    if (dotPosition_ != std::string::npos)
    {
        if (slashPosition_ == std::string::npos || dotPosition_ > slashPosition_)
        {
            return assetPath_;
        }
    }

    return assetPath_ + ".json";
}

std::string VFXManager::ResolveParticleAssetPath(const std::string& assetPath_) const
{
    std::string normalizedPath_ = NormalizeAssetPath(assetPath_);
    if (normalizedPath_.empty()) return normalizedPath_;

    normalizedPath_ = AddJsonExtensionIfNeeded(normalizedPath_);

    if (IsPathRooted(normalizedPath_))
    {
        return normalizedPath_;
    }

    if (normalizedPath_.rfind("Data/Particle/", 0) == 0)
    {
        return normalizedPath_;
    }

    if (normalizedPath_.rfind("Data/", 0) == 0)
    {
        return normalizedPath_;
    }

    return "Data/Particle/" + normalizedPath_;
}

std::string VFXManager::ResolveMeshVFXAssetPath(const std::string& assetPath_) const
{
    std::string normalizedPath_ = NormalizeAssetPath(assetPath_);
    if (normalizedPath_.empty()) return normalizedPath_;

    normalizedPath_ = AddJsonExtensionIfNeeded(normalizedPath_);

    if (IsPathRooted(normalizedPath_))
    {
        return normalizedPath_;
    }

    if (normalizedPath_.rfind("Data/VFX/", 0) == 0)
    {
        return normalizedPath_;
    }

    if (normalizedPath_.rfind("Data/", 0) == 0)
    {
        return normalizedPath_;
    }

    return "Data/VFX/" + normalizedPath_;
}

std::string VFXManager::NormalizeAssetPath(const std::string& assetPath_) const
{
    std::string normalizedPath_ = assetPath_;

    std::replace(
        normalizedPath_.begin(),
        normalizedPath_.end(),
        '\\',
        '/');

    while (!normalizedPath_.empty() &&
        (normalizedPath_.front() == '/' || normalizedPath_.front() == '\\'))
    {
        normalizedPath_.erase(normalizedPath_.begin());
    }

    return normalizedPath_;
}

bool VFXManager::IsPathRooted(const std::string& assetPath_) const
{
    if (assetPath_.size() >= 2 && assetPath_[1] == ':')
    {
        return true;
    }

    if (assetPath_.rfind("//", 0) == 0)
    {
        return true;
    }

    return false;
}