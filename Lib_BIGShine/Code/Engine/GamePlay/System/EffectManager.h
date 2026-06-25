#pragma once

#include "AttackSystem.h"

#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>

class GameObject;
class GameObjectManager;

struct ParticleSpawnRequest
{
    std::string assetPath;
    std::string emitterName;
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 normal = { 0.0f, 1.0f, 0.0f };
    float lifeTime = 1.0f;
    float normalOffset = 0.0f;
};

struct MeshVFXSpawnRequest
{
    std::string assetPath;
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 normal = { 0.0f, 1.0f, 0.0f };
    float lifeTime = 1.0f;
    float normalOffset = 0.0f;
};

class EffectManager
{
public:
    void Initialize(GameObjectManager* gameObjectManager_);
    void Update(float deltaTime_);
    void Clear();
    void RefreshAssetCache();

    std::shared_ptr<GameObject> SpawnParticle(
        const ParticleSpawnRequest& request_);
    std::shared_ptr<GameObject> SpawnMeshVFX(
        const MeshVFXSpawnRequest& request_);
    void SpawnForHit(const AttackHitEvent& event_);

    const std::vector<std::string>& GetParticleAssetPaths() const
    {
        return particleAssetPaths;
    }

    const std::vector<std::string>& GetMeshVFXAssetPaths() const
    {
        return meshVFXAssetPaths;
    }

private:
    struct ManagedEffect
    {
        std::weak_ptr<GameObject> object;
        float lifeTimer = 0.0f;
    };

private:
    DirectX::XMFLOAT3 ApplyNormalOffset(
        const DirectX::XMFLOAT3& position_,
        const DirectX::XMFLOAT3& normal_,
        float normalOffset_) const;
    void RegisterManagedEffect(
        const std::shared_ptr<GameObject>& object_,
        float lifeTime_);
    void CollectJsonAssets(
        const std::string& rootPath_,
        std::vector<std::string>& paths_) const;

private:
    GameObjectManager* gameObjectManager = nullptr;
    std::vector<ManagedEffect> managedEffects;
    std::vector<std::string> particleAssetPaths;
    std::vector<std::string> meshVFXAssetPaths;
};
