#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <algorithm>
#include <cstdint>
#include <array>
#include <string>
#include <vector>

enum class HIT_REACTION_KIND
{
    // Hit後に選ぶReaction種別。地上/空中とImpactの強さで分岐する。
    GROUND_LIGHT,
    GROUND_MEDIUM,
    GROUND_HEAVY,
    AIR_MEDIUM,
    AIR_HEAVY,
    COUNT
};

class BlackboardComponent;

// 被弾時のダメージ反映、リアクションと短時間の赤いフラッシュを管理する。
class HitReactionComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "HitReactionComponent";
        return name_;
    }

    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    // AttackSystemからCombat/OnHit Action経由で呼ばれるHit処理入口。
    void HandleHit();
    // 描画時に実時間から被弾フラッシュの強さを求め、ヒットストップ中も自然に消す。
    float GetDamageFlashWeight() const;
    // HPとHit記録を初期値へ戻す。
    void ResetHealth();

    void SetReactionActionName(const std::string& actionName_)
    {
        reactionActionName = actionName_;
        SetDefaultReactionActionName(actionName_);
    }

    void SetDamageActionName(const std::string& actionName_)
    {
        damageActionName = actionName_;
    }

    void SetDefaultReactionActionName(const std::string& actionName_);
    // ImpactPowerからReaction種別を選ぶしきい値を設定する。
    void SetReactionImpactThresholds(
        float mediumImpactThreshold_,
        float heavyImpactThreshold_,
        float airHeavyImpactThreshold_);
    // 全Reaction共通で呼ぶActionを追加する。
    void AddCommonReactionAction(const std::string& actionName_);
    // 指定Reaction種別だけで呼ぶActionを追加する。
    void AddReactionAction(HIT_REACTION_KIND kind_, const std::string& actionName_);

    void SetArmor(float armor_)
    {
        armor = (std::max)(armor_, 0.0f);
    }

    float GetArmor() const
    {
        return armor;
    }

private:
    // OwnerへCombat/OnHit Actionを登録する。
    void RegisterActions();
    // OwnerのActionRegistryからAction名を実行する。
    void CallOwnerAction(const std::string& actionName_);
    void CallOwnerActions(const std::vector<std::string>& actionNames_);
    // Blackboard上の接地情報とImpact値からReaction種別を決める。
    HIT_REACTION_KIND DecideReactionKind(
        const BlackboardComponent& blackboard_,
        bool canReact_) const;
    std::vector<std::string>& GetReactionActions(HIT_REACTION_KIND kind_);
    const std::vector<std::string>& GetReactionActions(HIT_REACTION_KIND kind_) const;

private:
    // HPと直近Hit情報。
    float maxHealth = 100.0f;
    float currentHealth = 100.0f;
    float lastDamage = 0.0f;
    float lastImpactPower = 0.0f;
    int hitCount = 0;

    // ImpactPowerがArmor以下なら大きなReactionを出さない。
    float armor = 0.0f;
    // Reaction種別を分けるImpactしきい値。
    float mediumImpactThreshold = 20.0f;
    float heavyImpactThreshold = 50.0f;
    float airHeavyImpactThreshold = 50.0f;
    bool useHitStop = true;
    // 0ならAll LayerへHitStopをかける。
    uint32_t hitStopLayerMask = 0;
    bool useKnockBack = true;

    std::string damageActionName;
    std::string reactionActionName;
    // Hit時に常に呼ぶActionと、Reaction種別ごとに呼ぶAction。
    std::vector<std::string> commonReactionActionNames;
    std::array<std::vector<std::string>, static_cast<size_t>(HIT_REACTION_KIND::COUNT)> reactionActionNamesByKind;
    // Hit時のMaterial Overlay演出用Action。
    std::string overlayStartActionName = "MaterialOverlay_Start";
    std::string overlayStopActionName = "MaterialOverlay_Stop";
    float overlayDuration = 0.12f;
    float overlayTimer = 0.0f;

    // 保存する調整値と、保存しない実時間の発生時刻を分ける。
    bool damageFlashEnabled = false;
    float damageFlashDuration = 0.18f;
    uint64_t damageFlashStartTime = 0;
    bool damageFlashActive = false;

};
