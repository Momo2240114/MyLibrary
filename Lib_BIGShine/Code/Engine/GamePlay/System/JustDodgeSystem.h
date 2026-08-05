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

// HitBoxとJustDodgeColliderが重なった時の接触情報。
struct JustDodgeContact
{
    // HitBox と JustDodgeCollider が重なった代表位置。演出の注視点として使う。
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };

    // 攻撃が来た向き。カメラ演出や反撃方向の入力になる。
    DirectX::XMFLOAT3 normal = { 0.0f, 1.0f, 0.0f };

    // 接触点が計算できなかった場合は、回避者位置などのフォールバックを使う。
    bool isValid = false;
};

// ジャスト回避成立時に発行するイベント情報。
// 回避者、攻撃者、攻撃ID、演出位置をまとめて保持する。
struct JustDodgeEvent
{
    std::weak_ptr<GameObject> dodger;
    std::weak_ptr<GameObject> attacker;

    // 成立時に関わった Collider。即時処理用なので所有はしない。
    JustDodgeCollider* justDodgeCollider = nullptr;
    HitBox* hitBox = nullptr;

    // Blackboard と CameraAction へ渡す演出用の位置・方向。
    DirectX::XMFLOAT3 dodgePosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 attackNormal = { 0.0f, 1.0f, 0.0f };
    bool hasContact = false;

    // 同じ攻撃の重複 JustDodge を防ぐための攻撃識別情報。
    uint32_t attackInstanceId = 0u;
    uint32_t hitGroupId = 0u;
    int teamId = 0;

    std::string sourceAction;
    std::string attackTag;
};

// JustDodgeColliderとHitBoxの重なりからジャスト回避成立を確定するSystem。
// 成立結果はBlackboard、CameraAction、Callbackへ流す。
class JustDodgeSystem
{
public:
    using JustDodgeCallback = std::function<void(const JustDodgeEvent&)>;

    void BeginFrame();
    void Clear();

    // 攻撃インスタンスが終了した時、重複回避防止の履歴を破棄する。
    void ClearAttackInstance(uint32_t attackInstanceId_);

    // 多段攻撃の特定 HitGroup だけ履歴をリセットしたい時に使う。
    void ClearAttackHitGroup(uint32_t attackInstanceId_, uint32_t hitGroupId_);

    // 接触点なしで JustDodge 判定を通知する互換用入口。
    void HandleJustDodge(
        HitBox& hitBox_,
        JustDodgeCollider& justDodgeCollider_);

    // CollisionStepSystem で計算した接触点つきで JustDodge 判定を通知する。
    void HandleJustDodge(
        HitBox& hitBox_,
        JustDodgeCollider& justDodgeCollider_,
            const JustDodgeContact& contact_);

    // 成立イベントを外部 System や Gameplay 側へ配るための購読登録。
    void AddJustDodgeCallback(JustDodgeCallback callback_);

    const std::vector<JustDodgeEvent>& GetFrameJustDodgeEvents() const
    {
        return frameJustDodgeEvents;
    }

private:
    // attackInstanceId + hitGroupId + dodgerで同じ攻撃の重複JustDodgeを防ぐ。
    bool RegisterJustDodge(
        uint32_t attackInstanceId_,
        uint32_t hitGroupId_,
        const GameObject* dodger_,
        bool allowMultiDodge_);

    bool IsSameHierarchy(
        const std::shared_ptr<GameObject>& objectA_,
        const std::shared_ptr<GameObject>& objectB_) const;

    // 子階層の Collider 同士でも、同じキャラクター所属か判断できるよう Root へたどる。
    std::shared_ptr<GameObject> GetHierarchyRoot(
        std::shared_ptr<GameObject> object_) const;

    // 回避成功情報を Blackboard に公開し、OnJustDodge Action を呼ぶ。
    void PublishJustDodgeToBlackboard(const JustDodgeEvent& event_);

private:
    // 今フレーム成立した JustDodge 一覧。BeginFrame で消える短命なイベント。
    std::vector<JustDodgeEvent> frameJustDodgeEvents;

    // 成立時に呼ぶ外部コールバック。
    std::vector<JustDodgeCallback> justDodgeCallbacks;

    // keyは上位32bitがattackInstanceId、下位32bitがhitGroupId。
    std::unordered_map<uint64_t, std::unordered_set<const GameObject*>> registeredDodgeTargets;
};
