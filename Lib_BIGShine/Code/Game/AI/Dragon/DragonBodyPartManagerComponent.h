#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class DragonBodyPartHurtBoxComponent;
class HealthComponent;
struct AttackHitEvent;

// Dragonのどの部位に当たったかを識別する列挙。
enum class DRAGON_BODY_PART
{
    HEAD,
    NECK,
    CHEST,
    BODY,
    LEFT_FRONT_LEG,
    RIGHT_FRONT_LEG,
    LEFT_BACK_LEG,
    RIGHT_BACK_LEG,
    LEFT_WING,
    RIGHT_WING,
    TAIL
};

// AttackSystemから受け取ったHit情報を、部位ダメージ計算用にまとめた構造体。
struct DamageContext
{
    // 同じ攻撃を識別するID。
    uint64_t attackInstanceId = 0;
    // 攻撃したGameObject。
    std::weak_ptr<GameObject> attacker;
    // ダメージを受ける本体側GameObject。
    std::weak_ptr<GameObject> targetCombatOwner;
    // HealthComponentへ適用する基礎ダメージ。
    float healthDamage = 0.0f;
    // 部位耐久へ適用する基礎ダメージ。
    float durabilityDamage = 0.0f;
    // HitしたWorld座標。
    DirectX::XMFLOAT3 hitPosition = { 0.0f, 0.0f, 0.0f };
    // Hit方向。
    DirectX::XMFLOAT3 hitDirection = { 0.0f, 0.0f, 1.0f };
};

// Dragonの1部位ごとの耐久、怯み、破壊時設定を保持する。
struct DragonBodyPartStatus
{
    // 対象部位。
    DRAGON_BODY_PART bodyPart = DRAGON_BODY_PART::BODY;
    // 部位耐久の最大値。
    float maxDurability = 100.0f;
    // 現在の部位耐久。
    float currentDurability = 100.0f;
    // Healthへ入るダメージ倍率。
    float healthDamageRate = 1.0f;
    // 部位耐久へ入るダメージ倍率。
    float durabilityDamageRate = 1.0f;
    // 部位破壊後にHealthへ入る追加ダメージ倍率。
    float brokenHealthDamageRate = 1.25f;
    // 怯み発生に必要な蓄積値の基準。
    float baseMaxStagger = 45.0f;
    // 現在の怯み発生しきい値。
    float currentMaxStagger = 45.0f;
    // 現在蓄積している怯み値。
    float currentStagger = 0.0f;
    // 怯み値へ入るダメージ倍率。
    float staggerDamageRate = 1.0f;
    // 怯み発生後、次の怯みしきい値を増やす倍率。
    float staggerRecoveryBaseMultiplier = 1.25f;
    // この部位が破壊された時に追加実行するAction名。
    std::string brokenActionName;
    // 部位耐久が0になり破壊済みか。
    bool broken = false;
};

// 部位ダメージ適用後に呼び出し元へ返す結果。
struct BodyPartDamageResult
{
    // ダメージを受けた部位。
    DRAGON_BODY_PART bodyPart = DRAGON_BODY_PART::BODY;
    // HealthComponentへ実際に適用したダメージ。
    float appliedHealthDamage = 0.0f;
    // 部位耐久へ実際に適用したダメージ。
    float appliedDurabilityDamage = 0.0f;
    // このHitで新しく部位破壊が発生したか。
    bool bodyPartBrokenThisHit = false;
    // Hit前から既に破壊済みだったか。
    bool alreadyBroken = false;
};

const char* GetDragonBodyPartName(DRAGON_BODY_PART bodyPart_);
DRAGON_BODY_PART ParseDragonBodyPart(const std::string& bodyPartName_);

// Dragon本体に付け、各部位の耐久・破壊・怯み・Health反映を管理するComponent。
// 子HurtBox側のDragonBodyPartHurtBoxComponentから部位情報を受け取り、本体側へ集約する。
class DragonBodyPartManagerComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "DragonBodyPartManagerComponent";
        return name_;
    }

    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    // 指定部位へダメージを適用し、Health、耐久、怯み、破壊Actionを更新する。
    BodyPartDamageResult ApplyDamage(
        DRAGON_BODY_PART bodyPart_,
        const DamageContext& damageContext_);
    // AttackSystemのHitEventを部位ダメージへ変換する。
    void HandleAttackHit(
        const AttackHitEvent& event_,
        const DragonBodyPartHurtBoxComponent& hurtBoxComponent_);

    // 指定部位の現在状態を取得する。
    const DragonBodyPartStatus* GetBodyPartStatus(
        DRAGON_BODY_PART bodyPart_) const;
    // 指定部位が破壊済みかを返す。
    bool IsBodyPartBroken(DRAGON_BODY_PART bodyPart_) const;
    // 全部位の耐久、怯み、破壊状態を初期化する。
    void ResetBodyParts();

private:
    // 足りない部位設定をデフォルト値で補完する。
    void EnsureDefaultBodyParts();
    // 指定部位の状態を編集可能なポインタで取得する。
    DragonBodyPartStatus* FindBodyPartStatus(DRAGON_BODY_PART bodyPart_);
    // OwnerのHealthComponentを取得または追加する。
    void ResolveHealthComponent();
    // AttackSystemのHit通知を購読する。
    void RegisterAttackHitCallback();
    // HealthComponentの現在HP割合をGameModeManagerへ反映する。
    void UpdateBossHpRate();
    // 最後に受けた部位ダメージ情報をBlackboardへ保存する。
    void PublishLastDamageToBlackboard();
    // 怯み値がしきい値を超えた場合にDamageReactionを要求する。
    void TryRequestDamageReaction(DragonBodyPartStatus& status_);
    // 部位破壊時の共通Actionと部位別Actionを実行する。
    void ExecuteBodyPartBrokenActions(const DragonBodyPartStatus& status_);

    // 管理対象の全部位状態。
    std::vector<DragonBodyPartStatus> bodyParts;
    // 本体HPへダメージを適用するHealthComponent。
    HealthComponent* health = nullptr;
    // AttackSystemへのHitCallback登録が済んでいるか。
    bool callbackRegistered = false;
    // Debugログ表示を有効にするか。
    bool debugLogEnabled = true;
    // 怯み値によるDamageReactionを有効にするか。
    bool reactionEnabled = true;
    // DamageReaction発生後の再発生待ち時間。
    float reactionCooldownTime = 2.0f;
    // DamageReactionの残りCooldown。
    float reactionCooldownTimer = 0.0f;
    // 怯み発生時にOwnerへ実行するAction名。
    std::string reactionActionName = "DragonAI_DamageReaction";
    // 部位破壊時に共通で実行するAction名。
    std::string commonBrokenActionName = "DragonBodyPart_Broken";
    // 直近Hitの結果。
    BodyPartDamageResult lastDamageResult;
    // 直近Hitの攻撃者名。
    std::string lastAttackerName;
    // 直近HitしたHurtBox名。
    std::string lastHurtBoxName;
    // 直近Hitの攻撃Instance ID。
    uint64_t lastAttackInstanceId = 0;
};
