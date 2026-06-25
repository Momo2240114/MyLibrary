#pragma once

#include "AttackSystem.h"
#include "Engine/Graphics/Particle/ParticleEffectAsset.h"
#include "Engine/Graphics/VFX/MeshVFXResource.h"

#include <DirectXMath.h>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class GameObject;
class GameObjectManager;

struct ParticleSpawnRequest
{
    // Asset cache name. Example: "Hit/Spark" for Data/Particle/Hit/Spark.json
    std::string assetName;

    // Compatibility field. This can be either old path or asset-name-like string.
    std::string assetPath;

    std::string emitterName;
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 normal = { 0.0f, 1.0f, 0.0f };
    float lifeTime = 1.0f;
    float normalOffset = 0.0f;
};

struct MeshVFXSpawnRequest
{
    // Asset cache name. Example: "Slash/Impact" for Data/VFX/Slash/Impact.json
    std::string assetName;

    // Compatibility field. This can be either old path or asset-name-like string.
    std::string assetPath;

    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 normal = { 0.0f, 1.0f, 0.0f };
    float lifeTime = 1.0f;
    float normalOffset = 0.0f;
};

class VFXManager
{
public:
    void Initialize(GameObjectManager* gameObjectManager_);
    void Update(float deltaTime_);
    void Clear();

    static void ReloadAssetCache();

    static const std::vector<std::string>& GetParticleAssetNames();
    static const std::vector<std::string>& GetMeshVFXAssetNames();

    static const ParticleEffectAsset* FindParticleAsset(
        const std::string& assetName_);
    static const MeshVFXAsset* FindMeshVFXAsset(
        const std::string& assetName_);

    std::shared_ptr<GameObject> SpawnParticle(
        const ParticleSpawnRequest& request_);
    std::shared_ptr<GameObject> SpawnMeshVFX(
        const MeshVFXSpawnRequest& request_);
    void SpawnForHit(const AttackHitEvent& event_);

private:
    struct ManagedEffect
    {
        std::weak_ptr<GameObject> object;
        float lifeTimer = 0.0f;
    };

private:
    static void LoadParticleAssetsFromDirectory(
        const std::string& rootDirectory_);
    static void LoadMeshVFXAssetsFromDirectory(
        const std::string& rootDirectory_);

    static std::string BuildAssetName(
        const std::filesystem::path& path_,
        const std::filesystem::path& rootDirectory_);
    static std::string BuildAssetNameFromRequestPath(
        const std::string& assetPath_,
        const std::string& rootDirectory_);

    static bool IsJsonFile(const std::filesystem::path& path_);
    static bool IsUnderDirectoryName(
        const std::filesystem::path& path_,
        const std::string& directoryName_);

    static const ParticleEffectAsset* FindParticleAssetFromRequest(
        const ParticleSpawnRequest& request_);
    static const MeshVFXAsset* FindMeshVFXAssetFromRequest(
        const MeshVFXSpawnRequest& request_);

private:
    DirectX::XMFLOAT3 ApplyNormalOffset(
        const DirectX::XMFLOAT3& position_,
        const DirectX::XMFLOAT3& normal_,
        float normalOffset_) const;
    void RegisterManagedEffect(
        const std::shared_ptr<GameObject>& object_,
        float lifeTime_);

private:
    static bool assetCacheLoaded;
    static std::unordered_map<std::string, ParticleEffectAsset> particleAssetMap;
    static std::unordered_map<std::string, MeshVFXAsset> meshVFXAssetMap;
    static std::vector<std::string> particleAssetNames;
    static std::vector<std::string> meshVFXAssetNames;

private:
    GameObjectManager* gameObjectManager = nullptr;
    std::vector<ManagedEffect> managedEffects;
};
