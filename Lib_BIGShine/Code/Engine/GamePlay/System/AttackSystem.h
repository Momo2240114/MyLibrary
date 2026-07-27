#pragma once

#include <functional>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <DirectXMath.h>

class GameObject;
class HitBox;
class HurtBox;

struct AttackHitContact
{
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 normal = { 0.0f, 1.0f, 0.0f };
    bool isValid = false;
};

struct AttackHitEvent
{
    std::weak_ptr<GameObject> attacker;
    std::weak_ptr<GameObject> target;
    HitBox* hitBox = nullptr;
    HurtBox* hurtBox = nullptr;
    float damage = 0.0f;
    float hitStopDuration = 0.0f;
    float hitStopScale = 0.0f;
    uint32_t hitStopLayerMask = 0u;
    float impactPower = 0.0f;
    float knockBackPower = 0.0f;
    float targetAirLiftPower = 0.0f;
    float attackerAirLiftPower = 0.0f;
    DirectX::XMFLOAT3 hitPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 hitNormal = { 0.0f, 1.0f, 0.0f };
    bool hasHitContact = false;
    uint32_t attackInstanceId = 0u;
    uint32_t hitGroupId = 0u;
    std::string sourceAction;
    int teamId = 0;
    std::string attackTag;
    std::string hurtTag;
    std::string hitParticleAssetPath;
    std::string hitParticleEmitterName;
    float hitParticleLifeTime = 1.0f;
    float hitParticleNormalOffset = 0.0f;
    std::string hitMeshVFXAssetPath;
    float hitMeshVFXLifeTime = 1.0f;
    float hitMeshVFXNormalOffset = 0.0f;
    std::string hitSoundPath;
    float hitSoundVolume = 1.0f;
    bool hitSoundMuffled = false;
    float hitSoundMuffledCutoff = 1200.0f;
    bool cameraShakeEnabled = false;
    float cameraShakeReceiveDuration = 0.0f;
    float cameraShakeReceivePositionStrength = 0.0f;
    float cameraShakeReceiveRotationStrength = 0.0f;
    float cameraShakeAttackScale = 0.5f;
    float cameraShakeFrequency = 24.0f;
    int cameraShakePriority = 0;
};

class AttackSystem
{
public:
    using HitCallback = std::function<void(const AttackHitEvent&)>;

    void BeginFrame();
    void Clear();
    uint32_t CreateAttackInstance();
    void ClearAttackInstance(uint32_t attackInstanceId_);
    void ClearAttackHitGroup(uint32_t attackInstanceId_, uint32_t hitGroupId_);
    void HandleHit(HitBox& hitBox_, HurtBox& hurtBox_);
    void HandleHit(
        HitBox& hitBox_,
        HurtBox& hurtBox_,
        const AttackHitContact& contact_);
    void AddHitCallback(HitCallback callback_);

    const std::vector<AttackHitEvent>& GetFrameHitEvents() const
    {
        return frameHitEvents;
    }

private:
    bool RegisterAttackHit(
        uint32_t attackInstanceId_,
        uint32_t hitGroupId_,
        const GameObject* target_,
        bool allowMultiHit_);
    bool IsSameHierarchy(
        const std::shared_ptr<GameObject>& objectA_,
        const std::shared_ptr<GameObject>& objectB_) const;
    std::shared_ptr<GameObject> GetHierarchyRoot(
        std::shared_ptr<GameObject> object_) const;
    void PublishHitToBlackboard(const AttackHitEvent& event_);
    void PlayCameraShake(const AttackHitEvent& event_) const;

private:
    std::vector<AttackHitEvent> frameHitEvents;
    std::vector<HitCallback> hitCallbacks;
    std::unordered_map<uint64_t, std::unordered_set<const GameObject*>> registeredHitTargets;
    uint32_t nextAttackInstanceId = 1u;
};

