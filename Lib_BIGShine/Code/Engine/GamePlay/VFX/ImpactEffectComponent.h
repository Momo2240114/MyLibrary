#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/System/EffectManager.h"
#include "ImpactEffectAsset.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

class ColliderComponent;
class GameObject;
struct CollisionContact;

class ImpactEffectComponent : public Component
{
public:
    const std::string& GetName() const override { return componentName; }
    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void SpawnAt(
        const DirectX::XMFLOAT3& position_,
        const DirectX::XMFLOAT3& normal_);
    bool LoadAsset(const std::string& path_);
    bool SaveAsset(const std::string& path_) const;
    const std::string& GetAssetPath() const { return assetPath; }
    ImpactEffectAsset& GetAsset() { return asset; }
    const ImpactEffectAsset& GetAsset() const { return asset; }

private:
    void BindColliders();
    void OnCollision(
        const std::shared_ptr<GameObject>& other_,
        const CollisionContact& contact_);
    void OnTrigger(const std::shared_ptr<GameObject>& other_);
    bool CanSpawnFor(const std::shared_ptr<GameObject>& other_) const;

private:
    static inline const std::string componentName = "ImpactEffectComponent";
    ImpactEffectAsset asset;
    std::string assetPath;
    std::unordered_set<ColliderComponent*> boundColliders;
    uint32_t targetLayerMask = 0xFFFFFFFFu;
    float coolDown = 0.1f;
    float coolDownTimer = 0.0f;
    bool spawnOnlyOnce = true;
    bool listenToTriggers = false;
    bool hasSpawned = false;
};
