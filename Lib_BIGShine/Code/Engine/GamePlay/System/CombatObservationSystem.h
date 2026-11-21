#pragma once

#include "Engine/GamePlay/Core/Object/PlayerActionObservation.h"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class GameObject;
class Component;
class PlayerControllerComponent;
class AnimationControllerComponent;
class BlackboardComponent;
class HealthComponent;
class AttackSystem;
class GuardSystem;
class JustGuardSystem;

// 登録済みAIの確定状態を保持し、表示や他のAIからの参照に使う観測値。
struct CombatActorObservation
{
    bool valid = false;
    bool alive = true;
    bool aiEnabled = false;
    std::string animationState;
    std::string graphActionName;
    int dragonActionId = -1;
    DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
};

// Playerと敵AIの行動・戦闘結果を独立して監視し、Objectの寿命を延長しないSystem。
class CombatObservationSystem : public std::enable_shared_from_this<CombatObservationSystem>
{
public:
    // 生成・構成変更時だけComponent参照を取得し、毎Frameの全Object検索をなくす。
    void RegisterObject(const std::shared_ptr<GameObject>& object_);
    // 個体の破棄・移管時に、その個体の参照と履歴だけを解除する。
    void UnregisterObject(GameObject* object_);
    // 名前の変更でも個体の履歴を維持し、検索索引だけを付け替える。
    void NotifyObjectNameChanged(GameObject* object_, const std::string& oldName_);
    // Scene切り替えでは対象と履歴を解除し、Systemのイベント接続は維持する。
    void Reset();
    // 命中・Guard・JustGuardの結果を弱参照のCallbackで直接受け取る。
    void BindCombatEvents(AttackSystem& attack_, GuardSystem& guard_, JustGuardSystem& justGuard_);
    // AI更新前に一度だけ時刻を進め、登録済み対象の確定状態を取得する。
    void CaptureFrame(float deltaTime_);
    // LateUpdate前の再取得では時刻を進めず、更新後の状態だけを反映する。
    void RefreshSnapshots();
    // 初回取得と対象交換に使う登録済みObjectの索引。全Objectの再走査は行わない。
    std::shared_ptr<GameObject> FindObject(const std::string& name_) const;
    bool IsRegisteredObject(const std::shared_ptr<GameObject>& object_) const;
    // Playerレイヤーの登録済み候補だけを初回・失効時に参照する。
    std::shared_ptr<GameObject> FindPlayerObject(const std::shared_ptr<GameObject>& exclude_ = nullptr) const;

    std::uint64_t GetRegistrationSerial() const
    {
        return registrationSerial;
    }

    bool GetPlayerActionObservation(const std::shared_ptr<GameObject>& object_, PlayerActionObservation& observation_) const;
    std::uint64_t GetObservedHitCount(const std::shared_ptr<GameObject>& object_) const;
    bool GetPlayerActionObservation(const std::string& name_, PlayerActionObservation& observation_) const;
    bool GetActorActionObservation(const std::string& name_, CombatActorObservation& observation_) const;
    std::uint64_t GetObservedHitCount(const std::string& name_) const;
    // 既存の知覚上書きGUIを保持し、実入力や位置・生死は変更しない。
    void DrawPlayerActionObservationDebug(const std::string& name_);
    // Player知覚の上書きと登録済みAIの状態を専用ObjectのInspectorへ表示する。
    void Gui();

private:
    // 登録索引用の弱参照。Captureではこの全体索引を走査しない。
    struct ObjectReference
    {
        std::weak_ptr<GameObject> object;
        std::string name;
        bool playerLayer = false;
    };

    // Player専用の確定状態・履歴・Component参照。
    struct PlayerObservationCache
    {
        std::weak_ptr<GameObject> object;
        std::weak_ptr<PlayerControllerComponent> controller;
        std::weak_ptr<AnimationControllerComponent> animation;
        std::weak_ptr<BlackboardComponent> blackboard;
        std::weak_ptr<HealthComponent> health;
        PlayerActionObservation value;
        PlayerActionObservation debugValue;
        bool overrideEnabled = false;
        float attackStart = 0.0f;
        float guardStart = 0.0f;
        std::uint64_t contactsAtAttackStart = 0;
    };

    // Combat Actorだけを保持し、同名の別個体へ命中履歴を引き継がない。
    struct ActorRecord
    {
        std::weak_ptr<GameObject> object;
        std::weak_ptr<AnimationControllerComponent> animation;
        std::weak_ptr<HealthComponent> health;
        std::weak_ptr<Component> ai;
        CombatActorObservation value;
        std::uint64_t hits = 0;
        std::uint64_t contacts = 0;
    };

    void CapturePlayers();
    void CaptureActors();
    void RecordObservedContact(const std::shared_ptr<GameObject>& attacker_, bool hit_);
    void RemoveNameIndex(GameObject* object_, const std::string& name_);
    std::unordered_map<GameObject*, ObjectReference> objectReferences;
    std::unordered_map<std::string, std::vector<std::weak_ptr<GameObject>>> objectsByName;
    std::unordered_map<GameObject*, PlayerObservationCache> playerObservations;
    std::unordered_map<GameObject*, ActorRecord> actors;
    std::vector<std::weak_ptr<GameObject>> playerLayerObjects;
    std::uint64_t registrationSerial = 0;
    float observationClock = 0.0f;
    float observationHistorySeconds = 6.0f;
    bool combatEventsBound = false;
};
