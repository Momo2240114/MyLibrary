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

    // 指定位置と法線方向を基準にImpact Effectを発生させる。
    void SpawnAt(
        const DirectX::XMFLOAT3& position_,
        const DirectX::XMFLOAT3& normal_);
    // ImpactEffect Asset JSONを読み込む。
    bool LoadAsset(const std::string& path_);
    // 現在のAsset設定をJSONへ保存する。
    bool SaveAsset(const std::string& path_) const;
    const std::string& GetAssetPath() const { return assetPath; }
    ImpactEffectAsset& GetAsset() { return asset; }
    const ImpactEffectAsset& GetAsset() const { return asset; }

private:
    // 所有ObjectのColliderにCollision/Trigger Callbackを接続する。
    void BindColliders();
    // CollisionContactの位置/法線を使ってImpactを発生させる。
    void OnCollision(
        const std::shared_ptr<GameObject>& other_,
        const CollisionContact& contact_);
    // Trigger相手との位置関係から仮法線を作ってImpactを発生させる。
    void OnTrigger(const std::shared_ptr<GameObject>& other_);
    // Layer/CoolDown/Once条件からSpawn可能か判定する。
    bool CanSpawnFor(const std::shared_ptr<GameObject>& other_) const;

private:
    static inline const std::string componentName = "ImpactEffectComponent";
    // Component内に直接持つImpact Effect設定。
    ImpactEffectAsset asset;
    // 外部Asset JSONを使う場合のPath。
    std::string assetPath;
    // Callbackを既に登録したCollider集合。
    std::unordered_set<ColliderComponent*> boundColliders;
    // Impact発生対象にする相手Layer。
    uint32_t targetLayerMask = 0xFFFFFFFFu;
    // 連続HitでEffectが出すぎないようにする間隔。
    float coolDown = 0.1f;
    float coolDownTimer = 0.0f;
    bool spawnOnlyOnce = true;
    // trueならCollisionだけでなくTrigger Enterでも発生させる。
    bool listenToTriggers = false;
    bool hasSpawned = false;
};
