#pragma once

#include <DirectXMath.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class GameObject;
class HitBox;
class JustDodgeCollider;

struct JustDodgeContact
{
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 normal = { 0.0f, 1.0f, 0.0f };
    bool isValid = false;
};

struct JustDodgeEvent
{
    std::weak_ptr<GameObject> dodger;
    std::weak_ptr<GameObject> attacker;

    JustDodgeCollider* justDodgeCollider = nullptr;
    HitBox* hitBox = nullptr;

    DirectX::XMFLOAT3 dodgePosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 attackNormal = { 0.0f, 1.0f, 0.0f };
    bool hasContact = false;

    uint32_t attackInstanceId = 0u;
    uint32_t hitGroupId = 0u;
    int teamId = 0;

    std::string sourceAction;
    std::string attackTag;
};

class JustDodgeSystem
{
public:
    using JustDodgeCallback = std::function<void(const JustDodgeEvent&)>;

    void BeginFrame();
    void Clear();

    void ClearAttackInstance(uint32_t attackInstanceId_);
    void ClearAttackHitGroup(uint32_t attackInstanceId_, uint32_t hitGroupId_);

    void HandleJustDodge(
        HitBox& hitBox_,
        JustDodgeCollider& justDodgeCollider_);

    void HandleJustDodge(
        HitBox& hitBox_,
        JustDodgeCollider& justDodgeCollider_,
        const JustDodgeContact& contact_);

    void AddJustDodgeCallback(JustDodgeCallback callback_);

    const std::vector<JustDodgeEvent>& GetFrameJustDodgeEvents() const
    {
        return frameJustDodgeEvents;
    }

private:
    bool RegisterJustDodge(
        uint32_t attackInstanceId_,
        uint32_t hitGroupId_,
        const GameObject* dodger_,
        bool allowMultiDodge_);

    bool IsSameHierarchy(
        const std::shared_ptr<GameObject>& objectA_,
        const std::shared_ptr<GameObject>& objectB_) const;

    std::shared_ptr<GameObject> GetHierarchyRoot(
        std::shared_ptr<GameObject> object_) const;

    void PublishJustDodgeToBlackboard(const JustDodgeEvent& event_);

private:
    std::vector<JustDodgeEvent> frameJustDodgeEvents;
    std::vector<JustDodgeCallback> justDodgeCallbacks;

    std::unordered_map<uint64_t, std::unordered_set<const GameObject*>> registeredDodgeTargets;
};