#include "PrecompiledHeader.h"

#include "DragonBodyPartManagerComponent.h"

#include "DragonBodyPartHurtBoxComponent.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/System/AttackSystem.h"
#include "Engine/Physics/HurtBox.h"
#include "Game/GameMode/GameModeManager.h"

#include <algorithm>
#include <cstdio>
#include <imgui.h>
#include <string>

namespace
{
    constexpr DRAGON_BODY_PART DEFAULT_BODY_PARTS[] =
    {
        DRAGON_BODY_PART::HEAD,
        DRAGON_BODY_PART::NECK,
        DRAGON_BODY_PART::CHEST,
        DRAGON_BODY_PART::BODY,
        DRAGON_BODY_PART::LEFT_FRONT_LEG,
        DRAGON_BODY_PART::RIGHT_FRONT_LEG,
        DRAGON_BODY_PART::LEFT_BACK_LEG,
        DRAGON_BODY_PART::RIGHT_BACK_LEG,
        DRAGON_BODY_PART::LEFT_WING,
        DRAGON_BODY_PART::RIGHT_WING,
        DRAGON_BODY_PART::TAIL
    };

    // 現在値と最大値からUI表示用の0から1の割合を作る。
    float CalculateProgressRatio(float currentValue_, float maxValue_)
    {
        if (maxValue_ <= 0.0f)
        {
            return 0.0f;
        }

        return (std::clamp)(currentValue_ / maxValue_, 0.0f, 1.0f);
    }

    // ProgressBar内に表示する現在値/最大値の文字列を作る。
    std::string MakeProgressLabel(float currentValue_, float maxValue_)
    {
        char label_[64]{};
        snprintf(
            label_,
            sizeof(label_),
            "%.0f / %.0f",
            currentValue_,
            maxValue_);
        return label_;
    }

    // 部位耐久や怯み値をImGuiのProgressBarで表示する。
    void DrawBodyPartProgressBar(
        const char* label_,
        float currentValue_,
        float maxValue_,
        const ImVec4& color_)
    {
        ImGui::TextUnformatted(label_);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color_);
        const std::string progressLabel_ =
            MakeProgressLabel(currentValue_, maxValue_);
        ImGui::ProgressBar(
            CalculateProgressRatio(currentValue_, maxValue_),
            ImVec2(-1.0f, 0.0f),
            progressLabel_.c_str());
        ImGui::PopStyleColor();
    }
}

// DRAGON_BODY_PARTを保存やDebug表示に使う文字列へ変換する。
const char* GetDragonBodyPartName(DRAGON_BODY_PART bodyPart_)
{
    switch (bodyPart_)
    {
    case DRAGON_BODY_PART::HEAD: return "HEAD";
    case DRAGON_BODY_PART::NECK: return "NECK";
    case DRAGON_BODY_PART::CHEST: return "CHEST";
    case DRAGON_BODY_PART::BODY: return "BODY";
    case DRAGON_BODY_PART::LEFT_FRONT_LEG: return "LEFT_FRONT_LEG";
    case DRAGON_BODY_PART::RIGHT_FRONT_LEG: return "RIGHT_FRONT_LEG";
    case DRAGON_BODY_PART::LEFT_BACK_LEG: return "LEFT_BACK_LEG";
    case DRAGON_BODY_PART::RIGHT_BACK_LEG: return "RIGHT_BACK_LEG";
    case DRAGON_BODY_PART::LEFT_WING: return "LEFT_WING";
    case DRAGON_BODY_PART::RIGHT_WING: return "RIGHT_WING";
    case DRAGON_BODY_PART::TAIL: return "TAIL";
    default: return "BODY";
    }
}

// JSONなどから読んだ文字列をDRAGON_BODY_PARTへ変換する。
DRAGON_BODY_PART ParseDragonBodyPart(const std::string& bodyPartName_)
{
    for (const DRAGON_BODY_PART bodyPart_ : DEFAULT_BODY_PARTS)
    {
        if (bodyPartName_ == GetDragonBodyPartName(bodyPart_))
        {
            return bodyPart_;
        }
    }
    return DRAGON_BODY_PART::BODY;
}

// 部位設定、Health参照、HitCallback、Blackboard初期値を準備する。
void DragonBodyPartManagerComponent::Start()
{
    EnsureDefaultBodyParts();
    ResolveHealthComponent();
    UpdateBossHpRate();
    RegisterAttackHitCallback();
    PublishLastDamageToBlackboard();
}

// DamageReactionのCooldownとBoss HP割合を毎フレーム更新する。
void DragonBodyPartManagerComponent::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("DragonBodyPartManagerComponent::Update");

    reactionCooldownTimer = (std::max)(0.0f, reactionCooldownTimer - deltaTime_);
    UpdateBossHpRate();
}

// 指定部位にダメージを適用し、本体Health、部位耐久、怯み値、破壊状態を更新する。
BodyPartDamageResult DragonBodyPartManagerComponent::ApplyDamage(
    DRAGON_BODY_PART bodyPart_,
    const DamageContext& damageContext_)
{
    EnsureDefaultBodyParts();
    ResolveHealthComponent();

    BodyPartDamageResult result_;
    result_.bodyPart = bodyPart_;

    DragonBodyPartStatus* status_ = FindBodyPartStatus(bodyPart_);
    if (!status_) return result_;

    result_.alreadyBroken = status_->broken;
    // 破壊済み部位はHealthダメージ倍率だけ追加し、部位耐久は0のまま扱う。
    const float healthDamageRate_ = status_->broken
        ? status_->healthDamageRate * status_->brokenHealthDamageRate
        : status_->healthDamageRate;
    result_.appliedHealthDamage =
        (std::max)(damageContext_.healthDamage, 0.0f) *
        (std::max)(healthDamageRate_, 0.0f);
    result_.appliedDurabilityDamage =
        (std::max)(damageContext_.durabilityDamage, 0.0f) *
        (std::max)(status_->durabilityDamageRate, 0.0f);
    const float appliedStaggerDamage_ =
        (std::max)(damageContext_.durabilityDamage, 0.0f) *
        (std::max)(status_->staggerDamageRate, 0.0f);

    if (health)
    {
        // 部位倍率適用後のダメージを本体HPへ反映する。
        health->ApplyDamage(result_.appliedHealthDamage);
    }
    UpdateBossHpRate();

    const bool wasBroken_ = status_->broken;
    status_->currentDurability = (std::max)(
        status_->currentDurability - result_.appliedDurabilityDamage,
        0.0f);
    status_->currentStagger += appliedStaggerDamage_;
    status_->broken = status_->currentDurability <= 0.0f;
    result_.bodyPartBrokenThisHit = !wasBroken_ && status_->broken;

    // 直近Hit情報を保存し、Debug表示やBlackboard参照へ使えるようにする。
    lastDamageResult = result_;
    lastAttackInstanceId = damageContext_.attackInstanceId;
    if (const std::shared_ptr<GameObject> attacker_ =
        damageContext_.attacker.lock())
    {
        lastAttackerName = attacker_->GetName();
    }
    PublishLastDamageToBlackboard();

    if (result_.bodyPartBrokenThisHit)
    {
        // 破壊がこのHitで初めて発生した場合だけ破壊Actionを実行する。
        ExecuteBodyPartBrokenActions(*status_);
    }
    TryRequestDamageReaction(*status_);

    return result_;
}

// AttackSystemのHitEventを、Dragon部位用のDamageContextへ変換する。
void DragonBodyPartManagerComponent::HandleAttackHit(
    const AttackHitEvent& event_,
    const DragonBodyPartHurtBoxComponent& hurtBoxComponent_)
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    const std::shared_ptr<GameObject> target_ = event_.target.lock();
    // このManagerのOwnerがHit対象ではない場合は無視する。
    if (!owner_ || target_ != owner_) return;

    DamageContext damageContext_;
    damageContext_.attackInstanceId = event_.attackInstanceId;
    damageContext_.attacker = event_.attacker;
    damageContext_.targetCombatOwner = event_.target;
    damageContext_.healthDamage = event_.damage;
    damageContext_.durabilityDamage = event_.damage;
    damageContext_.hitPosition = event_.hitPosition;
    damageContext_.hitDirection = event_.hitNormal;

    if (event_.hurtBox)
    {
        if (const std::shared_ptr<GameObject> hurtBoxObject_ =
            event_.hurtBox->GetGameObject())
        {
            lastHurtBoxName = hurtBoxObject_->GetName();
        }
    }

    ApplyDamage(hurtBoxComponent_.GetBodyPart(), damageContext_);
}

// 指定部位の状態を読み取り専用で取得する。
const DragonBodyPartStatus* DragonBodyPartManagerComponent::GetBodyPartStatus(
    DRAGON_BODY_PART bodyPart_) const
{
    for (const DragonBodyPartStatus& status_ : bodyParts)
    {
        if (status_.bodyPart == bodyPart_) return &status_;
    }
    return nullptr;
}

// 指定部位が破壊済みかを返す。
bool DragonBodyPartManagerComponent::IsBodyPartBroken(
    DRAGON_BODY_PART bodyPart_) const
{
    const DragonBodyPartStatus* status_ = GetBodyPartStatus(bodyPart_);
    return status_ && status_->broken;
}

// 全部位の耐久と怯みを初期値へ戻し、直近Hit情報も消す。
void DragonBodyPartManagerComponent::ResetBodyParts()
{
    EnsureDefaultBodyParts();
    for (DragonBodyPartStatus& status_ : bodyParts)
    {
        status_.currentDurability = status_.maxDurability;
        status_.currentMaxStagger = status_.baseMaxStagger;
        status_.currentStagger = 0.0f;
        status_.broken = false;
    }
    lastDamageResult = {};
    lastAttackerName.clear();
    lastHurtBoxName.clear();
    lastAttackInstanceId = 0;
    UpdateBossHpRate();
    PublishLastDamageToBlackboard();
}

// DEFAULT_BODY_PARTSに存在する部位が未登録なら、標準値で追加する。
void DragonBodyPartManagerComponent::EnsureDefaultBodyParts()
{
    for (const DRAGON_BODY_PART bodyPart_ : DEFAULT_BODY_PARTS)
    {
        if (!FindBodyPartStatus(bodyPart_))
        {
            DragonBodyPartStatus status_;
            status_.bodyPart = bodyPart_;
            bodyParts.push_back(status_);
        }
    }
}

// bodyParts配列から指定部位を探す。
DragonBodyPartStatus* DragonBodyPartManagerComponent::FindBodyPartStatus(
    DRAGON_BODY_PART bodyPart_)
{
    for (DragonBodyPartStatus& status_ : bodyParts)
    {
        if (status_.bodyPart == bodyPart_) return &status_;
    }
    return nullptr;
}

// OwnerにHealthComponentがなければ追加し、以降の部位ダメージ反映先にする。
void DragonBodyPartManagerComponent::ResolveHealthComponent()
{
    if (health) return;

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    health = owner_->GetOrAddComponent<HealthComponent>().get();
}

// 本体Healthの割合をGameModeManagerへ同期し、Boss HP UIや戦闘進行で参照できるようにする。
void DragonBodyPartManagerComponent::UpdateBossHpRate()
{
    BS_PROFILE_SCOPE("DragonBodyPartManagerComponent::UpdateBossHpRate");

    ResolveHealthComponent();

    float bossHpRate_ = 1.0f;
    if (health && health->GetMaxHealth() > 0.0f)
    {
        bossHpRate_ = (std::clamp)(
            health->GetCurrentHealth() / health->GetMaxHealth(),
            0.0f,
            1.0f);
    }

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    GameModeManager* gameModeManager_ = owner_->GetManager().GetGameModeManager();
    if (!gameModeManager_) return;

    gameModeManager_->SetBossHpRate(bossHpRate_);
}

// AttackSystemのHit通知を購読し、DragonBodyPartHurtBoxComponentが付いたHurtBoxだけ処理する。
void DragonBodyPartManagerComponent::RegisterAttackHitCallback()
{
    if (callbackRegistered) return;

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    std::weak_ptr<DragonBodyPartManagerComponent> weakManager_;
    for (const std::shared_ptr<Component>& component_ : owner_->components)
    {
        const std::shared_ptr<DragonBodyPartManagerComponent> manager_ =
            std::dynamic_pointer_cast<DragonBodyPartManagerComponent>(component_);
        if (manager_.get() == this)
        {
            weakManager_ = manager_;
            break;
        }
    }

    owner_->GetManager().GetAttackSystem().AddHitCallback(
        [weakManager_](const AttackHitEvent& event_)
        {
            const std::shared_ptr<DragonBodyPartManagerComponent> manager_ =
                weakManager_.lock();
            if (!manager_ || !event_.hurtBox) return;

            const std::shared_ptr<GameObject> hurtBoxObject_ =
                event_.hurtBox->GetGameObject();
            if (!hurtBoxObject_) return;

            const std::shared_ptr<DragonBodyPartHurtBoxComponent> hurtBoxComponent_ =
                hurtBoxObject_->GetComponent<DragonBodyPartHurtBoxComponent>();
            if (!hurtBoxComponent_) return;

            // HurtBox側に設定された部位情報を使って、本体ManagerへHit処理を戻す。
            manager_->HandleAttackHit(event_, *hurtBoxComponent_);
        });
    callbackRegistered = true;
}

// 直近Hitの部位、Healthダメージ、耐久ダメージ、破壊発生をBlackboardへ保存する。
void DragonBodyPartManagerComponent::PublishLastDamageToBlackboard()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    const std::shared_ptr<BlackboardComponent> blackboard_ =
        owner_->GetOrAddComponent<BlackboardComponent>();
    if (!blackboard_) return;

    blackboard_->SetInt(
        "DragonBodyPart",
        "LastBodyPart",
        static_cast<int>(lastDamageResult.bodyPart));
    blackboard_->SetFloat(
        "DragonBodyPart",
        "LastHealthDamage",
        lastDamageResult.appliedHealthDamage);
    blackboard_->SetFloat(
        "DragonBodyPart",
        "LastDurabilityDamage",
        lastDamageResult.appliedDurabilityDamage);
    blackboard_->SetBool(
        "DragonBodyPart",
        "LastBrokenThisHit",
        lastDamageResult.bodyPartBrokenThisHit);
}

// 怯み値がしきい値に達した場合、DragonAIへDamageReaction Actionを要求する。
void DragonBodyPartManagerComponent::TryRequestDamageReaction(
    DragonBodyPartStatus& status_)
{
    BS_PROFILE_SCOPE("DragonBodyPartManagerComponent::TryRequestDamageReaction");

    if (!reactionEnabled ||
        reactionActionName.empty() ||
        reactionCooldownTimer > 0.0f ||
        status_.currentMaxStagger <= 0.0f ||
        status_.currentStagger < status_.currentMaxStagger)
    {
        return;
    }

    status_.currentStagger = 0.0f;
    // 怯み発生後は次回しきい値を引き上げ、連続怯みが起きすぎないようにする。
    status_.baseMaxStagger *= (std::max)(status_.staggerRecoveryBaseMultiplier, 1.0f);
    status_.currentMaxStagger = status_.baseMaxStagger;
    reactionCooldownTimer = (std::max)(reactionCooldownTime, 0.0f);

    if (const std::shared_ptr<GameObject> owner_ = GetGameObject())
    {
        owner_->ExecuteAction(reactionActionName);
    }
}

// 共通破壊Action、部位名付きAction、部位個別Actionを順に実行する。
void DragonBodyPartManagerComponent::ExecuteBodyPartBrokenActions(
    const DragonBodyPartStatus& status_)
{
    BS_PROFILE_SCOPE("DragonBodyPartManagerComponent::ExecuteBodyPartBrokenActions");

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    if (!commonBrokenActionName.empty())
    {
        owner_->ExecuteAction(commonBrokenActionName);
    }
    owner_->ExecuteAction(
        std::string("DragonBodyPart_Broken_") +
        GetDragonBodyPartName(status_.bodyPart));
    if (!status_.brokenActionName.empty())
    {
        owner_->ExecuteAction(status_.brokenActionName);
    }
}

// 部位耐久、怯み、直近Hit、破壊Action設定を編集するDebug UIを描画する。
void DragonBodyPartManagerComponent::Gui()
{
    BS_PROFILE_SCOPE("DragonBodyPartManagerComponent::Gui");

    EnsureDefaultBodyParts();
    ResolveHealthComponent();

    ImGui::Text("Health Component: %s", health ? "Ready" : "None");
    ImGui::Checkbox("Debug Log", &debugLogEnabled);
    ImGui::Checkbox("Damage Reaction", &reactionEnabled);
    ImGui::DragFloat("Reaction Cooldown", &reactionCooldownTime, 0.1f, 0.0f, 30.0f);
    ImGui::Text("Reaction Cooldown Timer: %.2f", reactionCooldownTimer);
    char reactionActionBuffer_[128]{};
    strncpy_s(reactionActionBuffer_, reactionActionName.c_str(), sizeof(reactionActionBuffer_));
    if (ImGui::InputText("Reaction Action", reactionActionBuffer_, sizeof(reactionActionBuffer_)))
    {
        reactionActionName = reactionActionBuffer_;
    }
    char commonBrokenActionBuffer_[128]{};
    strncpy_s(commonBrokenActionBuffer_, commonBrokenActionName.c_str(), sizeof(commonBrokenActionBuffer_));
    if (ImGui::InputText("Common Broken Action", commonBrokenActionBuffer_, sizeof(commonBrokenActionBuffer_)))
    {
        commonBrokenActionName = commonBrokenActionBuffer_;
    }
    if (ImGui::Button("Reset Body Parts")) ResetBodyParts();

    ImGui::SeparatorText("Last Hit");
    ImGui::Text("Attack ID: %llu", static_cast<unsigned long long>(lastAttackInstanceId));
    ImGui::Text("Body Part: %s", GetDragonBodyPartName(lastDamageResult.bodyPart));
    ImGui::Text("Health Damage: %.2f", lastDamageResult.appliedHealthDamage);
    ImGui::Text("Durability Damage: %.2f", lastDamageResult.appliedDurabilityDamage);
    ImGui::Text("Broken This Hit: %s", lastDamageResult.bodyPartBrokenThisHit ? "Yes" : "No");
    ImGui::Text("Attacker: %s", lastAttackerName.c_str());
    ImGui::Text("HurtBox: %s", lastHurtBoxName.c_str());

    ImGui::SeparatorText("Body Part Overview");
    if (ImGui::BeginTable(
        "DragonBodyPartOverview",
        4,
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Part", ImGuiTableColumnFlags_WidthFixed, 145.0f);
        ImGui::TableSetupColumn("Durability", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Stagger", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableHeadersRow();

        for (const DragonBodyPartStatus& status_ : bodyParts)
        {
            ImGui::PushID(static_cast<int>(status_.bodyPart));
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(GetDragonBodyPartName(status_.bodyPart));

            ImGui::TableSetColumnIndex(1);
            const ImVec4 durabilityColor_ = status_.broken
                ? ImVec4(0.85f, 0.18f, 0.12f, 1.0f)
                : ImVec4(0.20f, 0.72f, 0.32f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, durabilityColor_);
            const std::string durabilityLabel_ =
                MakeProgressLabel(status_.currentDurability, status_.maxDurability);
            ImGui::ProgressBar(
                CalculateProgressRatio(status_.currentDurability, status_.maxDurability),
                ImVec2(-1.0f, 0.0f),
                durabilityLabel_.c_str());
            ImGui::PopStyleColor();

            ImGui::TableSetColumnIndex(2);
            ImGui::PushStyleColor(
                ImGuiCol_PlotHistogram,
                ImVec4(0.95f, 0.64f, 0.18f, 1.0f));
            const std::string staggerLabel_ =
                MakeProgressLabel(status_.currentStagger, status_.currentMaxStagger);
            ImGui::ProgressBar(
                CalculateProgressRatio(status_.currentStagger, status_.currentMaxStagger),
                ImVec2(-1.0f, 0.0f),
                staggerLabel_.c_str());
            ImGui::PopStyleColor();

            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(status_.broken ? "Broken" : "Active");
            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::SeparatorText("Body Parts");
    for (DragonBodyPartStatus& status_ : bodyParts)
    {
        if (ImGui::TreeNode(GetDragonBodyPartName(status_.bodyPart)))
        {
            const ImVec4 durabilityColor_ = status_.broken
                ? ImVec4(0.85f, 0.18f, 0.12f, 1.0f)
                : ImVec4(0.20f, 0.72f, 0.32f, 1.0f);
            DrawBodyPartProgressBar(
                "Durability",
                status_.currentDurability,
                status_.maxDurability,
                durabilityColor_);
            DrawBodyPartProgressBar(
                "Stagger",
                status_.currentStagger,
                status_.currentMaxStagger,
                ImVec4(0.95f, 0.64f, 0.18f, 1.0f));
            ImGui::DragFloat("Max Durability", &status_.maxDurability, 1.0f, 1.0f, 99999.0f);
            status_.currentDurability = (std::clamp)(
                status_.currentDurability,
                0.0f,
                status_.maxDurability);
            ImGui::DragFloat("Current Durability", &status_.currentDurability, 1.0f, 0.0f, status_.maxDurability);
            ImGui::DragFloat("Health Damage Rate", &status_.healthDamageRate, 0.05f, 0.0f, 10.0f);
            ImGui::DragFloat("Broken Health Rate", &status_.brokenHealthDamageRate, 0.05f, 0.0f, 10.0f);
            ImGui::DragFloat("Durability Damage Rate", &status_.durabilityDamageRate, 0.05f, 0.0f, 10.0f);
            ImGui::DragFloat("Base Max Stagger", &status_.baseMaxStagger, 1.0f, 0.0f, 99999.0f);
            ImGui::DragFloat("Current Max Stagger", &status_.currentMaxStagger, 1.0f, 0.0f, 99999.0f);
            ImGui::DragFloat("Current Stagger", &status_.currentStagger, 1.0f, 0.0f, 99999.0f);
            ImGui::DragFloat("Stagger Damage Rate", &status_.staggerDamageRate, 0.05f, 0.0f, 10.0f);
            ImGui::DragFloat("Stagger Recovery Base Multiplier", &status_.staggerRecoveryBaseMultiplier, 0.01f, 1.0f, 10.0f);
            char brokenActionBuffer_[128]{};
            strncpy_s(brokenActionBuffer_, status_.brokenActionName.c_str(), sizeof(brokenActionBuffer_));
            if (ImGui::InputText("Broken Action", brokenActionBuffer_, sizeof(brokenActionBuffer_)))
            {
                status_.brokenActionName = brokenActionBuffer_;
            }
            status_.broken = status_.currentDurability <= 0.0f;
            ImGui::Text("Broken: %s", status_.broken ? "Yes" : "No");
            ImGui::TreePop();
        }
    }
}

// 部位設定、Reaction設定、破壊Action名をJSONへ保存する。
void DragonBodyPartManagerComponent::Serialize(nlohmann::json& json_) const
{
    json_["DebugLogEnabled"] = debugLogEnabled;
    json_["ReactionEnabled"] = reactionEnabled;
    json_["ReactionCooldownTime"] = reactionCooldownTime;
    json_["ReactionCooldownTimer"] = reactionCooldownTimer;
    json_["ReactionActionName"] = reactionActionName;
    json_["CommonBrokenActionName"] = commonBrokenActionName;
    json_["BodyParts"] = nlohmann::json::array();
    for (const DragonBodyPartStatus& status_ : bodyParts)
    {
        json_["BodyParts"].push_back({
            { "BodyPart", GetDragonBodyPartName(status_.bodyPart) },
            { "MaxDurability", status_.maxDurability },
            { "CurrentDurability", status_.currentDurability },
            { "HealthDamageRate", status_.healthDamageRate },
            { "DurabilityDamageRate", status_.durabilityDamageRate },
            { "BrokenHealthDamageRate", status_.brokenHealthDamageRate },
            { "BaseMaxStagger", status_.baseMaxStagger },
            { "CurrentMaxStagger", status_.currentMaxStagger },
            { "CurrentStagger", status_.currentStagger },
            { "StaggerDamageRate", status_.staggerDamageRate },
            { "StaggerRecoveryBaseMultiplier", status_.staggerRecoveryBaseMultiplier },
            { "BrokenActionName", status_.brokenActionName },
            { "Broken", status_.broken },
            });
    }
}

// JSONから部位設定を復元し、古いキー名も一部Fallbackとして扱う。
void DragonBodyPartManagerComponent::Deserialize(const nlohmann::json& json_)
{
    debugLogEnabled = json_.value("DebugLogEnabled", true);
    reactionEnabled = json_.value("ReactionEnabled", true);
    reactionCooldownTime = (std::max)(json_.value("ReactionCooldownTime", 2.0f), 0.0f);
    reactionCooldownTimer = (std::max)(json_.value("ReactionCooldownTimer", 0.0f), 0.0f);
    reactionActionName = json_.value(
        "ReactionActionName",
        std::string("DragonAI_DamageReaction"));
    commonBrokenActionName = json_.value(
        "CommonBrokenActionName",
        std::string("DragonBodyPart_Broken"));
    bodyParts.clear();

    if (json_.contains("BodyParts") && json_["BodyParts"].is_array())
    {
        for (const nlohmann::json& bodyPartJson_ : json_["BodyParts"])
        {
            DragonBodyPartStatus status_;
            status_.bodyPart = ParseDragonBodyPart(
                bodyPartJson_.value("BodyPart", std::string("BODY")));
            status_.maxDurability = (std::max)(
                bodyPartJson_.value("MaxDurability", 100.0f),
                1.0f);
            status_.currentDurability = (std::clamp)(
                bodyPartJson_.value("CurrentDurability", status_.maxDurability),
                0.0f,
                status_.maxDurability);
            status_.healthDamageRate = (std::max)(
                bodyPartJson_.value("HealthDamageRate", 1.0f),
                0.0f);
            status_.durabilityDamageRate = (std::max)(
                bodyPartJson_.value("DurabilityDamageRate", 1.0f),
                0.0f);
            status_.brokenHealthDamageRate = (std::max)(
                bodyPartJson_.value("BrokenHealthDamageRate", 1.25f),
                0.0f);
            status_.baseMaxStagger = (std::max)(
                bodyPartJson_.value(
                    "BaseMaxStagger",
                    bodyPartJson_.value(
                        "MaxStagger",
                        bodyPartJson_.value("ReactionDamageThreshold", 45.0f))),
                0.0f);
            status_.currentMaxStagger = (std::max)(
                bodyPartJson_.value(
                    "CurrentMaxStagger",
                    bodyPartJson_.value(
                        "MaxStagger",
                        bodyPartJson_.value("ReactionDamageThreshold", 45.0f))),
                0.0f);
            status_.currentStagger = (std::max)(
                bodyPartJson_.value(
                    "CurrentStagger",
                    bodyPartJson_.value("AccumulatedReactionDamage", 0.0f)),
                0.0f);
            status_.staggerDamageRate = (std::max)(
                bodyPartJson_.value("StaggerDamageRate", 1.0f),
                0.0f);
            status_.staggerRecoveryBaseMultiplier = (std::max)(
                bodyPartJson_.value(
                    "StaggerRecoveryBaseMultiplier",
                    1.0f + bodyPartJson_.value("StaggerMaxIncreaseRate", 0.25f)),
                1.0f);
            status_.brokenActionName = bodyPartJson_.value("BrokenActionName", std::string{});
            status_.broken = bodyPartJson_.value("Broken", status_.currentDurability <= 0.0f);
            bodyParts.push_back(status_);
        }
    }

    EnsureDefaultBodyParts();
}
