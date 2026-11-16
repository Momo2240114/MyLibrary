#include "PrecompiledHeader.h"
#include "Engine/GamePlay/System/CombatObservationSystem.h"
#include "HumanBossAIComponent.h"
#include "HumanBossActionComponent.h"
#include "HumanBossAITraceComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/Graphics/Resource/Animation/AnimationGraphBridgeComponent.h"
#include "Game/GameMode/GameModeManager.h"
#include "Game/GameMode/GameModeBlackboardKeys.h"
#include <imgui_stdlib.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include "Engine/GamePlay/Combat/AttackTemplateManager.h"
#include "Engine/GamePlay/EventSystem/ActionSequence.h"
#include "Engine/Math/Curve/CurveComponent.h"

namespace
{
    constexpr const char* STATE_NAMES[] = { "NEUTRAL", "REACTION", "OFFENSE", "REPOSITION", "SHADOW", "DRAGON_SHADOW", "SPECIAL" };
    constexpr const char* ACTION_NAMES[] =
    {
        "NORMAL_ATTACK_A", "NORMAL_ATTACK_B", "BACK_DODGE", "SIDE_DODGE", "GUARD", "JUST_GUARD",
        "COUNTER_ATTACK", "DELAY_ATTACK", "GUARD_BREAK", "SHADOW_SHIFT_BACK", "SHADOW_SHIFT_SIDE",
        "SHADOW_SHIFT_BEHIND", "SHADOW_SHIFT_APPROACH", "DRAGON_HEAD_BREATH", "DRAGON_BITE",
        "DRAGON_CLAW", "DRAGON_TAIL", "DRAGON_WING", "SPECIAL_ATTACK", "SPECIAL_COUNTER", "LIGHT_COUNTER", "WHIFF_PUNISH", "ATTACK_FEINT", "HEAVY_SINGLE_ATTACK", "SHADOW_SLASH", "SHADOW_DIVE_01", "SHADOW_DIVE_02", "SHADOW_DOMAIN", "SHADOW_CHARGE", "SHADOW_ORB", "SHADOW_RAGNAROK", "GUARD_EXECUTION"
    };
    // 不正な保存値は配列参照に使わず、未選択として表示する。
    const char* ActionName(int index_)
    {
        return index_ >= 0 && index_ < static_cast<int>(HumanBossActionType::COUNT) ? ACTION_NAMES[index_] : "NONE";
    }
    // 共通SkillのPhaseを作り、予備動作と打撃の実行方法をデータへまとめる。
    SkillPhase CreateComboPhase(const std::string& animation_, SkillPhaseEnd end_, float duration_,
        const std::string& sequence_ = {}, const std::string& enter_ = {}, const std::string& exit_ = {})
    {
        SkillPhase phase_;
        phase_.animationState = animation_;
        phase_.endCondition = end_;
        phase_.duration = duration_;
        // 短い補間で構えと振りを切り替え、段間の動きが長く混ざるのを防ぐ。
        phase_.blendDuration = animation_ == "SS_Guard" ? 0.02f : 0.04f;
        phase_.maximumDuration = 5.0f;
        phase_.sequence = sequence_;
        // 打撃Phaseの耐性を指定し、予備動作の保護は行動実行担当で別途判定する。
        phase_.preventFlinch = animation_ == "SS_Attack1" || animation_ == "SS_Attack2" ||
            animation_ == "HumanBossComboFinisher" || animation_ == "HumanBossGuardBreak";
        phase_.enterAction = enter_;
        phase_.exitAction = exit_;
        return phase_;
    }
    // 段番号と既存Action種別から初期プリセットを選び、未設定の古いJSONだけを補う。
    std::string GetDefaultAttackWarpPreset(const std::string& animation_, HumanBossActionType type_)
    {
        if (animation_.compare(0, 13, "Combo_Attack_") == 0 && animation_.size() >= 18)
        {
            if (animation_.substr(16, 2) == "01") return "HumanBoss.ComboFirst";
            // 3・4段目も短い接近補正を使い、自然移動だけで届かず中断する状態を避ける。
            return "HumanBoss.ComboSecond";
        }
        if (animation_ == "SS_Sp_Skill1") return "HumanBoss.Counter";
        if (animation_ == "SS_Sp_Skill3") return "HumanBoss.Heavy";
        if (animation_ == "HumanBossGuardBreak") return "HumanBoss.GuardBreak";
        if (animation_ == "HumanBossCounter") return "HumanBoss.Counter";
        if (animation_ == "SS_Attack1" || animation_ == "SS_Attack2")
        {
            if (type_ == HumanBossActionType::WHIFF_PUNISH) return "HumanBoss.Punish";
            if (type_ == HumanBossActionType::LIGHT_COUNTER) return "HumanBoss.Counter";
            return "HumanBoss.Delay";
        }
        return {};
    }
    bool IsDodge(HumanBossActionType type_)
    {
        return type_ == HumanBossActionType::BACK_DODGE || type_ == HumanBossActionType::SIDE_DODGE;
    }
    // 反撃専用行動を、通常攻撃の待機時間と連続使用制限から区別する。
    bool IsCounterAttack(HumanBossActionType type_)
    {
        return type_ == HumanBossActionType::COUNTER_ATTACK || type_ == HumanBossActionType::LIGHT_COUNTER;
    }
    // 開始可否の確認と実行で同じ防御種別を使用する。
    HumanBossDefense GetDefense(HumanBossActionType type_)
    {
        if (IsDodge(type_)) return HumanBossDefense::DODGE;
        if (type_ == HumanBossActionType::GUARD) return HumanBossDefense::GUARD;
        if (type_ == HumanBossActionType::JUST_GUARD) return HumanBossDefense::JUST_GUARD;
        return HumanBossDefense::NONE;
    }
    // 基本の影渡りは列挙順、派生攻撃は保存された移動方向を使用する。
    HumanBossShadowShift GetShadowShiftMode(const HumanBossActionDefinition& definition_)
    {
        const int mode_ = definition_.type >= HumanBossActionType::SHADOW_SLASH
            ? definition_.shadowShiftMode
            : static_cast<int>(definition_.type) - static_cast<int>(HumanBossActionType::SHADOW_SHIFT_BACK);
        return static_cast<HumanBossShadowShift>(mode_);
    }
    // 選択済みのパターンだけを実行し、段間の通知は一つのActionSequenceに任せる。
    SkillDefinition CreatePatternSkill(const std::string& sequence_, const std::vector<std::string>& animations_)
    {
        SkillDefinition skill_;
        skill_.name = sequence_;
        skill_.category = SkillCategory::ATTACK;
        skill_.cleanupAction = "HumanBoss_WindupEnd";
        for (std::size_t index_ = 0; index_ < animations_.size(); ++index_)
        {
            SkillPhase phase_ = CreateComboPhase(animations_[index_],
                index_ + 1 == animations_.size() ? SkillPhaseEnd::ANIMATION_END : SkillPhaseEnd::ACTION_END,
                0.0f, sequence_);
            phase_.continueSequence = index_ > 0;
            phase_.preventFlinch = true;
            phase_.maximumDuration = 10.0f;
            phase_.blendDuration = animations_[index_] == "SS_Sp_Skill3" ? 0.08f : 0.04f;
            skill_.phases.push_back(phase_);
        }
        return skill_;
    }
}

HumanBossAIComponent::HumanBossAIComponent()
{
    // AIはMovementより先に予約する。Animation/Eventは既存の更新順を維持する。
    SetUpdatePriority(-20);
    justGuardReactionSkill.name = "HumanBoss.JustGuardRecoil";
    justGuardReactionSkill.category = SkillCategory::UTILITY;
    SkillPhase recoil_;
    recoil_.animationState = "Block_Hit_Break_Seq";
    recoil_.sequence = "HumanBoss_JustGuardRecoil";
    recoil_.endCondition = SkillPhaseEnd::ACTION_END;
    recoil_.playAnimationFromSequence = true;
    recoil_.waitForAnimationEnd = true;
    recoil_.maximumDuration = 5.0f;
    justGuardReactionSkill.phases = { recoil_ };
    for (int index_ = 0; index_ < static_cast<int>(HumanBossActionType::COUNT); ++index_)
    {
        HumanBossActionDefinition definition_;
        definition_.type = static_cast<HumanBossActionType>(index_);
        definition_.skill.name = ACTION_NAMES[index_];
        definition_.enabled = { false, false, false };
        if (index_ >= 2 && index_ <= 5) definition_.category = HumanBossState::REACTION;
        if (index_ >= 9 && index_ <= 12) definition_.category = HumanBossState::SHADOW;
        if (index_ >= 13 && index_ <= 17) definition_.category = HumanBossState::DRAGON_SHADOW;
        if (index_ >= 18 && index_ <= 19) definition_.category = HumanBossState::SPECIAL;
        actions.push_back(definition_);
    }
    const char* ANIMATIONS[] = { "SS_Attack1", "SS_Attack2", "SS_Dodge", "SS_Dodge", "SS_Guard", "Parry_L_Seq", "HumanBossCounter" };
    const float WEIGHTS[] = { 1.0f, 1.0f, 40.0f, 15.0f, 20.0f, 10.0f, 1.0f };
    for (int index_ = 0; index_ < 7; ++index_)
    {
        auto& definition_ = actions[index_];
        definition_.enabled = { true, true, true };
        definition_.weight.fill(WEIGHTS[index_]);
        definition_.cooldown.fill(index_ == 5 ? 2.5f : 1.2f);
        SkillPhase phase_;
        phase_.animationState = ANIMATIONS[index_];
        phase_.endCondition = SkillPhaseEnd::ANIMATION_END;
        phase_.maximumDuration = 5.0f;
        if (index_ == 4)
        {
            phase_.endCondition = SkillPhaseEnd::DURATION;
            phase_.duration = 0.7f;
            phase_.enterAction = "EnableGuardCollider";
            phase_.exitAction = "DisableGuardCollider";
        }
        if (index_ == 6) phase_.preventFlinch = true;
        if (index_ == 2 || index_ == 3)
        {
            // RootMotion回避は固定秒数で打ち切らず、クリップの終了まで移動を引き受ける。
            phase_.endCondition = SkillPhaseEnd::ANIMATION_END;
            phase_.duration = 0.75f;
        }
        definition_.skill.phases.push_back(phase_);
    }
    actions[0].skill.category = SkillCategory::ATTACK;
    actions[1].skill.category = SkillCategory::ATTACK;
    // 実在する二つの4段パターンを使い、他のパターンを抽選へ無理に追加しない。
    actions[0].skill = CreatePatternSkill("HumanBoss_COMBO_PATTERN_01",
        { "Combo_Attack_01_01_Seq", "Combo_Attack_01_02_Seq", "Combo_Attack_01_03_Seq", "Combo_Attack_01_04_Seq" });
    actions[1].skill = CreatePatternSkill("HumanBoss_COMBO_PATTERN_02",
        { "Combo_Attack_02_01_Seq", "Combo_Attack_02_02_Seq", "Combo_Attack_02_03_Seq", "Combo_Attack_02_04_Seq" });
    auto& heavy_ = actions[static_cast<std::size_t>(HumanBossActionType::HEAVY_SINGLE_ATTACK)];
    heavy_.enabled = { true, true, true };
    heavy_.weight = { 0.35f, 0.5f, 0.65f };
    heavy_.cooldown = { 8.0f, 7.0f, 6.0f };
    // 開始ActionとSequenceは同名にせず、Registryの名前索引での競合を避ける。
    heavy_.skill = CreatePatternSkill("HumanBoss_HEAVY_SINGLE_ATTACK_Sequence", { "SS_Sp_Skill3" });
    actions[0].cooldown = { 2.5f, 2.0f, 1.5f };
    actions[1].cooldown = { 3.0f, 2.5f, 2.0f };
    actions[5].enabled = { true, true, true };
    actions[5].weight = { 3.0f, 7.0f, 9.0f };
    // 強攻撃を続ける相手への防御機会を、長い再使用待ちで失わないようにする。
    actions[5].cooldown = { 2.5f, 2.25f, 2.0f };
    actions[5].skill.phases.front().blendDuration = 0.03f;
    actions[6].enabled = { true, true, true };
    actions[6].cooldown = { 5.0f, 5.0f, 4.0f };
    actions[6].skill.category = SkillCategory::COUNTER;
    actions[6].skill.cleanupAction = "HumanBoss_ExecutionRelease";
    actions[6].skill.phases = { CreateComboPhase("Execution_03_Seq", SkillPhaseEnd::DURATION, 1.92f, {}, "HumanBoss_ExecutionCapture") };
    // 読み合い専用の行動を既存Skill Phaseへ接続し、攻撃実行を複製しない。
    actions[7].enabled = { false, true, true };
    actions[7].cooldown = { 4.0f, 4.0f, 3.0f };
    actions[7].skill.category = SkillCategory::ATTACK;
    actions[7].skill.cleanupAction = "HumanBoss_WindupEnd";
    actions[7].skill.phases =
    {
        CreateComboPhase("HumanBossAttackTell", SkillPhaseEnd::DURATION, 0.16f, "HumanBoss_Windup", {}, "HumanBoss_WindupEnd"),
        CreateComboPhase("SS_Attack1", SkillPhaseEnd::ACTION_END, 0.0f, "HumanBoss_StrikeSetup"),
        CreateComboPhase("SS_Guard", SkillPhaseEnd::DURATION, 0.24f, {}, "HumanBoss_WindupBegin", "HumanBoss_WindupEnd"),
        CreateComboPhase("SS_Attack2", SkillPhaseEnd::ANIMATION_END, 0.0f, "HumanBoss_StrikeSetup")
    };
    actions[8].enabled = { false, true, true };
    actions[8].cooldown = { 6.0f, 6.0f, 5.0f };
    actions[8].skill.category = SkillCategory::ATTACK;
    actions[8].skill.cleanupAction = "HumanBoss_WindupEnd";
    actions[8].skill.phases =
    {
        CreateComboPhase("HumanBossAttackTell", SkillPhaseEnd::DURATION, 0.4f, "HumanBoss_Windup", {}, "HumanBoss_WindupEnd"),
        CreateComboPhase("HumanBossGuardBreak", SkillPhaseEnd::ANIMATION_END, 0.0f, "HumanBoss_StrikeSetup")
    };
    actions[20].enabled = { true, true, true };
    actions[20].cooldown = { 3.0f, 3.0f, 2.5f };
    actions[20].skill.category = SkillCategory::COUNTER;
    actions[20].skill.phases = { CreateComboPhase("SS_Attack2", SkillPhaseEnd::ANIMATION_END, 0.0f, "HumanBoss_StrikeSetup") };
    actions[21].enabled = { true, true, true };
    actions[21].maxDistance = 5.0f;
    actions[21].cooldown = { 3.0f, 2.5f, 2.0f };
    actions[21].skill.category = SkillCategory::ATTACK;
    actions[21].skill.phases = { CreateComboPhase("SS_Attack2", SkillPhaseEnd::ANIMATION_END, 0.0f, "HumanBoss_StrikeSetup") };
    actions[22].enabled = { false, true, true };
    actions[22].cooldown = { 8.0f, 8.0f, 7.0f };
    actions[22].skill.cleanupAction = "HumanBoss_WindupEnd";
    actions[22].skill.phases =
    {
        CreateComboPhase("HumanBossAttackTell", SkillPhaseEnd::DURATION, 0.18f, "HumanBoss_Windup", {}, "HumanBoss_WindupEnd"),
        CreateComboPhase("SS_Guard", SkillPhaseEnd::DURATION, 0.3f, {}, "HumanBoss_WindupBegin", "HumanBoss_WindupEnd")
    };
    // 影渡りは攻撃のない移動Skillとして接続し、開始と復帰の時間を確保する。
    for (int index_ = 9; index_ <= 12; ++index_)
    {
        auto& definition_ = actions[index_];
        definition_.enabled = { true, true, true };
        definition_.weight = { 0.7f, 1.0f, 1.3f };
        definition_.cooldown = { 6.0f, 5.0f, 4.0f };
        definition_.minDistance = index_ == 12 ? 3.0f : 0.5f;
        definition_.maxDistance = index_ == 12 ? 22.0f : 12.0f;
        definition_.skill.category = SkillCategory::UTILITY;
        definition_.skill.phases = { CreateComboPhase("HumanBossCombatWalk", SkillPhaseEnd::DURATION, 1.5f) };
    }
    // 移動と派生を一つのSkillへまとめ、通常BTに途中で制御を返さない。
    for (int index_ = 24; index_ <= 27; ++index_)
    {
        auto& definition_ = actions[index_];
        definition_.category = HumanBossState::SHADOW;
        definition_.enabled = { true, true, true };
        definition_.weight = { 0.35f, 0.5f, 0.65f };
        definition_.cooldown.fill(index_ == 27 ? 12.0f : index_ == 24 ? 6.0f : 9.0f);
        definition_.minDistance = 1.0f;
        definition_.maxDistance = index_ == 27 ? 12.0f : 22.0f;
        // 単独の横・後退距離を広げても、派生は対象の近くへ再出現できる方向を選ぶ。
        definition_.shadowShiftMode = index_ == 24 ? 3 : 2;
        definition_.skill.category = SkillCategory::UTILITY;
        definition_.skill.cleanupAction = "HumanBoss_ShadowClear";
        definition_.skill.phases = { CreateComboPhase("HumanBossCombatWalk", SkillPhaseEnd::DURATION, 1.5f) };
        if (index_ == 24)
        {
            definition_.skill.phases.push_back(CreateComboPhase("HumanBossAttackTell", SkillPhaseEnd::DURATION, 0.18f));
            auto slash_ = CreateComboPhase("SS_Sp_Skill1", SkillPhaseEnd::ANIMATION_END, 0.0f, "HumanBoss_SHADOW_SLASH_Sequence");
            slash_.motionWarpPreset = "HumanBoss.Counter";
            slash_.preventFlinch = true;
            definition_.skill.phases.push_back(slash_);
        }
        else if (index_ == 27)
        {
            definition_.skill.phases.push_back(CreateComboPhase("Buff_Seq", SkillPhaseEnd::DURATION, 1.5f, "HumanBoss_SHADOW_DOMAIN_Sequence"));
            auto strike_ = CreateComboPhase("Buff_Seq", SkillPhaseEnd::DURATION, 0.25f, {}, "HumanBoss_ShadowStrike");
            strike_.animationStartTime = 1.25f;
            definition_.skill.phases.push_back(strike_);
        }
        else
        {
            const std::string prefix_ = index_ == 25 ? "Attack_Air_to_Floor_01_" : "Attack_Air_to_Floor_02_";
            auto start_ = CreateComboPhase(prefix_ + "Start_Seq", SkillPhaseEnd::ANIMATION_END, 0.0f, {}, "HumanBoss_ShadowDiveStart");
            start_.warpHeight = index_ == 25 ? 4.0f : 6.0f;
            start_.suspendGravity = true;
            definition_.skill.phases.push_back(start_);
            auto hover_ = CreateComboPhase(prefix_ + "Loop_Seq", SkillPhaseEnd::DURATION, index_ == 25 ? 0.45f : 0.7f,
                index_ == 25 ? "HumanBoss_SHADOW_DIVE_01_Sequence" : "HumanBoss_SHADOW_DIVE_02_Sequence");
            hover_.suspendGravity = true;
            definition_.skill.phases.push_back(hover_);
            auto fall_ = CreateComboPhase(prefix_ + "Loop_Seq", SkillPhaseEnd::GROUND_CONTACT, 0.1f);
            fall_.suspendGravity = true;
            // 落下を約2.3倍へ速め、接地確認後にEndと着地攻撃へ接続する。
            fall_.verticalSpeed = index_ == 25 ? -27.6f : -23.0f;
            fall_.maximumDuration = 3.0f;
            definition_.skill.phases.push_back(fall_);
            definition_.skill.phases.push_back(CreateComboPhase(prefix_ + "End_Seq", SkillPhaseEnd::ANIMATION_END, 0.0f, {}, "HumanBoss_ShadowStrike"));
        }
        definition_.skill.phases.push_back(CreateComboPhase("SS_Idle", SkillPhaseEnd::DURATION, index_ == 27 ? 0.8f : 0.25f, {}, "HumanBoss_ShadowClear"));
    }
    // AI開始距離とWarp上限は別々に保持し、最終候補は実際の補正可能量で絞る。
    for (auto& definition_ : actions)
    {
        if (definition_.type == HumanBossActionType::NORMAL_ATTACK_A || definition_.type == HumanBossActionType::NORMAL_ATTACK_B) definition_.maxDistance = 5.0f;
        if (definition_.type == HumanBossActionType::HEAVY_SINGLE_ATTACK) definition_.maxDistance = 6.0f;
        if (definition_.type == HumanBossActionType::COUNTER_ATTACK || definition_.type == HumanBossActionType::LIGHT_COUNTER) definition_.maxDistance = 3.5f;
        if (definition_.type == HumanBossActionType::WHIFF_PUNISH || definition_.type == HumanBossActionType::DELAY_ATTACK) definition_.maxDistance = 4.5f;
        for (auto& phase_ : definition_.skill.phases)
            phase_.motionWarpPreset = GetDefaultAttackWarpPreset(phase_.animationState, definition_.type);
    }
    // 遠距離行動も既存Skillを使用し、通常の影渡りSkillとは分けて接続する。
    auto& charge_ = actions[static_cast<std::size_t>(HumanBossActionType::SHADOW_CHARGE)];
    charge_.category = HumanBossState::OFFENSE;
    charge_.enabled.fill(true);
    charge_.minDistance = 6.0f;
    charge_.maxDistance = 14.0f;
    charge_.weight = { 1.0f, 1.2f, 1.4f };
    charge_.cooldown.fill(10.0f);
    charge_.skill.category = SkillCategory::RUSH;
    charge_.skill.cleanupAction = "HumanBoss_RangedClear";
    charge_.skill.phases = {
        CreateComboPhase("HumanBossAttackTell", SkillPhaseEnd::DURATION, 1.2f, "HumanBoss_SHADOW_CHARGE_Sequence"),
        CreateComboPhase("Run_Attack_01_Seq_0", SkillPhaseEnd::ANIMATION_END, 0.0f, {}, {}, "HumanBoss_RangedClear"),
        CreateComboPhase("SS_Idle", SkillPhaseEnd::DURATION, 0.7f) };
    auto& orb_ = actions[static_cast<std::size_t>(HumanBossActionType::SHADOW_ORB)];
    orb_.category = HumanBossState::OFFENSE;
    orb_.enabled.fill(true);
    orb_.minDistance = 8.0f;
    orb_.maxDistance = 100.0f;
    orb_.weight = { 0.8f, 1.0f, 1.2f };
    orb_.cooldown.fill(12.0f);
    orb_.skill.category = SkillCategory::PROJECTILE;
    orb_.skill.phases = { CreateComboPhase("Buff_Seq", SkillPhaseEnd::ANIMATION_END, 0.0f, "HumanBoss_SHADOW_ORB_Sequence") };
    auto& ragnarok_ = actions[static_cast<std::size_t>(HumanBossActionType::SHADOW_RAGNAROK)];
    ragnarok_.category = HumanBossState::SPECIAL;
    ragnarok_.enabled.fill(true);
    ragnarok_.maxDistance = 1000.0f;
    ragnarok_.cooldown.fill(35.0f);
    ragnarok_.skill.category = SkillCategory::UTILITY;
    ragnarok_.skill.cleanupAction = "HumanBoss_RangedClear";
    // 攻撃回数は実行担当がこの予兆・打撃の組から展開し、無限ループを作らない。
    ragnarok_.skill.phases = {
        CreateComboPhase("HumanBossRagnarokAdvance", SkillPhaseEnd::ACTION_END, 0.0f),
        CreateComboPhase("Buff_Seq", SkillPhaseEnd::DURATION, 1.5f, "HumanBoss_SHADOW_RAGNAROK_Sequence"),
        CreateComboPhase("Buff_Seq", SkillPhaseEnd::DURATION, 0.2f, {}, "HumanBoss_RagnarokStrike"),
        CreateComboPhase("SS_Idle", SkillPhaseEnd::DURATION, 0.8f, {}, "HumanBoss_RangedClear") };
    // 既存の拘束・解除経路を再利用し、ガード継続には回避できる予備動作を追加する。
    auto& guardExecution_ = actions[static_cast<std::size_t>(HumanBossActionType::GUARD_EXECUTION)];
    guardExecution_.enabled.fill(true);
    guardExecution_.maxDistance = 2.8f;
    guardExecution_.cooldown.fill(8.0f);
    guardExecution_.skill.category = SkillCategory::COUNTER;
    guardExecution_.skill.cleanupAction = "HumanBoss_ExecutionRelease";
    guardExecution_.skill.phases = {
        CreateComboPhase("HumanBossAttackTell", SkillPhaseEnd::DURATION, 0.55f, {}, "HumanBoss_WindupBegin", "HumanBoss_WindupEnd"),
        CreateComboPhase("Execution_03_Seq", SkillPhaseEnd::DURATION, 1.92f, {}, "HumanBoss_ExecutionCapture") };
    cooldowns.resize(actions.size(), 0.0f);
}

const std::string& HumanBossAIComponent::GetName() const
{
    static const std::string NAME = "HumanBossAIComponent";
    return NAME;
}

void HumanBossAIComponent::Start()
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::Start");
    // 旧ボスJSONではAIのLateUpdateが無効。暴走開始のRaw時間更新とオーラ追従に
    // 必須になったため、旧保存データを読み込んだ場合もここで有効化する。
    SetLateUpdateEnabled(true);
    NormalizeSettings();
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    const auto executor_ = owner_->GetOrAddComponent<HumanBossActionComponent>(false);
    executor = executor_;
    ResetBerserk();
    executor_->InitializeAttackAssist();
    executor_->RegisterActions();
    owner_->GetOrAddComponent<BlackboardComponent>();
    // Sceneに編集済みのSequenceやCurveがあれば維持し、未設定の接続だけを補う。
    const auto sequence_ = owner_->GetOrAddComponent<ActionSequence>();
    sequence_->LoadMissingSequences("Data/ActionSequence/HumanBossAttacks.json");
    sequence_->LoadMissingSequences("Data/ActionSequence/HumanBossShadow.json");
    sequence_->LoadMissingSequences("Data/ActionSequence/HumanBossRanged.json");
    if (const auto animation_ = AnimationGraphBridgeComponent::Acquire(owner_))
    {
        constexpr const char* SHADOW_STATES[] = { "Run_Attack_01_Seq_0", "HumanBossRagnarokAdvance", "SS_Sp_Skill1", "Buff_Seq", "Attack_Air_to_Floor_01_Start_Seq",
            "Attack_Air_to_Floor_01_Loop_Seq", "Attack_Air_to_Floor_01_End_Seq", "Attack_Air_to_Floor_02_Start_Seq",
            "Attack_Air_to_Floor_02_Loop_Seq", "Attack_Air_to_Floor_02_End_Seq" };
        for (const auto* stateName_ : SHADOW_STATES)
            if (!animation_->GetStateCatalog().count(stateName_))
                animation_->LoadMissingStateAsset(stateName_, std::string("Data/Animation/StateData/HumanBoss/States/") + stateName_ + ".json");
    }
    sequence_->LoadMissingSequences("Data/ActionSequence/HumanBossJustGuardReaction.json");
    sequence_->EnsureImmediateSequence("HumanBoss_Windup", { { "HumanBoss", "HumanBoss_WindupBegin" } });
    sequence_->EnsureImmediateSequence("HumanBoss_StrikeSetup",
        { { "HumanBoss", "HumanBoss_WindupEnd" }, { "HumanBoss", "HumanBoss_FaceTarget" } });
    const auto curve_ = owner_->GetOrAddComponent<CurveComponent>();
    const char* CURVE_NAMES[] = { "HumanBoss.WindupPoseSpeed", "HumanBoss.SlashSpeedA", "HumanBoss.SlashSpeedB", "HumanBoss.FinisherSpeed", "HumanBoss.AttackRootWeight",
        "HumanBoss.Combo01Speed", "HumanBoss.Combo02Speed", "HumanBoss.HeavySingleSpeed", "HumanBoss.WarpCorrection", "HumanBoss.ShadowSlashSpeed", "HumanBoss.ChargeGrowth", "HumanBoss.ChargeTravel" };
    for (const auto* curveName_ : CURVE_NAMES)
        if (!curve_->HasTrack(curveName_)) curve_->LoadTrackAsset(std::string("Data/Curve/") + curveName_ + ".json");
    cooldowns.assign(actions.size(), 0.0f);
    EnsureDefaultBehaviorGraph();
    rangedPressureTime = 0.0f;
    runtimeStarted = true;
    RegisterActions();
    EnterNeutral();
}

void HumanBossAIComponent::RegisterActions()
{
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    const std::weak_ptr<HumanBossAIComponent> weak_ = owner_->GetComponent<HumanBossAIComponent>();
    // 演出への受け渡しと戦闘復帰で旧Skillを破棄し、既存の待機状態から判断を再開する。
    owner_->RegisterGlobalAction("ActorPresentation", "HumanPresentation_Stop", [weak_]()
    {
        BS_PROFILE_SCOPE("HumanBossAIComponent::PresentationStop");
        if (const auto self_ = weak_.lock()) self_->Stop();
    });
    owner_->RegisterAction("HumanBoss", "HumanBoss_OnAttackJustGuarded", [weak_]()
    {
        if (const auto self_ = weak_.lock())
        {
            if (self_->IsBerserkActive()) self_->OnBerserkJustGuarded();
            else self_->BeginJustGuardRecoil();
        }
    });
    owner_->RegisterAction("HumanBoss", "HumanBoss_JustGuardRecoilEnd", [weak_]()
    {
        const auto self_ = weak_.lock();
        const auto actor_ = self_ ? self_->GetGameObject() : nullptr;
        // 中断済みのSequenceや通常攻撃の通知は、この硬直の終了として扱わない。
        if (self_ && self_->justGuardStunned && actor_) actor_->ExecuteAction("ActionEnd");
    });
    // 予備動作では滑らかに追従し、各打撃の開始時だけPlayerへ向き直す。
    for (const bool immediate_ : { false, true })
    {
        const std::string actionName_ = immediate_ ? "HumanBoss_FaceTarget" : "HumanBoss_WindupFaceTarget";
        owner_->RegisterAction("HumanBoss", actionName_, [weak_, immediate_]()
        {
            const auto self_ = weak_.lock();
            const auto actor_ = self_ ? self_->GetGameObject() : nullptr;
            const auto target_ = self_ ? self_->target.lock() : nullptr;
            const auto executor_ = self_ ? self_->executor.lock() : nullptr;
            if (!actor_ || !target_ || !executor_ || !actor_->transform || !target_->transform) return;
            const auto actorPosition_ = actor_->transform->GetWorldPosition();
            const auto targetPosition_ = target_->transform->GetWorldPosition();
            executor_->FaceTarget({ targetPosition_.x - actorPosition_.x, 0.0f, targetPosition_.z - actorPosition_.z }, immediate_);
        });
    }
    // HitReactionのダメージ処理を維持し、ひるみ中はNeutralがAnimationを上書きしない。
    owner_->RegisterAction("HumanBoss", "HumanBossHit", [weak_]()
    {
        BS_PROFILE_SCOPE("HumanBossAIComponent::OnHit");
        const auto self_ = weak_.lock();
        const auto actor_ = self_ ? self_->GetGameObject() : nullptr;
        if (!actor_) return;
        if (self_->IsBerserkActive()) return;
        bool reacted_ = true;
        if (const auto blackboard_ = actor_->GetComponent<BlackboardComponent>())
            blackboard_->GetBool("Hit", "ImpactReacted", reacted_);
        if (!reacted_) return;
        self_->Stop();
        ++self_->bossHitStreak;
        self_->bossHitChainTimer = self_->mindSettings.hitEscapeChainWindow;
        self_->aggression = (std::max)(0.0f, self_->aggression - 0.08f);
        self_->confidence = (std::max)(0.0f, self_->confidence - 0.05f);
        // 連打のたびに長い復帰待ちを重ねず、短いひるみ後に離脱を判断できるようにする。
        self_->hitTimer = self_->bossHitStreak >= 2 ? (std::min)(self_->hitRecoveryDuration, self_->mindSettings.hitEscapeRecovery) : self_->hitRecoveryDuration;
        self_->repositionTimer = self_->mindSettings.postHitRepositionTime;
        self_->intentLockTimer = 0.0f;
        const bool airHeavyHit_ = self_->airHeavyPressureTimer > 0.0f && self_->airHeavyAttackSerial == self_->playerObservation.serial;
        if (airHeavyHit_)
        {
            // 同じ空中攻撃からの離脱には観察待ちを重ねず、復帰後の追跡へ判断を戻す。
            self_->airHeavyPressureTimer = self_->mindSettings.airHeavyPressureWindow;
            self_->waitTimer = 0.0f;
            self_->repositionTimer = 0.0f;
            self_->intent = HumanBossIntent::CHASE;
            if (self_->airHeavyAttackChain >= 2) self_->hitTimer = (std::min)(self_->hitRecoveryDuration, self_->mindSettings.hitEscapeRecovery);
        }
        if (self_->bossHitStreak >= 2 && !airHeavyHit_)
        {
            // 連続被弾後は再観察で立ち止まらず、短い復帰耐性の間に距離を取り直す。
            self_->waitTimer = 0.0f;
            self_->intent = HumanBossIntent::REPOSITION;
            self_->intentLockTimer = self_->mindSettings.hitEscapeProtection;
        }
        if (const auto executor_ = self_->executor.lock())
        {
            if (self_->bossHitStreak >= 2 || (airHeavyHit_ && self_->airHeavyAttackChain >= 2))
            {
                // 地上の強攻撃にも復帰耐性を適用し、ダメージを受けながら連続ひるみだけを防ぐ。
                const float armor_ = airHeavyHit_ ? (std::max)(self_->mindSettings.hitEscapeArmor, self_->mindSettings.airHeavyRecoveryArmor) : self_->mindSettings.hitEscapeArmor;
                executor_->ProtectRecovery(self_->mindSettings.hitEscapeProtection, armor_);
            }
            executor_->SetLocomotion(self_->hitState, { 0.0f, 0.0f, 0.0f }, self_->toPlayer, { 0.0f, 0.0f });
        }
    });
    for (const std::string& actionName_ : { std::string("HumanBoss_Approach"), std::string("HumanBoss_Reposition"), std::string("HumanBoss_Observe"), std::string("HumanBoss_NoReaction") })
    {
        owner_->RegisterAction("HumanBoss", actionName_, [weak_, actionName_]()
        {
            if (const auto self_ = weak_.lock()) self_->UpdateLocomotion(actionName_);
        });
    }
    for (int index_ = 0; index_ < static_cast<int>(HumanBossActionType::COUNT); ++index_)
    {
        owner_->RegisterAction("HumanBoss", std::string("HumanBoss_") + ACTION_NAMES[index_], [weak_, index_]()
        {
            if (const auto self_ = weak_.lock()) self_->RequestAction(static_cast<HumanBossActionType>(index_));
        });
    }
}

void HumanBossAIComponent::EnterNeutral()
{
    state = HumanBossState::NEUTRAL;
    currentAction = -1;
    counterPending = false;
    // Phaseごとのテンポを維持しつつ、短い抽選結果にも観察時間の下限を設ける。
    waitTimer = (std::max)(mindSettings.minimumObservationTime,
        std::uniform_real_distribution<float>(waitMin, waitMax)(random) * mindSettings.phaseObserve[phase]);
    strafeDirection = std::uniform_int_distribution<int>(0, 1)(random) == 0 ? -1.0f : 1.0f;
}

void HumanBossAIComponent::Stop()
{
    if (stoppingAction) return;
    stoppingAction = true;
    justGuardStunned = false;
    if (const auto executor_ = executor.lock())
    {
        // 初回の予兆前に被弾などで中断された場合、35秒待たず準備行動を再試行できるようにする。
        const auto specialIndex_ = static_cast<std::size_t>(HumanBossActionType::SHADOW_RAGNAROK);
        if (currentAction == static_cast<int>(specialIndex_) && !executor_->WasRagnarokUsed() && specialIndex_ < cooldowns.size())
            cooldowns[specialIndex_] = (std::min)(cooldowns[specialIndex_], 2.0f);
        executor_->Stop(idleState);
    }
    behaviorGraph.ResetRuntime();
    if (const auto owner_ = GetGameObject())
        if (const auto blackboard_ = owner_->GetComponent<BlackboardComponent>())
        {
            blackboard_->SetBool("HumanBossAI", "ActionEnded", true);
            blackboard_->SetBool("HumanBossAI", "JustGuardStunned", false);
        }
    repositionTimer = 0.0f;
    lightCounterPending = false;
    comboDelayPending = false;
    EnterNeutral();
    stoppingAction = false;
}

void HumanBossAIComponent::BeginJustGuardRecoil()
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::BeginJustGuardRecoil");
    const auto owner_ = GetGameObject();
    const auto executor_ = executor.lock();
    if (!runtimeStarted || stoppingAction || justGuardStunned || !owner_ || !owner_->manager || !executor_ ||
        !executor_->IsActive() || currentAction < 0 || hitTimer > 0.0f) return;
    const auto& attack_ = actions[currentAction].skill;
    if (attack_.category != SkillCategory::ATTACK && attack_.category != SkillCategory::COUNTER) return;
    if (const auto health_ = owner_->GetComponent<HealthComponent>()) if (health_->IsDead()) return;
    bool locked_ = false;
    if (const auto mode_ = owner_->GetManager().GetGameModeManager())
        mode_->GetGlobalBlackboard().GetBool(GameModeBlackboardKeys::CATEGORY_GAME, GameModeBlackboardKeys::ENEMY_AI_LOCKED, locked_);
    if (locked_) return;
    // Skillの予約、BehaviorTreeの実行位置、武器と派生判定を閉じてから硬直を開始する。
    Stop();
    hitTimer = 0.0f;
    justGuardStunned = true;
    counterPending = false;
    lightCounterPending = false;
    intent = HumanBossIntent::OBSERVE;
    intentLockTimer = 0.0f;
    if (!executor_->Begin(justGuardReactionSkill, HumanBossDefense::NONE, { 0.0f, 0.0f, 0.0f }) || !executor_->IsActive())
    {
        // 編集で素材が失われても旧コンボは再開せず、既存の復帰待ちで停止を解く。
        justGuardStunned = false;
        hitTimer = (std::max)(0.05f, hitRecoveryDuration);
        graphStatus = "JustGuard recoil unavailable: " + executor_->GetValidationError();
    }
    PublishDebug();
}

bool HumanBossAIComponent::ObservePlayer()
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::ObservePlayer");
    playerAttackDetected = false;
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->manager || !owner_->transform) return false;
    const auto observation_ = owner_->GetManager().GetCombatObservationSystem();
    if (!observation_) { reactionReason = "Observation system unavailable"; return false; }
    auto player_ = target.lock();
    if (!player_ || player_->GetName() != playerObjectName || !observation_->IsRegisteredObject(player_))
    {
        player_ = observation_->FindObject(playerObjectName);
        target = player_;
    }
    if (!player_ || player_ == owner_ || !player_->transform) { reactionReason = "Target missing"; return false; }
    if (const auto playerHealth_ = player_->GetComponent<HealthComponent>())
        if (playerHealth_->IsDead()) { reactionReason = "Player dead"; return false; }
    const auto ownerPosition_ = owner_->transform->GetWorldPosition();
    if (!observation_->GetPlayerActionObservation(player_, playerObservation))
    { reactionReason = "Registered player observation pending"; return false; }
    const auto playerPosition_ = playerObservation.position;
    toPlayer = { playerPosition_.x - ownerPosition_.x, 0.0f, playerPosition_.z - ownerPosition_.z };
    distance = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
    if (distance > 0.0001f) { toPlayer.x /= distance; toPlayer.z /= distance; }
    else toPlayer = { 0.0f, 0.0f, 1.0f };
    if (!playerObservation.alive) { reactionReason = "Player dead"; return false; }
    recentPlayerAction = playerObservation.actionName;
    sameActionRepeatCount = playerObservation.sameActionCount;
    guardFrequency = playerObservation.guardFrequency;
    dodgeFrequency = playerObservation.dodgeFrequency;
    attackSerial = static_cast<unsigned int>(playerObservation.serial);
    float reach_ = playerAttackReach;
    float halfAngle_ = playerAttackHalfAngle;
    for (const auto& threat_ : threats)
        if (threat_.animationState == playerObservation.animationState) { reach_ = threat_.reach; halfAngle_ = threat_.halfAngle; break; }
    const float facingDot_ = -(playerObservation.forward.x * toPlayer.x + playerObservation.forward.z * toPlayer.z);
    const float heightTolerance_ = playerObservation.airborne && playerObservation.heavyAttack
        ? (std::max)(threatHeightTolerance, mindSettings.airHeavyThreatHeightTolerance) : threatHeightTolerance;
    if (distance > reach_ || std::abs(ownerPosition_.y - playerObservation.position.y) > heightTolerance_ ||
        facingDot_ < std::cos(halfAngle_ * DirectX::XM_PI / 180.0f))
    { reactionReason = "Attack cannot reach boss"; return true; }
    incomingTime = playerObservation.timeUntilHit;
    // 踏み込みで遅れて射程へ入った強攻撃も、未命中の有効時間なら受け流しの対象に残す。
    playerAttackDetected = (playerObservation.attackPhase == ObservedAttackPhase::STARTUP && incomingTime <= reactionLookAhead) ||
        (playerObservation.heavyAttack && playerObservation.attackPhase == ObservedAttackPhase::ACTIVE && !playerObservation.attackMadeContact);
    reactionReason = playerAttackDetected ? "Reachable attack window" : "Outside attack window";
    return true;
}

bool HumanBossAIComponent::IsAvailable(std::size_t index_, std::string* reason_) const
{
    // 既存の判定結果を変えず、追跡が要求された場合だけ最初の除外理由を残す。
    const auto reject_ = [reason_](const char* message_)
    { if (reason_) *reason_ = message_; return false; };
    if (justGuardStunned || stoppingAction) return reject_("JG recoil / action stopping");
    if (index_ >= actions.size() || index_ >= cooldowns.size()) return reject_("Action definition missing");
    const auto& definition_ = actions[index_];
    const auto type_ = definition_.type;
    const auto category_ = definition_.category;

    // 成功が確定した専用反撃は、通常抽選用の距離・Cooldown・連続使用制限で落とさない。
    const bool confirmedCounter_ = type_ == HumanBossActionType::COUNTER_ATTACK && counterPending;
    if (!definition_.enabled[phase]) return reject_("Disabled in current AI phase");
    if (!confirmedCounter_)
    {
        if (cooldowns[index_] > 0.0f) return reject_("Action cooldown remaining");
        if (distance < definition_.minDistance || distance > definition_.maxDistance) return reject_("Outside configured action distance");
        if (category_ != HumanBossState::REACTION && category_ != HumanBossState::SHADOW &&
            static_cast<int>(index_) == lastAction) return reject_("Same action repetition blocked");
    }

    // 履歴と共通待機時間による制限は、行動ごとの再使用時間とは別に適用する。
    if (type_ >= HumanBossActionType::SHADOW_SLASH && static_cast<int>(index_) == lastAction) return reject_("Same action repetition blocked");
    const bool normalOffense_ = category_ == HumanBossState::OFFENSE && !IsCounterAttack(type_);
    if (normalOffense_ && static_cast<int>(index_) == lastOffense) return reject_("Same offense repetition blocked");
    if (category_ == HumanBossState::REACTION && reactionBudget < GetReactionCost(type_)) return reject_("Insufficient reaction budget");
    if (IsDodge(type_) && dodgeTimer > 0.0f) return reject_("Shared dodge cooldown remaining");
    if (normalOffense_ && attackTimer > 0.0f) return reject_("Shared attack cooldown remaining");

    // 影渡りの派生は、対応済みの行動範囲だけを実行担当へ渡す。
    if (category_ == HumanBossState::SHADOW)
    {
        const bool basicShift_ = type_ >= HumanBossActionType::SHADOW_SHIFT_BACK &&
            type_ <= HumanBossActionType::SHADOW_SHIFT_APPROACH;
        const bool shadowAttack_ = type_ >= HumanBossActionType::SHADOW_SLASH &&
            type_ <= HumanBossActionType::SHADOW_DOMAIN;
        if (!shadowEnabled || (!basicShift_ && !shadowAttack_)) return reject_("Shadow category disabled / unsupported action");
    }
    if (category_ == HumanBossState::DRAGON_SHADOW && !dragonShadowEnabled) return reject_("Dragon shadow disabled");
    if (category_ == HumanBossState::SPECIAL && type_ != HumanBossActionType::SHADOW_RAGNAROK && !specialEnabled) return reject_("Special category disabled");

    // 設定上の条件を満たした後で、必要素材と移動経路を検証する。
    const HumanBossDefense defense_ = GetDefense(type_);
    const auto executor_ = executor.lock();
    if (!executor_) return reject_("Action executor missing");
    if (!executor_->CanStart(definition_.skill, defense_, target.lock()))
    { if (reason_) *reason_ = "Executor rejected: " + executor_->GetValidationError(); return false; }
    if (category_ == HumanBossState::SHADOW &&
        !executor_->CanShadowShift(definition_.skill, GetShadowShiftMode(definition_), target.lock(), strafeDirection))
        return reject_("Shadow destination / route unavailable");
    return true;
}

bool HumanBossAIComponent::BeginAction(std::size_t index_)
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::BeginAction");
    if (!IsAvailable(index_)) return false;
    const auto& definition_ = actions[index_];
    const auto executor_ = executor.lock();
    DirectX::XMFLOAT3 velocity_{ 0.0f, 0.0f, 0.0f };
    if (definition_.type == HumanBossActionType::BACK_DODGE) velocity_ = { -toPlayer.x * dodgeSpeed, 0.0f, -toPlayer.z * dodgeSpeed };
    if (definition_.type == HumanBossActionType::SIDE_DODGE) velocity_ = { toPlayer.z * strafeDirection * dodgeSpeed, 0.0f, -toPlayer.x * strafeDirection * dodgeSpeed };
    const HumanBossDefense defense_ = GetDefense(definition_.type);
    auto executionSkill_ = definition_.skill;
    if (definition_.type == HumanBossActionType::COUNTER_ATTACK && counterPending && mindSettings.justGuardCounterDelay > 0.0f)
    {
        // 保存済みの反撃定義は変えず、プレイヤーのひるみを見せる段階を実行時だけ挟む。
        auto recoilPhase_ = CreateComboPhase("Parry_L_Seq", SkillPhaseEnd::DURATION,
            mindSettings.justGuardCounterDelay, {}, "HumanBoss_ExecutionRecoil");
        recoilPhase_.preventFlinch = true;
        executionSkill_.phases.insert(executionSkill_.phases.begin(), recoilPhase_);
    }
    const bool usesAttackWarp_ = std::any_of(definition_.skill.phases.begin(), definition_.skill.phases.end(),
        [](const SkillPhase& phase_) { return !phase_.motionWarpPreset.empty(); });
    if (!usesAttackWarp_) executor_->FaceTarget(toPlayer, definition_.skill.category != SkillCategory::ATTACK && definition_.skill.category != SkillCategory::COUNTER);
    const bool started_ = definition_.category == HumanBossState::SHADOW
        ? executor_->BeginShadowShift(definition_.skill,
            GetShadowShiftMode(definition_), target.lock(), strafeDirection)
        : executor_->Begin(executionSkill_, defense_, velocity_, target.lock());
    if (!started_)
    {
        if (const auto owner_ = GetGameObject())
            if (const auto trace_ = owner_->GetComponent<HumanBossAITraceComponent>())
            {
                const std::string event_ = std::string("Action start rejected: ") + ActionName(static_cast<int>(index_));
                trace_->Capture(*this, event_.c_str(), true);
            }
        return false;
    }
    if (const auto owner_ = GetGameObject())
        if (const auto blackboard_ = owner_->GetComponent<BlackboardComponent>()) blackboard_->SetBool("HumanBossAI", "ActionEnded", false);
    if (definition_.category == HumanBossState::REACTION) reactedSerial = attackSerial;
    counterPending = false;
    if (definition_.category == HumanBossState::REACTION)
    {
        if (!debugFreezeBudget) reactionBudget = (std::max)(0.0f, reactionBudget - GetReactionCost(definition_.type));
        reactionStreak = lastReaction == static_cast<int>(index_) ? reactionStreak + 1 : 1;
    }
    if (definition_.type == HumanBossActionType::ATTACK_FEINT) feintTimer = mindSettings.feintCooldown;
    if ((definition_.type == HumanBossActionType::NORMAL_ATTACK_A || definition_.type == HumanBossActionType::NORMAL_ATTACK_B) && playerObservation.guarding) ++guardProbeCount;
    lightCounterPending = false;
    if (definition_.type == HumanBossActionType::COUNTER_ATTACK || definition_.type == HumanBossActionType::LIGHT_COUNTER) whiffTimer = 0.0f;
    comboDelayPending = false;
    dodgePunishRequests = 0;
    currentAction = static_cast<int>(index_);
    lastAction = currentAction;
    lastActionCategory = definition_.category;
    state = definition_.category;
    cooldowns[index_] = definition_.cooldown[phase];
    // 種類を変えて影渡りを連発せず、攻撃や通常移動へ戻る時間を残す。
    if (state == HumanBossState::SHADOW)
        for (std::size_t shadowIndex_ = 0; shadowIndex_ < actions.size(); ++shadowIndex_)
            if (actions[shadowIndex_].category == HumanBossState::SHADOW)
                cooldowns[shadowIndex_] = (std::max)(cooldowns[shadowIndex_], 2.0f);
    if (state == HumanBossState::REACTION) { reactionTimer = reactionCooldown * phaseReactionMultiplier[phase]; lastReaction = currentAction; }
    if (IsDodge(definition_.type)) dodgeTimer = dodgeCooldown;
    if (state == HumanBossState::OFFENSE && definition_.type != HumanBossActionType::COUNTER_ATTACK && definition_.type != HumanBossActionType::LIGHT_COUNTER)
    { attackTimer = attackCooldown; lastOffense = currentAction; reactionStreak = 0; }
    intentReason += std::string(" -> ") + ActionName(currentAction) + " / cooldown=" + std::to_string(cooldowns[index_]) + " budgetCost=" + std::to_string(GetReactionCost(definition_.type));
    if (const auto owner_ = GetGameObject())
        if (const auto trace_ = owner_->GetComponent<HumanBossAITraceComponent>()) trace_->Capture(*this, "Action started", true);
    return true;
}

bool HumanBossAIComponent::RequestAction(HumanBossActionType type_)
{
    if (justGuardStunned || stoppingAction) return false;
    const auto executor_ = executor.lock();
    // 外部要求も実行中Actionを上書きせず、特殊演出の終了後に通常AIへ戻す。
    if ((type_ == HumanBossActionType::COUNTER_ATTACK && !counterPending) || (type_ == HumanBossActionType::LIGHT_COUNTER && !lightCounterPending) || hitTimer > 0.0f || !runtimeStarted || !executor_ || executor_->IsActive() || currentAction >= 0 || !ObservePlayer()) return false;
    const auto owner_ = GetGameObject();
    if (const auto health_ = owner_->GetComponent<HealthComponent>()) if (health_->IsDead()) return false;
    bool locked_ = false;
    if (const auto mode_ = owner_->GetManager().GetGameModeManager())
        mode_->GetGlobalBlackboard().GetBool(GameModeBlackboardKeys::CATEGORY_GAME, GameModeBlackboardKeys::ENEMY_AI_LOCKED, locked_);
    if (locked_) return false;
    for (std::size_t index_ = 0; index_ < actions.size(); ++index_)
        if (actions[index_].type == type_) return BeginAction(index_);
    return false;
}

void HumanBossAIComponent::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::Update");
    const auto owner_ = GetGameObject();
    const auto executor_ = executor.lock();
    if (!owner_) return;
    if (!runtimeStarted || !owner_->IsCombatReady() || !executor_)
    {
        // 戦闘引渡し前の停止も可視化し、AIロックと生成待ちを区別できるようにする。
        suspensionReason = !runtimeStarted ? "AI not started" : !owner_->IsCombatReady() ? "Actor not committed to combat" : "Action executor missing";
        PublishDebug();
        return;
    }
    const float delta_ = (std::max)(deltaTime_, 0.0f);
    const auto health_ = owner_->GetComponent<HealthComponent>();
    bool locked_ = false;
    if (owner_->manager)
    {
        if (const auto mode_ = owner_->GetManager().GetGameModeManager())
        {
            mode_->GetGlobalBlackboard().GetBool(GameModeBlackboardKeys::CATEGORY_GAME, GameModeBlackboardKeys::ENEMY_AI_LOCKED, locked_);
            // ドラゴンの部位管理を使わない人型ボスのHPも、既存HUDへ同期する。
            if (health_ && health_->GetMaxHealth() > 0.0f)
                mode_->SetBossHpRate(health_->GetCurrentHealth() / health_->GetMaxHealth());
        }
    }
    // HP閾値の予約は通常の行動判断より先に処理する。以前はJG硬直・観察失敗・
    // 実行中Skillの終了待ちに遮られ、予約があっても開始演出へ到達しない経路があった。
    if (!locked_ && health_ && !health_->IsDead() && TryStartBerserk())
    {
        suspensionReason.clear();
        PublishDebug();
        return;
    }
    if (justGuardStunned && !locked_ && (!health_ || !health_->IsDead()))
    {
        // 硬直中は観察失敗やツリーの再評価で反応を上書きせず、既存Skillだけを進める。
        for (auto& cooldown_ : cooldowns) cooldown_ = (std::max)(0.0f, cooldown_ - delta_);
        executor_->Tick(delta_);
        if (!executor_->IsActive())
        {
            justGuardStunned = false;
            behaviorGraph.ResetRuntime();
            EnterNeutral();
            intent = HumanBossIntent::OBSERVE;
        }
        PublishDebug();
        return;
    }
    std::string suspensionReason_;
    if (locked_) suspensionReason_ = "Game mode AI lock";
    else if (health_ && health_->IsDead()) suspensionReason_ = "Boss dead";
    else if (!ObservePlayer()) suspensionReason_ = reactionReason.empty() ? "Target observation unavailable" : reactionReason;
    if (!suspensionReason_.empty())
    {
        if (locked_ || (health_ && health_->IsDead())) ResetBerserk();
        // 停止中に毎フレームNeutralを初期化せず、残ったひるみ待ちも解除する。
        if (suspensionReason.empty()) Stop();
        suspensionReason = suspensionReason_;
        hitTimer = 0.0f;
        bossHitChainTimer = 0.0f;
        bossHitStreak = 0;
        airHeavyPressureTimer = 0.0f;
        airHeavyMemoryTimer = 0.0f;
        airHeavyAttackChain = 0;
        airHeavyAttackSerial = 0;
        PublishDebug();
        return;
    }
    suspensionReason.clear();
    if (UpdateBerserk(delta_)) { PublishDebug(); return; }
    if (health_ && health_->GetMaxHealth() > 0.0f && !debugForcePhase)
    {
        const float rate_ = health_->GetCurrentHealth() / health_->GetMaxHealth();
        phase = rate_ <= phase3Hp ? 2 : rate_ <= phase2Hp ? 1 : 0;
    }
    if (debugForcePhase) phase = std::clamp(debugPhase, 0, 2);
    for (auto& cooldown_ : cooldowns) cooldown_ = (std::max)(0.0f, cooldown_ - delta_);
    reactionTimer = (std::max)(0.0f, reactionTimer - delta_);
    dodgeTimer = (std::max)(0.0f, dodgeTimer - delta_);
    attackTimer = (std::max)(0.0f, attackTimer - delta_);
    hitTimer = (std::max)(0.0f, hitTimer - delta_);
    executor_->UpdateHitProtection(delta_);
    if (executor_->ConsumeJustGuardSuccess())
    {
        // 専用反撃の予備段階でひるみを見せ、通常の行動抽選を挟まず一度だけ実行する。
        Stop();
        counterPending = true;
        counterWindowTimer = mindSettings.whiffWindow;
        intent = HumanBossIntent::COUNTER_ATTACK;
        intentLockTimer = 0.0f;
        waitTimer = 0.0f;
        if (!BeginAction(static_cast<std::size_t>(HumanBossActionType::COUNTER_ATTACK)))
        {
            counterPending = false;
            intentReason = "Execution counter unavailable: " + executor_->GetValidationError();
        }
        PublishDebug();
        return;
    }
    rangedPressureTime = distance >= 8.0f ? (std::min)(5.0f, rangedPressureTime + delta_) : (std::max)(0.0f, rangedPressureTime - delta_);
    UpdateMind(delta_);
    if (hitTimer > 0.0f) { PublishDebug(); return; }
    executor_->Tick(delta_);
    if (comboDelayPending && dodgePunishRequests == 0 && executor_->ExtendComboDelay(mindSettings.dodgePunishDelay, mindSettings.maximumComboDelay))
    { comboDelayPending = false; ++dodgePunishRequests; intentReason = "Dodge observed: extend the next combo gap"; }
    if (currentAction >= 0)
    {
        if (executor_->IsActive())
        {
            TickBehaviorGraph(delta_);
            PublishDebug();
            return;
        }
        const bool completed_ = executor_->GetResult() == SkillExecutionResult::COMPLETED;
        if (!completed_ && actions[currentAction].type == HumanBossActionType::SHADOW_RAGNAROK && !executor_->WasRagnarokUsed())
            cooldowns[currentAction] = (std::min)(cooldowns[currentAction], 2.0f);
        const bool followUpRejected_ = executor_->GetResult() == SkillExecutionResult::FOLLOW_UP_REJECTED;
        const bool counter_ = completed_ && counterPending;
        const bool light_ = completed_ && (executor_->ConsumeGuardSuccess() || (IsDodge(actions[currentAction].type) && whiffTimer > 0.0f)) &&
            std::uniform_real_distribution<float>(0.0f, 1.0f)(random) < mindSettings.lightCounterChance;
        const bool reposition_ = completed_ && state == HumanBossState::OFFENSE;
        const bool approachCompleted_ = completed_ && actions[currentAction].type == HumanBossActionType::SHADOW_SHIFT_APPROACH;
        const bool feintDodged_ = completed_ && actions[currentAction].type == HumanBossActionType::ATTACK_FEINT && (playerObservation.dodging || comboDelayPending || dodgePunishRequests > 0);
        // 開始したノードへ終了を返してから、次の優先判断へ戻す。
        TickBehaviorGraph(delta_, true);
        EnterNeutral();
        counterPending = counter_;
        lightCounterPending = light_;
        counterWindowTimer = counter_ || light_ ? mindSettings.whiffWindow : 0.0f;
        intentLockTimer = 0.0f;
        if (reposition_) repositionTimer = repositionTime;
        // 攻撃を伴わない接近だけは即時に再評価し、影の派生攻撃後は観察へ戻す。
        if (approachCompleted_) { waitTimer = 0.0f; repositionTimer = 0.0f; }
        if (followUpRejected_)
        {
            // 不成立の次段へ張り付かず、既存Cooldownを残して接近・向き直しへ戻す。
            waitTimer = 0.0f;
            repositionTimer = 0.0f;
            intent = HumanBossIntent::CHASE;
            intentLockTimer = mindSettings.decisionLockTime;
            intentReason = "Combo ended: next attack cannot reach target";
        }
        if (feintDodged_)
        { intent = HumanBossIntent::PUNISH_DODGE; waitTimer = 0.0f; repositionTimer = 0.0f; intentLockTimer = mindSettings.decisionLockTime; intentReason = "Feint induced dodge: delay punish candidate"; }
        PublishDebug();
        return;
    }
    // 接近中に待機を使い切らず、近接間合いへ入ってから相手の出方を見る。
    if (distance <= preferredCombatDistance + 0.25f)
        waitTimer = (std::max)(0.0f, waitTimer - delta_);
    repositionTimer = (std::max)(0.0f, repositionTimer - delta_);
    // 判断はツリーだけに任せ、攻撃開始に失敗しても観察時間を延長し続けない。
    TickBehaviorGraph(delta_);
    PublishDebug();
}

void HumanBossAIComponent::PublishDebug()
{
    const auto owner_ = GetGameObject();
    const auto blackboard_ = owner_ ? owner_->GetComponent<BlackboardComponent>() : nullptr;
    if (!blackboard_) return;
    blackboard_->SetBool("HumanBossAI", "JustGuardStunned", justGuardStunned);
    blackboard_->SetInt("HumanBossAI", "BerserkStage", berserkStage);
    blackboard_->SetString("HumanBossAI", "SuspensionReason", suspensionReason);
    blackboard_->SetString("HumanBossAI", "Intent", GetHumanBossIntentName(intent));
    blackboard_->SetFloat("HumanBossAI", "ReactionBudget", reactionBudget);
    blackboard_->SetFloat("HumanBossAI", "AttackFrequency", playerObservation.attackFrequency);
    blackboard_->SetFloat("HumanBossAI", "WeakAttackRate", playerObservation.weakAttackRate);
    blackboard_->SetFloat("HumanBossAI", "HeavyAttackRate", playerObservation.heavyAttackRate);
    blackboard_->SetFloat("HumanBossAI", "ApproachTendency", approachTendency);
    blackboard_->SetFloat("HumanBossAI", "RetreatTendency", retreatTendency);
    blackboard_->SetFloat("HumanBossAI", "RangedPressureTime", rangedPressureTime);
    blackboard_->SetBool("HumanBossAI", "SpecialAttackUsed", executor.lock() && executor.lock()->WasRagnarokUsed());
    blackboard_->SetInt("HumanBossAI", "DodgeDirection", static_cast<int>(playerObservation.dodgeDirection));
    blackboard_->SetInt("HumanBossAI", "PlayerAttackPhase", static_cast<int>(playerObservation.attackPhase));
    blackboard_->SetFloat("HumanBossAI", "ReactionDelay", reactionDelayRemaining);
    blackboard_->SetFloat("HumanBossAI", "AirHeavyPressureRemaining", airHeavyPressureTimer);
    blackboard_->SetInt("HumanBossAI", "AirHeavyAttackChain", airHeavyAttackChain);
    blackboard_->SetString("HumanBossAI", "DecisionReason", intentReason);
    blackboard_->SetInt("HumanBossAI", "Phase", phase + 1);
    blackboard_->SetString("HumanBossAI", "State", STATE_NAMES[static_cast<int>(state)]);
    blackboard_->SetString("HumanBossAI", "Action", ActionName(currentAction));
    blackboard_->SetFloat("HumanBossAI", "Distance", distance);
    blackboard_->SetBool("HumanBossAI", "PlayerAttackDetected", playerAttackDetected);
    blackboard_->SetString("HumanBossAI", "RecentPlayerAction", recentPlayerAction);
    blackboard_->SetInt("HumanBossAI", "SameActionRepeatCount", sameActionRepeatCount);
    blackboard_->SetFloat("HumanBossAI", "GuardFrequency", guardFrequency);
    blackboard_->SetFloat("HumanBossAI", "DodgeFrequency", dodgeFrequency);
    if (const auto trace_ = owner_->GetComponent<HumanBossAITraceComponent>()) trace_->Capture(*this, "State change");
}

void HumanBossAIComponent::NormalizeSettings()
{
    walkSpeed = (std::max)(0.0f, walkSpeed);
    strafeSpeed = (std::max)(0.0f, strafeSpeed);
    dodgeSpeed = (std::max)(0.0f, dodgeSpeed);
    reactionCooldown = (std::max)(0.0f, reactionCooldown);
    dodgeCooldown = (std::max)(0.0f, dodgeCooldown);
    attackCooldown = (std::max)(0.0f, attackCooldown);
    reactionLookAhead = (std::max)(0.0f, reactionLookAhead);
    justGuardLead = (std::max)(0.0f, justGuardLead);
    playerAttackReach = (std::max)(0.0f, playerAttackReach);
    threatHeightTolerance = (std::max)(0.0f, threatHeightTolerance);
    noneReactionWeight = (std::max)(0.01f, noneReactionWeight);
    repositionTime = (std::max)(0.0f, repositionTime);
    hitRecoveryDuration = (std::max)(0.05f, hitRecoveryDuration);
    for (auto& threat_ : threats)
    {
        threat_.reach = (std::max)(0.0f, threat_.reach);
        threat_.halfAngle = std::clamp(threat_.halfAngle, 0.0f, 180.0f);
    }
    minCombatDistance = (std::max)(0.0f, minCombatDistance);
    preferredCombatDistance = (std::max)(minCombatDistance, preferredCombatDistance);
    maxCombatDistance = (std::max)(preferredCombatDistance, maxCombatDistance);
    waitMin = (std::max)(0.05f, waitMin);
    waitMax = (std::max)(waitMin, waitMax);
    phase2Hp = std::clamp(phase2Hp, 0.0f, 1.0f);
    phase3Hp = std::clamp(phase3Hp, 0.0f, phase2Hp);
    playerAttackHalfAngle = std::clamp(playerAttackHalfAngle, 0.0f, 180.0f);
    for (auto& multiplier_ : phaseReactionMultiplier) multiplier_ = (std::max)(0.05f, multiplier_);
    for (auto& definition_ : actions)
    {
        definition_.minDistance = (std::max)(0.0f, definition_.minDistance);
        definition_.maxDistance = (std::max)(definition_.minDistance, definition_.maxDistance);
        for (auto& weight_ : definition_.weight) weight_ = (std::max)(0.0f, weight_);
        for (auto& cooldown_ : definition_.cooldown) cooldown_ = (std::max)(0.0f, cooldown_);
    }
}
void HumanBossAIComponent::Gui()
{
    DrawBerserkGui();
    BS_PROFILE_SCOPE("HumanBossAIComponent::Gui");
    ImGui::Text("Phase %d / %s", phase + 1, STATE_NAMES[static_cast<int>(state)]);
    ImGui::Text("JustGuard recoil: %s", justGuardStunned ? "ACTIVE" : "OFF");
    if (ImGui::TreeNode("JustGuard Recoil Skill"))
    {
        ImGui::BeginDisabled(justGuardStunned);
        for (auto& phase_ : justGuardReactionSkill.phases)
        {
            ImGui::InputText("Recoil Animation", &phase_.animationState);
            ImGui::InputText("Recoil Sequence", &phase_.sequence);
            ImGui::DragFloat("Recoil Safety Timeout", &phase_.maximumDuration, 0.05f, 0.05f, 120.0f);
        }
        ImGui::EndDisabled();
        ImGui::TreePop();
    }
    ImGui::Text("AI status: %s", suspensionReason.empty() ? (runtimeStarted ? "Active" : "Not started") : suspensionReason.c_str());
    ImGui::Text("Component suspended: %s", IsRuntimeSuppressed() ? "yes" : "no");
    const auto combatOwner_ = GetGameObject();
    ImGui::Text("Combat ready: %s", combatOwner_ && combatOwner_->IsCombatReady() ? "yes" : "no");
    // 観察開始前も名前から確認し、実際のHP死亡と生成・観察待ちを区別する。
    auto observedPlayer_ = target.lock();
    if (!observedPlayer_ && combatOwner_ && combatOwner_->manager)
        observedPlayer_ = combatOwner_->GetManager().Find(playerObjectName);
    if (const auto health_ = observedPlayer_ ? observedPlayer_->GetComponent<HealthComponent>() : nullptr)
        ImGui::Text("Player %s HP: %.1f / %.1f / Dead: %s", observedPlayer_->GetName().c_str(),
            health_->GetCurrentHealth(), health_->GetMaxHealth(), health_->IsDead() ? "yes" : "no");
    ImGui::Text("Action: %s / Last: %s (%s)", ActionName(currentAction), ActionName(lastAction), STATE_NAMES[static_cast<int>(lastActionCategory)]);
    ImGui::Text("Distance %.2f / Reaction %.2f / Attack %.2f / Dodge %.2f", distance, reactionTimer, attackTimer, dodgeTimer);
    float shadowCooldown_ = 0.0f;
    for (std::size_t index_ = 0; index_ < actions.size() && index_ < cooldowns.size(); ++index_)
        if (actions[index_].category == HumanBossState::SHADOW) shadowCooldown_ = (std::max)(shadowCooldown_, cooldowns[index_]);
    ImGui::Text("Shadow Cooldown: %.2f / Hit Recovery: %.2f", shadowCooldown_, hitTimer);
    ImGui::Text("Player attack: %s / %s", playerAttackDetected ? "Detected" : "None", reactionReason.c_str());
    ImGui::Text("Recent: %s x%d / Guard %.2f / Dodge %.2f", recentPlayerAction.c_str(), sameActionRepeatCount, guardFrequency, dodgeFrequency);
    ImGui::TextWrapped("Behavior Graph: %s", graphStatus.c_str());
    ImGui::Text("Observation remaining: %.2f / Reposition remaining: %.2f", waitTimer, repositionTimer);
    if (const auto executor_ = executor.lock())
    {
        const auto specialIndex_ = static_cast<std::size_t>(HumanBossActionType::SHADOW_RAGNAROK);
        const bool available_ = IsAvailable(specialIndex_);
        ImGui::Text("Ragnarok: %s / cooldown %.2f / phase enabled %s / already used %s",
            available_ ? "AVAILABLE" : "BLOCKED", cooldowns[specialIndex_],
            actions[specialIndex_].enabled[phase] ? "YES" : "NO", executor_->WasRagnarokUsed() ? "YES" : "NO");
        executor_->CanUseRagnarok();
        ImGui::TextWrapped("Ragnarok conditions: %s", executor_->GetRagnarokBlockReason().empty() ?
            "Passed (check active skill, state/sequence assets, AI lock and cooldown)" : executor_->GetRagnarokBlockReason().c_str());
    }
    if (const auto executor_ = executor.lock())
    {
        for (std::size_t index_ = 0; index_ < 2 && index_ < actions.size(); ++index_)
        {
            const bool valid_ = executor_->CanStart(actions[index_].skill, HumanBossDefense::NONE, target.lock());
            ImGui::Text("%s: Enabled %d / Range %.2f-%.2f / Cooldown %.2f", ActionName(static_cast<int>(index_)), actions[index_].enabled[phase], actions[index_].minDistance, actions[index_].maxDistance, cooldowns[index_]);
            if (!valid_) ImGui::TextWrapped("Blocked: %s", executor_->GetValidationError().c_str());
        }
    }
    ImGui::InputText("Behavior Graph Asset", &behaviorGraphAssetPath);
    if (ImGui::Button("Reload Behavior Graph")) LoadBehaviorGraphAsset(behaviorGraphAssetPath);
    DrawMindGui();
    ImGui::InputText("Player Object", &playerObjectName);
    ImGui::InputText("Idle State", &idleState);
    ImGui::InputText("Combat Walk State", &walkState);
    ImGui::InputText("Hit State", &hitState);
    ImGui::DragFloat("Hit Recovery Duration", &hitRecoveryDuration, 0.05f, 0.05f, 5.0f);
    ImGui::DragFloat("Min Combat Distance", &minCombatDistance, 0.05f, 0.0f, 100.0f);
    ImGui::DragFloat("Preferred Combat Distance", &preferredCombatDistance, 0.05f, 0.0f, 100.0f);
    ImGui::DragFloat("Max Combat Distance", &maxCombatDistance, 0.05f, 0.0f, 100.0f);
    ImGui::DragFloat("Wait Min", &waitMin, 0.05f, 0.05f, 10.0f);
    ImGui::DragFloat("Wait Max", &waitMax, 0.05f, 0.05f, 10.0f);
    ImGui::DragFloat("Walk Speed", &walkSpeed, 0.05f, 0.0f, 30.0f);
    ImGui::DragFloat("Strafe Speed", &strafeSpeed, 0.05f, 0.0f, 30.0f);
    ImGui::DragFloat("Dodge Speed", &dodgeSpeed, 0.05f, 0.0f, 30.0f);
    ImGui::DragFloat("Reaction Cooldown", &reactionCooldown, 0.05f, 0.0f, 30.0f);
    ImGui::DragFloat("Shared Dodge Cooldown", &dodgeCooldown, 0.05f, 0.0f, 30.0f);
    ImGui::DragFloat("Shared Attack Cooldown", &attackCooldown, 0.05f, 0.0f, 30.0f);
    ImGui::DragFloat("Reaction Look Ahead", &reactionLookAhead, 0.01f, 0.0f, 2.0f);
    ImGui::DragFloat("Just Guard Lead", &justGuardLead, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Default Player Attack Reach", &playerAttackReach, 0.05f, 0.0f, 100.0f);
    ImGui::DragFloat("Default Attack Half Angle", &playerAttackHalfAngle, 1.0f, 0.0f, 180.0f);
    ImGui::DragFloat("Threat Height Tolerance", &threatHeightTolerance, 0.05f, 0.0f, 10.0f);
    ImGui::DragFloat("NONE Reaction Weight", &noneReactionWeight, 1.0f, 0.0f, 1000.0f);
    ImGui::DragFloat("Reposition Time", &repositionTime, 0.05f, 0.0f, 10.0f);
    ImGui::SliderFloat("Phase 2 HP", &phase2Hp, 0.0f, 1.0f);
    ImGui::SliderFloat("Phase 3 HP", &phase3Hp, 0.0f, 1.0f);
    ImGui::DragFloat3("Phase Reaction Cooldown Multipliers", phaseReactionMultiplier.data(), 0.05f, 0.05f, 5.0f);
    ImGui::Checkbox("Enable Shadow", &shadowEnabled);
    ImGui::Checkbox("Enable Dragon Shadow", &dragonShadowEnabled);
    ImGui::Checkbox("Enable Special", &specialEnabled);
    if (ImGui::TreeNode("Player Attack Reach Overrides"))
    {
        for (std::size_t index_ = 0; index_ < threats.size(); ++index_)
        {
            ImGui::PushID(static_cast<int>(index_));
            ImGui::InputText("Animation State", &threats[index_].animationState);
            ImGui::DragFloat("Reach", &threats[index_].reach, 0.05f, 0.0f, 100.0f);
            ImGui::DragFloat("Half Angle", &threats[index_].halfAngle, 1.0f, 0.0f, 180.0f);
            ImGui::PopID();
        }
        if (ImGui::Button("Add Reach Override")) threats.push_back({});
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Action Definitions"))
    {
        ImGui::TextDisabled("Phase settings: Enabled / Weight / Cooldown. Edit only while idle.");
        const bool busy_ = currentAction >= 0 || justGuardStunned;
        ImGui::BeginDisabled(busy_);
        for (std::size_t index_ = 0; index_ < actions.size(); ++index_)
        {
            auto& definition_ = actions[index_];
            ImGui::PushID(static_cast<int>(index_));
            if (ImGui::TreeNode(ActionName(static_cast<int>(index_))))
            {
                ImGui::Text("Remaining Cooldown: %.2f", index_ < cooldowns.size() ? cooldowns[index_] : 0.0f);
                int category_ = static_cast<int>(definition_.category);
                if (ImGui::Combo("Category", &category_, STATE_NAMES, IM_ARRAYSIZE(STATE_NAMES))) definition_.category = static_cast<HumanBossState>(category_);
                for (int phase_ = 0; phase_ < 3; ++phase_)
                {
                    ImGui::PushID(phase_);
                    ImGui::Text("PHASE_%d", phase_ + 1);
                    ImGui::Checkbox("Enabled", &definition_.enabled[phase_]);
                    ImGui::DragFloat("Weight", &definition_.weight[phase_], 0.5f, 0.0f, 1000.0f);
                    ImGui::DragFloat("Cooldown", &definition_.cooldown[phase_], 0.05f, 0.0f, 120.0f);
                    ImGui::PopID();
                }
                ImGui::DragFloat(definition_.category == HumanBossState::OFFENSE ? "Attack Start Min Distance" : "Min Distance", &definition_.minDistance, 0.05f, 0.0f, 1000.0f);
                ImGui::DragFloat(definition_.category == HumanBossState::OFFENSE ? "Attack Start Max Distance" : "Max Distance", &definition_.maxDistance, 0.05f, 0.0f, 1000.0f);
                if (definition_.type >= HumanBossActionType::SHADOW_SLASH && definition_.type <= HumanBossActionType::SHADOW_DOMAIN)
                {
                    const char* SHIFT_NAMES[] = { "BACK", "SIDE", "BEHIND", "APPROACH" };
                    ImGui::Combo("Shadow Shift Direction", &definition_.shadowShiftMode, SHIFT_NAMES, IM_ARRAYSIZE(SHIFT_NAMES));
                }
                ImGui::InputText("Cleanup Action", &definition_.skill.cleanupAction);
                for (std::size_t phaseIndex_ = 0; phaseIndex_ < definition_.skill.phases.size(); ++phaseIndex_)
                {
                    auto& skillPhase_ = definition_.skill.phases[phaseIndex_];
                    ImGui::PushID(static_cast<int>(phaseIndex_) + 10);
                    ImGui::Text("Skill Phase %d", static_cast<int>(phaseIndex_));
                    ImGui::InputText("Animation", &skillPhase_.animationState);
                    ImGui::InputText("Sequence", &skillPhase_.sequence);
                    ImGui::InputText("Motion Warp Preset", &skillPhase_.motionWarpPreset);
                    ImGui::Checkbox("Continue Previous Sequence", &skillPhase_.continueSequence);
                    ImGui::Checkbox("Play Animation From Sequence", &skillPhase_.playAnimationFromSequence);
                    ImGui::Checkbox("Wait For Animation End", &skillPhase_.waitForAnimationEnd);
                    ImGui::InputText("Enter Action", &skillPhase_.enterAction);
                    ImGui::InputText("Exit Action", &skillPhase_.exitAction);
                    int end_ = static_cast<int>(skillPhase_.endCondition);
                    const char* END_NAMES[] = { "ANIMATION_END", "DURATION", "ACTION_END", "GROUND_CONTACT" };
                    if (ImGui::Combo("End", &end_, END_NAMES, IM_ARRAYSIZE(END_NAMES))) skillPhase_.endCondition = static_cast<SkillPhaseEnd>(end_);
                    ImGui::DragFloat("Duration", &skillPhase_.duration, 0.05f, 0.0f, 120.0f);
                    ImGui::DragFloat("Maximum Duration", &skillPhase_.maximumDuration, 0.05f, 0.05f, 120.0f);
                    ImGui::DragFloat("Blend Duration", &skillPhase_.blendDuration, 0.01f, 0.0f, 5.0f);
                    ImGui::DragFloat("Animation Start Time", &skillPhase_.animationStartTime, 0.01f, 0.0f, 120.0f);
                    ImGui::Checkbox("Prevent Flinch During Strike", &skillPhase_.preventFlinch);
                    ImGui::DragFloat("Forward Speed", &skillPhase_.forwardSpeed, 0.1f, -100.0f, 100.0f);
                    ImGui::InputText("Forward Speed Curve", &skillPhase_.forwardSpeedCurveName);
                    ImGui::DragFloat("Warp Height", &skillPhase_.warpHeight, 0.1f, 0.0f, 10.0f);
                    ImGui::DragFloat("Vertical Speed", &skillPhase_.verticalSpeed, 0.1f, -60.0f, 60.0f);
                    ImGui::Checkbox("Suspend Gravity", &skillPhase_.suspendGravity);
                    ImGui::PopID();
                }
                if (ImGui::Button("Add Skill Phase")) definition_.skill.phases.push_back({});
                if (!definition_.skill.phases.empty())
                {
                    ImGui::SameLine();
                    if (ImGui::Button("Remove Last Skill Phase")) definition_.skill.phases.pop_back();
                }
                if (!IsAvailable(index_)) ImGui::TextDisabled("Unavailable: cooldown, repeat, distance, unreachable Warp, or missing assets.");
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        ImGui::EndDisabled();
        ImGui::TreePop();
    }
    NormalizeSettings();
}

void HumanBossAIComponent::Serialize(nlohmann::json& json_) const
{
    SerializeBerserk(json_);
    BS_PROFILE_SCOPE("HumanBossAIComponent::Serialize");
    mindSettings.Serialize(json_["Mind"]);
    SerializeSkillDefinition(justGuardReactionSkill, json_["JustGuardReactionSkill"]);
    json_["BehaviorGraphAssetPath"] = behaviorGraphAssetPath;
    behaviorGraph.Serialize(json_["BehaviorGraph"]);
    json_["PlayerObjectName"] = playerObjectName;
    json_["IdleState"] = idleState;
    json_["WalkState"] = walkState;
    json_["HitState"] = hitState;
    json_["HitRecoveryDuration"] = hitRecoveryDuration;
    json_["MinCombatDistance"] = minCombatDistance;
    json_["PreferredCombatDistance"] = preferredCombatDistance;
    json_["MaxCombatDistance"] = maxCombatDistance;
    json_["WaitMin"] = waitMin;
    json_["WaitMax"] = waitMax;
    json_["WalkSpeed"] = walkSpeed;
    json_["StrafeSpeed"] = strafeSpeed;
    json_["DodgeSpeed"] = dodgeSpeed;
    json_["ReactionCooldown"] = reactionCooldown;
    json_["DodgeCooldown"] = dodgeCooldown;
    json_["AttackCooldown"] = attackCooldown;
    json_["ReactionLookAhead"] = reactionLookAhead;
    json_["JustGuardLead"] = justGuardLead;
    json_["PlayerAttackReach"] = playerAttackReach;
    json_["PlayerAttackHalfAngle"] = playerAttackHalfAngle;
    json_["ThreatHeightTolerance"] = threatHeightTolerance;
    json_["NoneReactionWeight"] = noneReactionWeight;
    json_["RepositionTime"] = repositionTime;
    json_["Phase2Hp"] = phase2Hp;
    json_["Phase3Hp"] = phase3Hp;
    json_["PhaseReactionMultiplier"] = phaseReactionMultiplier;
    json_["ShadowEnabled"] = shadowEnabled;
    json_["DragonShadowEnabled"] = dragonShadowEnabled;
    json_["SpecialEnabled"] = specialEnabled;
    json_["Threats"] = nlohmann::json::array();
    for (const auto& threat_ : threats)
        json_["Threats"].push_back({ { "Animation", threat_.animationState }, { "Reach", threat_.reach }, { "HalfAngle", threat_.halfAngle } });
    json_["Actions"] = nlohmann::json::array();
    for (const auto& definition_ : actions)
    {
        nlohmann::json actionJson_;
        actionJson_["Type"] = static_cast<int>(definition_.type);
        actionJson_["Category"] = static_cast<int>(definition_.category);
        actionJson_["Enabled"] = definition_.enabled;
        actionJson_["Weight"] = definition_.weight;
        actionJson_["Cooldown"] = definition_.cooldown;
        actionJson_["ShadowShiftMode"] = definition_.shadowShiftMode;
        actionJson_["MinDistance"] = definition_.minDistance;
        actionJson_["MaxDistance"] = definition_.maxDistance;
        SerializeSkillDefinition(definition_.skill, actionJson_["Skill"]);
        json_["Actions"].push_back(actionJson_);
    }
}

void HumanBossAIComponent::Deserialize(const nlohmann::json& json_)
{
    DeserializeBerserk(json_);
    BS_PROFILE_SCOPE("HumanBossAIComponent::Deserialize");
    Stop();
    airHeavyPressureTimer = 0.0f;
    airHeavyMemoryTimer = 0.0f;
    airHeavyAttackChain = 0;
    airHeavyAttackSerial = 0;
    if (json_.contains("JustGuardReactionSkill"))
    {
        SkillDefinition reaction_;
        std::string error_;
        if (DeserializeSkillDefinition(json_["JustGuardReactionSkill"], reaction_, error_)) justGuardReactionSkill = std::move(reaction_);
    }
    // 省略項目は既存値を維持し、古いJSONへ新規設定を要求しない。
    if (json_.contains("Mind") && json_["Mind"].is_object()) mindSettings.Deserialize(json_["Mind"]);
    behaviorGraphAssetPath = json_.value("BehaviorGraphAssetPath", behaviorGraphAssetPath);
    // 外部参照がある場合は内包グラフを読み込まず、シーン保存時の古い内容による上書きを防ぐ。
    behaviorGraph.Clear();
    if (behaviorGraphAssetPath.empty() && json_.contains("BehaviorGraph"))
    {
        behaviorGraph.Deserialize(json_["BehaviorGraph"]);
        graphStatus = "Using embedded Behavior Graph";
    }
    EnsureDefaultBehaviorGraph();
    playerObjectName = json_.value("PlayerObjectName", playerObjectName);
    if (!behaviorGraphAssetPath.empty() && json_.contains("BehaviorGraph"))
        behaviorGraph.RestoreEditorLayout(json_["BehaviorGraph"]);
    idleState = json_.value("IdleState", idleState);
    walkState = json_.value("WalkState", walkState);
    hitState = json_.value("HitState", hitState);
    hitRecoveryDuration = json_.value("HitRecoveryDuration", hitRecoveryDuration);
    minCombatDistance = json_.value("MinCombatDistance", minCombatDistance);
    preferredCombatDistance = json_.value("PreferredCombatDistance", preferredCombatDistance);
    maxCombatDistance = json_.value("MaxCombatDistance", maxCombatDistance);
    waitMin = json_.value("WaitMin", waitMin);
    waitMax = json_.value("WaitMax", waitMax);
    walkSpeed = json_.value("WalkSpeed", walkSpeed);
    strafeSpeed = json_.value("StrafeSpeed", strafeSpeed);
    dodgeSpeed = json_.value("DodgeSpeed", dodgeSpeed);
    reactionCooldown = json_.value("ReactionCooldown", reactionCooldown);
    dodgeCooldown = json_.value("DodgeCooldown", dodgeCooldown);
    attackCooldown = json_.value("AttackCooldown", attackCooldown);
    reactionLookAhead = json_.value("ReactionLookAhead", reactionLookAhead);
    justGuardLead = json_.value("JustGuardLead", justGuardLead);
    playerAttackReach = json_.value("PlayerAttackReach", playerAttackReach);
    playerAttackHalfAngle = json_.value("PlayerAttackHalfAngle", playerAttackHalfAngle);
    threatHeightTolerance = json_.value("ThreatHeightTolerance", threatHeightTolerance);
    noneReactionWeight = json_.value("NoneReactionWeight", noneReactionWeight);
    repositionTime = json_.value("RepositionTime", repositionTime);
    phase2Hp = json_.value("Phase2Hp", phase2Hp);
    phase3Hp = json_.value("Phase3Hp", phase3Hp);
    phaseReactionMultiplier = json_.value("PhaseReactionMultiplier", phaseReactionMultiplier);
    shadowEnabled = json_.value("ShadowEnabled", shadowEnabled);
    dragonShadowEnabled = json_.value("DragonShadowEnabled", dragonShadowEnabled);
    specialEnabled = json_.value("SpecialEnabled", specialEnabled);
    if (json_.contains("Threats") && json_["Threats"].is_array())
    {
        threats.clear();
        for (const auto& threatJson_ : json_["Threats"])
            threats.push_back({ threatJson_.value("Animation", std::string()), threatJson_.value("Reach", playerAttackReach), threatJson_.value("HalfAngle", playerAttackHalfAngle) });
    }
    if (json_.contains("Actions") && json_["Actions"].is_array())
    {
        for (const auto& actionJson_ : json_["Actions"])
        {
            if (!actionJson_.is_object()) continue;
            const int type_ = actionJson_.value("Type", -1);
            if (type_ < 0 || type_ >= static_cast<int>(actions.size())) continue;
            auto& definition_ = actions[type_];
            definition_.category = static_cast<HumanBossState>(std::clamp(actionJson_.value("Category", static_cast<int>(definition_.category)), 0, 6));
            definition_.enabled = actionJson_.value("Enabled", definition_.enabled);
            definition_.weight = actionJson_.value("Weight", definition_.weight);
            definition_.cooldown = actionJson_.value("Cooldown", definition_.cooldown);
            definition_.shadowShiftMode = std::clamp(actionJson_.value("ShadowShiftMode", definition_.shadowShiftMode), 0, 3);
            definition_.minDistance = actionJson_.value("MinDistance", definition_.minDistance);
            definition_.maxDistance = actionJson_.value("MaxDistance", definition_.maxDistance);
            if (actionJson_.contains("Skill"))
            {
                SkillDefinition skill_;
                std::string error_;
                if (DeserializeSkillDefinition(actionJson_["Skill"], skill_, error_))
                {
                    // 明示的な空文字は補正OFFとして維持し、項目がない旧保存値だけ移行する。
                    const auto& phaseJson_ = actionJson_["Skill"]["Phases"];
                    for (std::size_t index_ = 0; index_ < skill_.phases.size(); ++index_)
                        if (!phaseJson_[index_].contains("MotionWarpPreset"))
                            skill_.phases[index_].motionWarpPreset = GetDefaultAttackWarpPreset(skill_.phases[index_].animationState, definition_.type);
                    definition_.skill = std::move(skill_);
                }
            }
        }
    }
    NormalizeSettings();
    cooldowns.assign(actions.size(), 0.0f);
}

void HumanBossAIComponent::EnsureDefaultBehaviorGraph()
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::EnsureDefaultBehaviorGraph");
    if (!behaviorGraph.GetNodes().empty()) return;
    if (!behaviorGraphAssetPath.empty() && LoadBehaviorGraphAsset(behaviorGraphAssetPath))
    {
        graphStatus = "Using external Behavior Graph: " + behaviorGraphAssetPath;
        return;
    }
    // 読み込み失敗を攻撃抽選へ隠さず、Editorで修復できる待機グラフとして残す。
    behaviorGraph.Clear();
    const uintptr_t rootId_ = behaviorGraph.AddNode(AI_BEHAVIOR_NODE_TYPE::ROOT, "HumanBoss", { 0.0f, 0.0f })->id;
    const uintptr_t waitId_ = behaviorGraph.AddNode(AI_BEHAVIOR_NODE_TYPE::WAIT, "Missing Graph Asset", { 300.0f, 0.0f })->id;
    behaviorGraph.FindNode(waitId_)->waitSeconds = 0.5f;
    behaviorGraph.AddLink(rootId_, waitId_);
    graphStatus = behaviorGraphAssetPath.empty()
        ? "Using wait graph: embedded Behavior Graph is empty"
        : "Using wait graph: failed to load external asset " + behaviorGraphAssetPath;
}

void HumanBossAIComponent::ResetBehaviorGraphRuntime()
{
    Stop();
}

bool HumanBossAIComponent::SaveBehaviorGraphAsset(const std::string& path_) const
{
    return behaviorGraph.SaveToFile(path_);
}

bool HumanBossAIComponent::LoadBehaviorGraphAsset(const std::string& path_)
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::LoadBehaviorGraphAsset");
    AIBehaviorGraph loaded_;
    // 壊れたJSONや空のアセットでは現在の編集内容を置き換えず、失敗をInspectorへ表示する。
    try
    {
        if (!loaded_.LoadFromFile(path_) || loaded_.GetNodes().empty())
        {
            graphStatus = "Failed to load: " + path_;
            return false;
        }
    }
    catch (const nlohmann::json::exception&)
    {
        graphStatus = "Invalid Behavior Graph JSON: " + path_;
        return false;
    }
    Stop();
    behaviorGraph = std::move(loaded_);
    behaviorGraphAssetPath = path_;
    graphStatus = "Loaded: " + path_;
    return true;
}

std::string HumanBossAIComponent::GetCurrentGraphActionName() const
{
    return currentAction >= 0 ? std::string("HumanBoss_") + ActionName(currentAction) : std::string();
}

float HumanBossAIComponent::GetGraphActionWeight(const std::string& actionName_, std::string* reason_) const
{
    const auto reject_ = [reason_](const char* message_)
    { if (reason_) *reason_ = message_; return 0.0f; };
    if (reason_) *reason_ = "Eligible: weighted candidate";
    if (justGuardStunned || stoppingAction) return reject_("JG recoil / action stopping");
    const auto owner_ = gameObject.lock();
    const auto matchesAction_ = [&actionName_, &owner_](const std::string& name_)
    {
        return actionName_ == name_ || (owner_ && actionName_ == owner_->BuildActionName(name_));
    };
    // 実行中は開始済みノードの終了監視だけを許可し、別Actionによる割り込みを防ぐ。
    if (currentAction >= 0) return matchesAction_(GetCurrentGraphActionName()) ? 1.0f : reject_("Another action is running");
    if (matchesAction_("HumanBoss_NoReaction"))
        return intent == HumanBossIntent::DEFEND && IsReactionReady() && !IsHeavyJustGuardReady() ? noneReactionWeight : 0.0f;
    if (matchesAction_("HumanBoss_Approach") || matchesAction_("HumanBoss_Reposition") || matchesAction_("HumanBoss_Observe")) return 1.0f;
    for (std::size_t index_ = 0; index_ < actions.size(); ++index_)
    {
        const auto& definition_ = actions[index_];
        if (!matchesAction_(std::string("HumanBoss_") + ActionName(static_cast<int>(index_)))) continue;
        // 観察中の影の派生攻撃も待機を守り、通常攻撃だけが止まる状態を防ぐ。
        if (waitTimer > 0.0f && definition_.category == HumanBossState::SHADOW &&
            definition_.type >= HumanBossActionType::SHADOW_SLASH) return reject_("Observation wait blocks shadow attack");
        if (!IsAvailable(index_, reason_)) return 0.0f;
        const float intentModifier_ = GetIntentActionModifier(index_);
        if (intentModifier_ <= 0.0f) return reject_("Current intent excludes this action");
        if (definition_.type == HumanBossActionType::COUNTER_ATTACK) return counterPending ? intentModifier_ : reject_("Confirmed counter not pending");
        if (definition_.type == HumanBossActionType::LIGHT_COUNTER) return lightCounterPending ? intentModifier_ : reject_("Light counter not pending");
        if (definition_.category == HumanBossState::REACTION)
        {
            if (!playerAttackDetected || reactionTimer > 0.0f || attackSerial == reactedSerial) return reject_("No new threat / reaction cooldown");
            if (definition_.type == HumanBossActionType::JUST_GUARD && (incomingTime > justGuardLead || incomingTime < -justGuardLead)) return reject_("Outside just-guard timing window");
        }
        if (definition_.category == HumanBossState::OFFENSE)
        {
            if ((waitTimer > 0.0f || repositionTimer > 0.0f) && definition_.type != HumanBossActionType::WHIFF_PUNISH) return reject_("Observation / reposition wait remaining");
        }
        return definition_.weight[phase] * intentModifier_;
    }
    // 未登録Actionで終了待ちへ入らず、追加済みEmitterなどの独立Actionだけを許可する。
    return owner_ && (owner_->GetLocalRegistry().HasAction(owner_->BuildActionName(actionName_)) ||
        owner_->GetLocalRegistry().HasAction(actionName_) || ActionRegistry::Instance().HasAction(actionName_)) ? 1.0f : 0.0f;
}

float HumanBossAIComponent::GetAttackDecisionDistance() const
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::GetAttackDecisionDistance");
    const auto executor_ = executor.lock();
    const auto target_ = target.lock();
    float range_ = maxCombatDistance;
    if (!executor_) return range_;
    for (const auto& definition_ : actions)
        if (definition_.enabled[phase] && definition_.category == HumanBossState::OFFENSE &&
            definition_.type != HumanBossActionType::COUNTER_ATTACK && definition_.type != HumanBossActionType::LIGHT_COUNTER && definition_.type < HumanBossActionType::SHADOW_CHARGE)
            range_ = (std::max)(range_, (std::min)(definition_.maxDistance, executor_->GetAttackStartReach(definition_.skill, target_)));
    return range_;
}

void HumanBossAIComponent::TickBehaviorGraph(float deltaTime_, bool finishCurrent_)
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::TickBehaviorGraph");
    const auto owner_ = GetGameObject();
    const auto target_ = target.lock();
    const auto blackboard_ = owner_ ? owner_->GetComponent<BlackboardComponent>() : nullptr;
    if (!owner_ || !blackboard_) return;
    PublishDebug();
    blackboard_->SetBool("HumanBossAI", "ActionEnded", finishCurrent_ || currentAction < 0);
    blackboard_->SetBool("HumanBossAI", "CounterReady", intent == HumanBossIntent::COUNTER_ATTACK && (counterPending || lightCounterPending));
    blackboard_->SetBool("HumanBossAI", "CanReact", intent == HumanBossIntent::DEFEND && IsReactionReady());
    // 専用枝で予兆付きの掴みを選び、通常攻撃や影移動の抽選に埋もれさせない。
    blackboard_->SetBool("HumanBossAI", "GuardExecutionReady", GetGraphActionWeight("HumanBoss_GUARD_EXECUTION") > 0.0f);
    blackboard_->SetBool("HumanBossAI", "NeedsReposition", intent == HumanBossIntent::REPOSITION || intent == HumanBossIntent::DISENGAGE || (distance < minCombatDistance && waitTimer > 0.0f));
    // 攻撃の有効距離が適正距離より短い設定でも、攻撃可能な位置まで接近する。
    float maximumAttackReach_ = 0.0f;
    const auto executor_ = executor.lock();
    for (std::size_t index_ = 0; index_ < actions.size(); ++index_)
    {
        const auto& definition_ = actions[index_];
        if (executor_ && definition_.enabled[phase] && definition_.category == HumanBossState::OFFENSE &&
            definition_.type != HumanBossActionType::COUNTER_ATTACK && definition_.type != HumanBossActionType::LIGHT_COUNTER &&
            GetIntentActionModifier(index_) > 0.0f && definition_.type < HumanBossActionType::SHADOW_CHARGE)
            maximumAttackReach_ = (std::max)(maximumAttackReach_, (std::min)(definition_.maxDistance,
                executor_->GetAttackStartReach(definition_.skill, target_)));
    }
    const float approachDistance_ = maximumAttackReach_ > 0.0f
        ? (std::min)(preferredCombatDistance + 0.25f, (std::max)(0.1f, maximumAttackReach_ - 0.1f)) : preferredCombatDistance + 0.25f;
    bool canShadow_ = false;
    if (shadowEnabled)
        for (std::size_t index_ = 0; index_ < actions.size(); ++index_)
            if (actions[index_].category == HumanBossState::SHADOW && GetIntentActionModifier(index_) > 0.0f && IsAvailable(index_))
            { canShadow_ = true; break; }
    blackboard_->SetBool("HumanBossAI", "CanShadow", canShadow_);
    const bool canRanged_ = currentAction < 0 &&
        (GetGraphActionWeight("HumanBoss_SHADOW_CHARGE") > 0.0f || GetGraphActionWeight("HumanBoss_SHADOW_ORB") > 0.0f);
    blackboard_->SetBool("HumanBossAI", "CanRangedOffense", canRanged_);
    const auto specialIndex_ = static_cast<std::size_t>(HumanBossActionType::SHADOW_RAGNAROK);
    const bool canSpecial_ = currentAction < 0 && hitTimer <= 0.0f && IsAvailable(specialIndex_);
    blackboard_->SetBool("HumanBossAI", "CanUseSpecialAttack", canSpecial_);
    // 初回は通常のGuard/Dodgeより先に選ぶ。実行中Skillと強制被弾の優先制御は維持する。
    blackboard_->SetBool("HumanBossAI", "InitialSpecialReady", canSpecial_ && executor_ && !executor_->WasRagnarokUsed());
    blackboard_->SetFloat("HumanBossAI", "SpecialAttackCooldown", cooldowns[specialIndex_]);
    if (executor_)
    {
        const auto health_ = owner_->GetComponent<HealthComponent>();
        const bool hpUnlocked_ = health_ && !health_->IsDead() && health_->GetMaxHealth() > 0.0f &&
            health_->GetCurrentHealth() / health_->GetMaxHealth() <= executor_->GetRagnarokHpThreshold();
        blackboard_->SetBool("HumanBossAI", "SpecialAttackUnlocked", hpUnlocked_);
        executor_->CanUseRagnarok();
        blackboard_->SetString("HumanBossAI", "SpecialAttackBlockReason", executor_->GetRagnarokBlockReason());
    }
    // 補正で届く攻撃がある時はApproachで先に潰さず、既存の重み・Intentで攻撃を抽選する。
    bool hasReachableOffense_ = false;
    for (std::size_t index_ = 0; index_ < actions.size(); ++index_)
        if (actions[index_].category == HumanBossState::OFFENSE && actions[index_].type != HumanBossActionType::COUNTER_ATTACK &&
            actions[index_].type != HumanBossActionType::LIGHT_COUNTER && IsAvailable(index_) && GetIntentActionModifier(index_) > 0.0f)
        { hasReachableOffense_ = true; break; }
    const bool canPunishAtDistance_ = intent == HumanBossIntent::PUNISH && IsAvailable(static_cast<std::size_t>(HumanBossActionType::WHIFF_PUNISH));
    blackboard_->SetBool("HumanBossAI", "NeedsApproach", (distance > approachDistance_ || (airHeavyPressureTimer > 0.0f && distance > mindSettings.airLandingPressureDistance)) && !canPunishAtDistance_ && !hasReachableOffense_);
    blackboard_->SetBool("HumanBossAI", "NeedsObservation", intent == HumanBossIntent::OBSERVE || (waitTimer > 0.0f && intent != HumanBossIntent::PUNISH && intent != HumanBossIntent::COUNTER_ATTACK) || (intent == HumanBossIntent::BAIT && baitTimer > 0.0f));
    blackboard_->SetBool("HumanBossAI", "CanOffense", intent != HumanBossIntent::OBSERVE && intent != HumanBossIntent::DEFEND && ((waitTimer <= 0.0f && repositionTimer <= 0.0f) || intent == HumanBossIntent::PUNISH) && hasReachableOffense_);
    blackboard_->SetFloat("HumanBossAI", "WaitRemaining", waitTimer);
    behaviorContext = {};
    behaviorContext.owner = owner_.get();
    behaviorContext.target = target_.get();
    behaviorContext.blackboard = blackboard_.get();
    behaviorContext.deltaTime = deltaTime_;
    behaviorContext.hasTarget = target_ != nullptr;
    behaviorContext.targetDistance = distance;
    if (owner_->manager)
        if (auto mode_ = owner_->GetManager().GetGameModeManager()) behaviorContext.globalBlackboard = &mode_->GetGlobalBlackboard();
    const auto trace_ = owner_->GetComponent<HumanBossAITraceComponent>();
    const bool traceEnabled_ = trace_ && trace_->IsRecording();
    if (traceEnabled_) trace_->BeginEvaluation();
    behaviorContext.actionWeightModifier = [this, trace_, traceEnabled_](const std::string& actionName_)
    {
        std::string reason_;
        const float weight_ = GetGraphActionWeight(actionName_, traceEnabled_ ? &reason_ : nullptr);
        if (traceEnabled_) trace_->ObserveActionWeight(actionName_, weight_, reason_);
        return weight_;
    };
    if (traceEnabled_)
    {
        behaviorContext.actionWeightObserver = [trace_](const std::string& action_, float base_, float modifier_)
        { trace_->ObserveGraphWeight(action_, base_, modifier_); };
        behaviorContext.nodeEvaluationObserver = [trace_, this](const AIBehaviorNode& node_, AI_BEHAVIOR_STATUS status_)
        { trace_->ObserveNode(node_, status_, behaviorContext); };
    }
    const AI_BEHAVIOR_STATUS status_ = behaviorGraph.Tick(behaviorContext);
    if (traceEnabled_)
    {
        trace_->FinishEvaluation();
        trace_->Capture(*this, finishCurrent_ ? "Action finished" : "Graph decision", finishCurrent_);
    }
    // Contextは次のTickまで保持するため、Componentの所有循環を作らないようCallbackを外す。
    behaviorContext.actionWeightModifier = {};
    behaviorContext.actionWeightObserver = {};
    behaviorContext.nodeEvaluationObserver = {};
    if (status_ == AI_BEHAVIOR_STATUS::FAILURE && currentAction < 0)
        if (const auto executor_ = executor.lock()) executor_->SetLocomotion(idleState, { 0.0f, 0.0f, 0.0f }, toPlayer, { 0.0f, 0.0f });
}

void HumanBossAIComponent::UpdateLocomotion(const std::string& actionName_)
{
    BS_PROFILE_SCOPE("HumanBossAIComponent::UpdateLocomotion");
    if (currentAction >= 0 || justGuardStunned || stoppingAction) return;
    if (actionName_ == "HumanBoss_NoReaction")
    {
        reactedSerial = attackSerial;
        reactionTimer = reactionCooldown * phaseReactionMultiplier[phase];
        reactionReason = "NONE selected by BehaviorGraph";
    }
    const auto executor_ = executor.lock();
    if (!executor_) return;
    // 受け流しの受付を待つ間は横移動や接近を止め、攻撃との間合いを保つ。
    if (intent == HumanBossIntent::DEFEND && playerAttackDetected)
    {
        state = HumanBossState::NEUTRAL;
        executor_->SetLocomotion(idleState, { 0.0f, 0.0f, 0.0f }, toPlayer, { 0.0f, 0.0f });
        return;
    }
    DirectX::XMFLOAT3 velocity_{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT2 blend_{ 0.0f, 0.0f };
    state = HumanBossState::NEUTRAL;
    if (actionName_ == "HumanBoss_Approach")
    {
        const float chaseSpeed_ = walkSpeed * (airHeavyPressureTimer > 0.0f ? mindSettings.airHeavyChaseSpeedMultiplier : 1.0f);
        velocity_ = { toPlayer.x * chaseSpeed_, 0.0f, toPlayer.z * chaseSpeed_ };
        blend_.y = 1.0f;
    }
    else if (actionName_ == "HumanBoss_Reposition")
    {
        state = HumanBossState::REPOSITION;
        if (distance < preferredCombatDistance) { velocity_ = { -toPlayer.x * walkSpeed, 0.0f, -toPlayer.z * walkSpeed }; blend_.y = -1.0f; }
        else { velocity_ = { toPlayer.z * strafeDirection * strafeSpeed, 0.0f, -toPlayer.x * strafeDirection * strafeSpeed }; blend_.x = strafeDirection; }
    }
    else if (waitTimer > (waitMin + waitMax) * 0.25f)
    {
        velocity_ = { toPlayer.z * strafeDirection * strafeSpeed, 0.0f, -toPlayer.x * strafeDirection * strafeSpeed };
        blend_.x = strafeDirection;
    }
    executor_->SetLocomotion(blend_.x == 0.0f && blend_.y == 0.0f ? idleState : walkState, velocity_, toPlayer, blend_);
}
