#pragma once

#include "Engine/Physics/HitBox.h"

#include <DirectXMath.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class GameObject;
class GuardCollider;

enum class GUARD_RESULT_TYPE
{
    NONE,
    NORMAL_GUARD,
    JUST_GUARD
};

struct GuardContact
{
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 normal = { 0.0f, 1.0f, 0.0f };
    bool isValid = false;
};

struct GuardEvent
{
    std::weak_ptr<GameObject> defender;
    std::weak_ptr<GameObject> attacker;

    GuardCollider* guardCollider = nullptr;
    HitBox* hitBox = nullptr;

    GUARD_RESULT_TYPE resultType = GUARD_RESULT_TYPE::NONE;
    ATTACK_SOURCE_TYPE attackSourceType = ATTACK_SOURCE_TYPE::MELEE;

    DirectX::XMFLOAT3 guardPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 attackNormal = { 0.0f, 1.0f, 0.0f };
    bool hasContact = false;

    uint32_t attackInstanceId = 0u;
    uint32_t hitGroupId = 0u;
    int teamId = 0;

    float damage = 0.0f;
    float impactPower = 0.0f;
    float knockBackPower = 0.0f;

    std::string sourceAction;
    std::string attackTag;
};

class GuardSystem
{
public:
    using GuardCallback = std::function<void(const GuardEvent&)>;

    void BeginFrame();
    void Clear();

    void ClearAttackInstance(uint32_t attackInstanceId_);
    void ClearAttackHitGroup(uint32_t attackInstanceId_, uint32_t hitGroupId_);

    bool HandleGuard(HitBox& hitBox_, GuardCollider& guardCollider_);
    bool HandleGuard(
        HitBox& hitBox_,
        GuardCollider& guardCollider_,
        const GuardContact& contact_);

    void AddGuardCallback(GuardCallback callback_);

    const std::vector<GuardEvent>& GetFrameGuardEvents() const
    {
        return frameGuardEvents;
    }

private:
    bool RegisterGuard(
        uint32_t attackInstanceId_,
        uint32_t hitGroupId_,
        const GameObject* defender_,
        bool allowMultiGuard_);

    bool IsSameHierarchy(
        const std::shared_ptr<GameObject>& objectA_,
        const std::shared_ptr<GameObject>& objectB_) const;

    std::shared_ptr<GameObject> GetHierarchyRoot(
        std::shared_ptr<GameObject> object_) const;

    void PublishDefenderGuardResult(const GuardEvent& event_);
    void PublishAttackerGuardResult(const GuardEvent& event_);
    void PublishNormalGuardToDefender(const GuardEvent& event_);
    void PublishJustGuardToDefender(const GuardEvent& event_);
    void PublishNormalGuardToAttacker(const GuardEvent& event_);
    void PublishJustGuardToAttacker(const GuardEvent& event_);

    std::string GetDefenderActionName(GUARD_RESULT_TYPE resultType_) const;
    std::string GetAttackerActionName(GUARD_RESULT_TYPE resultType_) const;
    std::shared_ptr<GameObject> FindBlackboardReceiver(std::shared_ptr<GameObject> object_) const;

private:
    std::vector<GuardEvent> frameGuardEvents;
    std::vector<GuardCallback> guardCallbacks;

    std::unordered_map<uint64_t, std::unordered_set<const GameObject*>> registeredGuardTargets;
};
