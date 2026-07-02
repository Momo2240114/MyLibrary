#include "VFXManager.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"

#include <DirectXMath.h>
#include <algorithm>
#include <cctype>
#include <filesystem>

bool VFXManager::assetCacheLoaded = false;
std::unordered_map<std::string, ParticleEffectAsset> VFXManager::particleAssetMap;
std::unordered_map<std::string, MeshVFXAsset> VFXManager::meshVFXAssetMap;
std::vector<std::string> VFXManager::particleAssetNames;
std::vector<std::string> VFXManager::meshVFXAssetNames;

void VFXManager::Initialize(GameObjectManager* gameObjectManager_)
{
    gameObjectManager = gameObjectManager_;

    if (!assetCacheLoaded)
    {
        ReloadAssetCache();
    }
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

void VFXManager::ReloadAssetCache()
{
    particleAssetMap.clear();
    meshVFXAssetMap.clear();
    particleAssetNames.clear();
    meshVFXAssetNames.clear();

    LoadParticleAssetsFromDirectory("Data/Particle");
    LoadMeshVFXAssetsFromDirectory("Data/VFX");

    std::sort(particleAssetNames.begin(), particleAssetNames.end());
    std::sort(meshVFXAssetNames.begin(), meshVFXAssetNames.end());

    assetCacheLoaded = true;
}

const std::vector<std::string>& VFXManager::GetParticleAssetNames()
{
    if (!assetCacheLoaded)
    {
        ReloadAssetCache();
    }

    return particleAssetNames;
}

const std::vector<std::string>& VFXManager::GetMeshVFXAssetNames()
{
    if (!assetCacheLoaded)
    {
        ReloadAssetCache();
    }

    return meshVFXAssetNames;
}

const ParticleEffectAsset* VFXManager::FindParticleAsset(
    const std::string& assetName_)
{
    if (!assetCacheLoaded)
    {
        ReloadAssetCache();
    }

    const auto iterator_ = particleAssetMap.find(assetName_);
    if (iterator_ == particleAssetMap.end()) return nullptr;

    return &iterator_->second;
}

const MeshVFXAsset* VFXManager::FindMeshVFXAsset(
    const std::string& assetName_)
{
    if (!assetCacheLoaded)
    {
        ReloadAssetCache();
    }

    const auto iterator_ = meshVFXAssetMap.find(assetName_);
    if (iterator_ == meshVFXAssetMap.end()) return nullptr;

    return &iterator_->second;
}

std::shared_ptr<GameObject> VFXManager::SpawnParticle(
    const ParticleSpawnRequest& request_)
{
    if (!gameObjectManager) return nullptr;

    const ParticleEffectAsset* asset_ = FindParticleAssetFromRequest(request_);
    if (!asset_) return nullptr;

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
    if (!particle_)
    {
        gameObjectManager->Destroy(object_);
        return nullptr;
    }

    particle_->SetAsset(*asset_);
    particle_->ResetRuntime();

    bool played_ = false;
    if (!request_.emitterName.empty())
    {
        played_ = particle_->TriggerEmitterAtPosition(
            request_.emitterName,
            spawnPosition_);
    }
    if (!played_)
    {
        particle_->Play();
    }

    RegisterManagedEffect(object_, request_.lifeTime);
    return object_;
}

std::shared_ptr<GameObject> VFXManager::SpawnMeshVFX(
    const MeshVFXSpawnRequest& request_)
{
    if (!gameObjectManager) return nullptr;

    const MeshVFXAsset* asset_ = FindMeshVFXAssetFromRequest(request_);
    if (!asset_) return nullptr;

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
    if (!meshVFX_)
    {
        gameObjectManager->Destroy(object_);
        return nullptr;
    }

    meshVFX_->SetAsset(*asset_);
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
        request_.assetName = event_.hitParticleAssetPath;
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
        request_.assetName = event_.hitMeshVFXAssetPath;
        request_.assetPath = event_.hitMeshVFXAssetPath;
        request_.position = event_.hitPosition;
        request_.normal = event_.hitNormal;
        request_.lifeTime = event_.hitMeshVFXLifeTime;
        request_.normalOffset = event_.hitMeshVFXNormalOffset;
        SpawnMeshVFX(request_);
    }
}

void VFXManager::LoadParticleAssetsFromDirectory(
    const std::string& rootDirectory_)
{
    namespace fs = std::filesystem;

    const fs::path rootPath_(rootDirectory_);
    std::error_code error_;
    if (!fs::exists(rootPath_, error_) || !fs::is_directory(rootPath_, error_)) return;

    for (const fs::directory_entry& entry_ : fs::recursive_directory_iterator(rootPath_, error_))
    {
        if (error_) break;
        if (!entry_.is_regular_file(error_)) continue;
        if (!IsJsonFile(entry_.path())) continue;
        if (IsUnderDirectoryName(entry_.path(), "Emitter")) continue;

        const std::string assetName_ = BuildAssetName(entry_.path(), rootPath_);
        if (assetName_.empty()) continue;

        ParticleEffectAsset asset_{};
        if (!ParticleComponent::LoadAssetDataOnly(entry_.path().generic_string(), asset_))
        {
            continue;
        }

        particleAssetMap[assetName_] = asset_;
        particleAssetNames.push_back(assetName_);
    }
}

void VFXManager::LoadMeshVFXAssetsFromDirectory(
    const std::string& rootDirectory_)
{
    namespace fs = std::filesystem;

    const fs::path rootPath_(rootDirectory_);
    std::error_code error_;
    if (!fs::exists(rootPath_, error_) || !fs::is_directory(rootPath_, error_)) return;

    for (const fs::directory_entry& entry_ : fs::recursive_directory_iterator(rootPath_, error_))
    {
        if (error_) break;
        if (!entry_.is_regular_file(error_)) continue;
        if (!IsJsonFile(entry_.path())) continue;
        if (IsUnderDirectoryName(entry_.path(), "Parts")) continue;

        MeshVFXComponent component_;
        if (!component_.LoadAsset(entry_.path().generic_string()))
        {
            continue;
        }

        const std::string assetName_ = BuildAssetName(entry_.path(), rootPath_);
        if (assetName_.empty()) continue;

        meshVFXAssetMap[assetName_] = component_.GetAsset();
        meshVFXAssetNames.push_back(assetName_);
    }
}

std::string VFXManager::BuildAssetName(
    const std::filesystem::path& path_,
    const std::filesystem::path& rootDirectory_)
{
    namespace fs = std::filesystem;

    fs::path relativePath_;
    try
    {
        relativePath_ = fs::relative(path_, rootDirectory_);
    }
    catch (...)
    {
        relativePath_ = path_.filename();
    }

    relativePath_.replace_extension();
    return relativePath_.generic_string();
}

std::string VFXManager::BuildAssetNameFromRequestPath(
    const std::string& assetPath_,
    const std::string& rootDirectory_)
{
    if (assetPath_.empty()) return {};

    std::string normalizedPath_ = assetPath_;
    std::replace(
        normalizedPath_.begin(),
        normalizedPath_.end(),
        '\\',
        '/');

    while (!normalizedPath_.empty() && normalizedPath_.front() == '/')
    {
        normalizedPath_.erase(normalizedPath_.begin());
    }

    const std::string rootPrefix_ = rootDirectory_ + "/";
    if (normalizedPath_.rfind(rootPrefix_, 0) == 0)
    {
        normalizedPath_.erase(0, rootPrefix_.size());
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

bool VFXManager::IsJsonFile(const std::filesystem::path& path_)
{
    std::string extension_ = path_.extension().string();
    std::transform(
        extension_.begin(),
        extension_.end(),
        extension_.begin(),
        [](unsigned char character_)
        {
            return static_cast<char>(std::tolower(character_));
        });

    return extension_ == ".json";
}

bool VFXManager::IsUnderDirectoryName(
    const std::filesystem::path& path_,
    const std::string& directoryName_)
{
    for (const std::filesystem::path& part_ : path_)
    {
        if (part_.generic_string() == directoryName_)
        {
            return true;
        }
    }

    return false;
}

const ParticleEffectAsset* VFXManager::FindParticleAssetFromRequest(
    const ParticleSpawnRequest& request_)
{
    if (!assetCacheLoaded)
    {
        ReloadAssetCache();
    }

    if (!request_.assetName.empty())
    {
        if (const ParticleEffectAsset* asset_ = FindParticleAsset(request_.assetName))
        {
            return asset_;
        }
    }

    if (!request_.assetPath.empty())
    {
        if (const ParticleEffectAsset* asset_ = FindParticleAsset(request_.assetPath))
        {
            return asset_;
        }

        const std::string assetName_ = BuildAssetNameFromRequestPath(
            request_.assetPath,
            "Data/Particle");
        if (const ParticleEffectAsset* asset_ = FindParticleAsset(assetName_))
        {
            return asset_;
        }
    }

    return nullptr;
}

const MeshVFXAsset* VFXManager::FindMeshVFXAssetFromRequest(
    const MeshVFXSpawnRequest& request_)
{
    if (!assetCacheLoaded)
    {
        ReloadAssetCache();
    }

    if (!request_.assetName.empty())
    {
        if (const MeshVFXAsset* asset_ = FindMeshVFXAsset(request_.assetName))
        {
            return asset_;
        }
    }

    if (!request_.assetPath.empty())
    {
        if (const MeshVFXAsset* asset_ = FindMeshVFXAsset(request_.assetPath))
        {
            return asset_;
        }

        const std::string assetName_ = BuildAssetNameFromRequestPath(
            request_.assetPath,
            "Data/VFX");
        if (const MeshVFXAsset* asset_ = FindMeshVFXAsset(assetName_))
        {
            return asset_;
        }
    }

    return nullptr;
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
