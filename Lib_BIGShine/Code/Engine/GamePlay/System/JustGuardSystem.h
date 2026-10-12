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
class GuardCollider;
class HitBox;

// HitBoxとGuardColliderが重なった時のジャストガード用接触情報。
struct JustGuardContact
{
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 normal = { 0.0f, 1.0f, 0.0f };
    bool isValid = false;
};

// ジャストガード成立時に、反撃や演出へ渡すイベント情報。
struct JustGuardEvent
{
    std::weak_ptr<GameObject> defender;
    std::weak_ptr<GameObject> attacker;

    GuardCollider* guardCollider = nullptr;
    HitBox* hitBox = nullptr;

    DirectX::XMFLOAT3 guardPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 attackNormal = { 0.0f, 1.0f, 0.0f };
    bool hasContact = false;

    uint32_t attackInstanceId = 0u;
    uint32_t hitGroupId = 0u;
    int teamId = 0;

    std::string sourceAction;
    std::string attackTag;
};

// GuardColliderのJustGuard受付中に、HitBoxとの重なりから成立イベントを確定するSystem。
class JustGuardSystem
{
public:
    using JustGuardCallback = std::function<void(const JustGuardEvent&)>;

    void BeginFrame();
    void Clear();

    void ClearAttackInstance(uint32_t attackInstanceId_);
    void ClearAttackHitGroup(uint32_t attackInstanceId_, uint32_t hitGroupId_);

    bool HandleJustGuard(HitBox& hitBox_, GuardCollider& guardCollider_);
    bool HandleJustGuard(
        HitBox& hitBox_,
        GuardCollider& guardCollider_,
        const JustGuardContact& contact_);

    void AddJustGuardCallback(JustGuardCallback callback_);

    const std::vector<JustGuardEvent>& GetFrameJustGuardEvents() const
    {
        return frameJustGuardEvents;
    }

private:
    // attackInstanceId + hitGroupId + defenderで同じ攻撃の重複JustGuardを防ぐ。
    bool RegisterJustGuard(
        uint32_t attackInstanceId_,
        uint32_t hitGroupId_,
        const GameObject* defender_,
        bool allowMultiGuard_);

    bool IsSameHierarchy(
        const std::shared_ptr<GameObject>& objectA_,
        const std::shared_ptr<GameObject>& objectB_) const;

    // 子階層のColliderでも同じキャラクター所属か判定できるようRootへたどる。
    std::shared_ptr<GameObject> GetHierarchyRoot(
        std::shared_ptr<GameObject> object_) const;

    // 成立情報をBlackboardに公開し、OnJustGuardとReactionを呼ぶ。
    void PublishJustGuardToBlackboard(const JustGuardEvent& event_);
    void PublishJustGuardToAttacker(const JustGuardEvent& event_);
    // Projectile反射など、HitBox側に設定された既存の攻撃別Actionを維持する。
    void PublishJustGuardToAttackHitBox(const JustGuardEvent& event_);
    std::shared_ptr<GameObject> FindBlackboardReceiver(std::shared_ptr<GameObject> object_) const;

private:
    // 今フレーム成立したJustGuard一覧。BeginFrameで消える短命イベント。
    std::vector<JustGuardEvent> frameJustGuardEvents;

    // 成立時に呼ぶ外部コールバック。
    std::vector<JustGuardCallback> justGuardCallbacks;

    // keyは上位32bitがattackInstanceId、下位32bitがhitGroupId。
    std::unordered_map<uint64_t, std::unordered_set<const GameObject*>> registeredGuardTargets;
};
