#pragma once

#include "AttackSystem.h"

#include <DirectXMath.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>

class GameObject;
class GameObjectManager;
class HitBox;

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

struct SlashEffectSpawnSetting
{
    std::string vfxPath;
    float spawnTime = 0.0f;
    float lifeTime = 0.18f;
    DirectX::XMFLOAT3 localOffset = { 0.0f, 1.0f, 1.2f };
    DirectX::XMFLOAT3 localRotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 localScale = { 1.0f, 1.0f, 1.0f };
    bool hasHitBox = false;
    float hitStartTime = 0.0f;
    float hitEndTime = 0.08f;
    float hitBoxRadius = 0.5f;
    float hitBoxHalfHeight = 1.0f;
    float damage = 10.0f;
    float knockBackPower = 3.0f;
    float hitStopDuration = 0.0f;
    float hitStopScale = 0.0f;
    std::string hitEffectPath;
    uint32_t hitGroupId = 0u;
    bool allowMultiHit = false;
    bool followOwner = false;
};

struct AttackEffectSpawnRequest
{
    SlashEffectSpawnSetting setting;
    GameObject* ownerObject = nullptr;
    uint32_t attackInstanceId = 0u;
};

void to_json(nlohmann::json& json_, const SlashEffectSpawnSetting& setting_);
void from_json(const nlohmann::json& json_, SlashEffectSpawnSetting& setting_);

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
    std::shared_ptr<GameObject> SpawnSlashEffect(
        const AttackEffectSpawnRequest& request_);
    std::shared_ptr<GameObject> SpawnAttackEffect(
        const AttackEffectSpawnRequest& request_);
    std::shared_ptr<GameObject> SpawnEffectWithHitBox(
        const AttackEffectSpawnRequest& request_);
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
        std::weak_ptr<HitBox> hitBox;
        float lifeTimer = 0.0f;
        float elapsedTime = 0.0f;
        float hitStartTime = 0.0f;
        float hitEndTime = 0.0f;
    };

    struct ScheduledAttackEffect
    {
        AttackEffectSpawnRequest request;
        float timeRemaining = 0.0f;
    };

private:
    DirectX::XMFLOAT3 ApplyNormalOffset(
        const DirectX::XMFLOAT3& position_,
        const DirectX::XMFLOAT3& normal_,
        float normalOffset_) const;
    void RegisterManagedEffect(
        const std::shared_ptr<GameObject>& object_,
        float lifeTime_,
        const std::shared_ptr<HitBox>& hitBox_ = nullptr,
        float hitStartTime_ = 0.0f,
        float hitEndTime_ = 0.0f);
    void CollectJsonAssets(
        const std::string& rootPath_,
        std::vector<std::string>& paths_) const;

private:
    GameObjectManager* gameObjectManager = nullptr;
    std::vector<ManagedEffect> managedEffects;
    std::vector<ScheduledAttackEffect> scheduledAttackEffects;
    std::vector<std::string> particleAssetPaths;
    std::vector<std::string> meshVFXAssetPaths;
};
