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

class VFXManager
{
public:
    void Initialize(GameObjectManager* gameObjectManager_);
    void Update(float deltaTime_);
    void Clear();

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
    DirectX::XMFLOAT3 ApplyNormalOffset(
        const DirectX::XMFLOAT3& position_,
        const DirectX::XMFLOAT3& normal_,
        float normalOffset_) const;
    void RegisterManagedEffect(
        const std::shared_ptr<GameObject>& object_,
        float lifeTime_);

private:
    GameObjectManager* gameObjectManager = nullptr;
    std::vector<ManagedEffect> managedEffects;
};
