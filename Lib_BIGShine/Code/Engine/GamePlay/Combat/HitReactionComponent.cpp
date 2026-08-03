#include "PrecompiledHeader.h"

#include "HitReactionComponent.h"

#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/System/TimeManager.h"
#include "Engine/Physics/ColliderComponent.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>

namespace
{
    const char* GetHitReactionKindName(HIT_REACTION_KIND kind_)
    {
        switch (kind_)
        {
        case HIT_REACTION_KIND::GROUND_LIGHT: return "Ground Light";
        case HIT_REACTION_KIND::GROUND_MEDIUM: return "Ground Medium";
        case HIT_REACTION_KIND::GROUND_HEAVY: return "Ground Heavy";
        case HIT_REACTION_KIND::AIR_MEDIUM: return "Air Medium";
        case HIT_REACTION_KIND::AIR_HEAVY: return "Air Heavy";
        default: return "Unknown";
        }
    }

    void DrawActionNameList(const char* label_, std::vector<std::string>& actionNames_)
    {
        if (!ImGui::TreeNode(label_)) return;

        for (size_t index_ = 0; index_ < actionNames_.size(); ++index_)
        {
            ImGui::PushID(static_cast<int>(index_));
            char actionNameBuffer_[128]{};
            strncpy_s(actionNameBuffer_, actionNames_[index_].c_str(), sizeof(actionNameBuffer_));
            if (ImGui::InputText("Action", actionNameBuffer_, sizeof(actionNameBuffer_)))
            {
                actionNames_[index_] = actionNameBuffer_;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove"))
            {
                actionNames_.erase(actionNames_.begin() + index_);
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }

        if (ImGui::SmallButton("Add Action"))
        {
            actionNames_.push_back({});
        }

        ImGui::TreePop();
    }
}

void HitReactionComponent::Start()
{
    RegisterActions();
    currentHealth = (std::clamp)(currentHealth, 0.0f, maxHealth);
}

void HitReactionComponent::Update(float deltaTime_)
{
    if (overlayTimer <= 0.0f) return;

    overlayTimer -= deltaTime_;
    if (overlayTimer <= 0.0f)
    {
        overlayTimer = 0.0f;
        CallOwnerAction(overlayStopActionName);
    }
}

void HitReactionComponent::HandleHit()
{

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    // AttackSystemがBlackboardへ公開したHit情報を読み、ダメージ・ヒットストップ・吹き飛びへ分解する。
    const std::shared_ptr<BlackboardComponent> blackboard_ =
        owner_->GetComponent<BlackboardComponent>();
    if (!blackboard_) return;

    float damage_ = 0.0f;
    float hitStopDuration_ = 0.0f;
    float hitStopScale_ = 0.0f;
    float impactPower_ = 0.0f;
    float knockBackPower_ = 0.0f;
    float targetAirLiftPower_ = 0.0f;
    DirectX::SimpleMath::Vector2 hitDirection_{ 0.0f, 0.0f };
    blackboard_->GetFloat("Hit", "Damage", damage_);
    if (!blackboard_->GetFloat("Hit", "HitStopDuration", hitStopDuration_))
    {
        blackboard_->GetFloat("Hit", "HitStopTime", hitStopDuration_);
    }
    blackboard_->GetFloat("Hit", "HitStopScale", hitStopScale_);
    blackboard_->GetFloat("Hit", "ImpactPower", impactPower_);
    blackboard_->GetFloat("Hit", "KnockBackPower", knockBackPower_);
    blackboard_->GetFloat("Hit", "TargetAirLiftPower", targetAirLiftPower_);
    blackboard_->GetVector2("Hit", "Direction", hitDirection_);

    lastDamage = (std::max)(damage_, 0.0f);
    lastImpactPower = (std::max)(impactPower_, 0.0f);
    currentHealth = (std::max)(currentHealth - lastDamage, 0.0f);
    ++hitCount;
    blackboard_->SetFloat("Hit", "CurrentHealth", currentHealth);
    blackboard_->SetBool("Hit", "Dead", currentHealth <= 0.0f);
    blackboard_->SetFloat("Hit", "Armor", armor);

    const bool canReact_ = lastImpactPower > 0.0f && lastImpactPower > armor;
    blackboard_->SetBool("Hit", "ImpactReacted", canReact_);
    CallOwnerAction(damageActionName);
    CallOwnerActions(commonReactionActionNames);

    // HitStopはTimeManagerへ委譲し、対象Layerだけ短時間スローにする。
    if (useHitStop && hitStopDuration_ > 0.0f)
    {
        const std::shared_ptr<TimeManager> timeManager_ =
            owner_->GetManager().GetTimeManager();
        if (timeManager_)
        {
            const uint32_t layerMask_ = hitStopLayerMask == 0u
                ? static_cast<uint32_t>(TarGetLayerType::All)
                : hitStopLayerMask;
            timeManager_->TriggerHitStop(
                layerMask_,
                hitStopDuration_,
                hitStopScale_);
        }
    }

    float massScale_ = 1.0f;
    if (const std::shared_ptr<ColliderComponent> collider_ =
        owner_->GetComponent<ColliderComponent>())
    {
        massScale_ = 1.0f / (std::max)(collider_->GetMass(), 1.0f);
    }

    if (canReact_ && useKnockBack && knockBackPower_ > 0.0f)
    {
        // Armorを超えた衝撃だけ、BlackboardのHit方向を使って横方向のノックバックを入れる。
        const std::shared_ptr<CharacterMovementComponent> movement_ =
            owner_->GetComponent<CharacterMovementComponent>();
        if (movement_)
        {
            const float horizontalLength_ = std::sqrt(
                hitDirection_.x * hitDirection_.x +
                hitDirection_.y * hitDirection_.y);
            if (horizontalLength_ > 0.0001f)
            {
                const DirectX::XMFLOAT3 knockBackImpulse_
                {
                    hitDirection_.x / horizontalLength_ * knockBackPower_ * massScale_,
                    0.0f,
                    hitDirection_.y / horizontalLength_ * knockBackPower_ * massScale_
                };
                movement_->AddImpulse(knockBackImpulse_);
            }
        }
    }

    if (canReact_ && targetAirLiftPower_ > 0.0f)
    {
        const float airLiftImpulse_ = targetAirLiftPower_ * massScale_;
        if (const std::shared_ptr<CharacterMovementComponent> movement_ =
            owner_->GetComponent<CharacterMovementComponent>())
        {
            movement_->ResetVerticalVelocity();
            movement_->AddImpulse({ 0.0f, airLiftImpulse_, 0.0f });
        }
        else if (const std::shared_ptr<ColliderComponent> collider_ =
            owner_->GetComponent<ColliderComponent>())
        {
            collider_->velocity.y = airLiftImpulse_;
        }
    }

    if (canReact_)
    {
        // 地上/空中とImpactPowerのしきい値から、再生するリアクションActionを選ぶ。
        CallOwnerActions(GetReactionActions(DecideReactionKind(*blackboard_, canReact_)));
    }
    CallOwnerAction(overlayStartActionName);
    overlayTimer = (std::max)(overlayDuration, 0.0f);
    if (overlayTimer <= 0.0f) CallOwnerAction(overlayStopActionName);
}

void HitReactionComponent::ResetHealth()
{
    currentHealth = maxHealth;
    lastDamage = 0.0f;
    lastImpactPower = 0.0f;
    hitCount = 0;
}

void HitReactionComponent::RegisterActions()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    // AttackSystemから最終的にOnHitで呼ばれる入口。Healthとは別にリアクションを担当する。
    std::weak_ptr<HitReactionComponent> weakReaction_;
    for (const std::shared_ptr<Component>& component_ : owner_->components)
    {
        const std::shared_ptr<HitReactionComponent> reaction_ =
            std::dynamic_pointer_cast<HitReactionComponent>(component_);
        if (reaction_.get() == this)
        {
            weakReaction_ = reaction_;
            break;
        }
    }

    owner_->RegisterAction("Combat", "OnHit", [weakReaction_]()
    {
        if (const std::shared_ptr<HitReactionComponent> reaction_ = weakReaction_.lock())
        {
            reaction_->HandleHit();
        }
    });
}

void HitReactionComponent::CallOwnerAction(const std::string& actionName_)
{
    if (actionName_.empty() || actionName_ == "OnHit") return;

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (owner_) owner_->ExecuteAction(actionName_);
}

void HitReactionComponent::CallOwnerActions(const std::vector<std::string>& actionNames_)
{
    for (const std::string& actionName_ : actionNames_)
    {
        CallOwnerAction(actionName_);
    }
}

HIT_REACTION_KIND HitReactionComponent::DecideReactionKind(
    const BlackboardComponent& blackboard_,
    bool canReact_) const
{
    if (!canReact_)
    {
        return HIT_REACTION_KIND::GROUND_LIGHT;
    }

    // 接地状態と衝撃値で、軽/中/重/空中リアクションを切り替える。
    bool isGrounded_ = true;
    if (!blackboard_.GetBool("Player", "IsGrounded", isGrounded_))
    {
        blackboard_.GetBool("Anim", "IsGrounded", isGrounded_);
    }

    if (!isGrounded_)
    {
        return lastImpactPower >= airHeavyImpactThreshold
            ? HIT_REACTION_KIND::AIR_HEAVY
            : HIT_REACTION_KIND::AIR_MEDIUM;
    }

    if (lastImpactPower < mediumImpactThreshold)
    {
        return HIT_REACTION_KIND::GROUND_LIGHT;
    }

    return lastImpactPower >= heavyImpactThreshold
        ? HIT_REACTION_KIND::GROUND_HEAVY
        : HIT_REACTION_KIND::GROUND_MEDIUM;
}

std::vector<std::string>& HitReactionComponent::GetReactionActions(HIT_REACTION_KIND kind_)
{
    return reactionActionNamesByKind[static_cast<size_t>(kind_)];
}

const std::vector<std::string>& HitReactionComponent::GetReactionActions(HIT_REACTION_KIND kind_) const
{
    return reactionActionNamesByKind[static_cast<size_t>(kind_)];
}

void HitReactionComponent::SetDefaultReactionActionName(const std::string& actionName_)
{
    GetReactionActions(HIT_REACTION_KIND::GROUND_LIGHT).clear();

    const HIT_REACTION_KIND reactionKinds_[] =
    {
        HIT_REACTION_KIND::GROUND_MEDIUM,
        HIT_REACTION_KIND::GROUND_HEAVY,
        HIT_REACTION_KIND::AIR_MEDIUM,
        HIT_REACTION_KIND::AIR_HEAVY
    };

    for (const HIT_REACTION_KIND kind_ : reactionKinds_)
    {
        std::vector<std::string>& actionNames_ = GetReactionActions(kind_);
        actionNames_.clear();
        if (!actionName_.empty())
        {
            actionNames_.push_back(actionName_);
        }
    }
}

void HitReactionComponent::SetReactionImpactThresholds(
    float mediumImpactThreshold_,
    float heavyImpactThreshold_,
    float airHeavyImpactThreshold_)
{
    mediumImpactThreshold = (std::max)(mediumImpactThreshold_, 0.0f);
    heavyImpactThreshold = (std::max)(heavyImpactThreshold_, mediumImpactThreshold);
    airHeavyImpactThreshold = (std::max)(airHeavyImpactThreshold_, 0.0f);
}

void HitReactionComponent::AddCommonReactionAction(const std::string& actionName_)
{
    if (actionName_.empty()) return;
    if (std::find(commonReactionActionNames.begin(), commonReactionActionNames.end(), actionName_) ==
        commonReactionActionNames.end())
    {
        commonReactionActionNames.push_back(actionName_);
    }
}

void HitReactionComponent::AddReactionAction(HIT_REACTION_KIND kind_, const std::string& actionName_)
{
    if (actionName_.empty()) return;
    std::vector<std::string>& actionNames_ = GetReactionActions(kind_);
    if (std::find(actionNames_.begin(), actionNames_.end(), actionName_) == actionNames_.end())
    {
        actionNames_.push_back(actionName_);
    }
}

void HitReactionComponent::Gui()
{
    ImGui::Text("Health: %.1f / %.1f", currentHealth, maxHealth);
    ImGui::Text("Last Damage: %.1f  Last Impact: %.1f  Hit Count: %d", lastDamage, lastImpactPower, hitCount);
    if (ImGui::DragFloat("Max Health", &maxHealth, 1.0f, 1.0f, 999999.0f))
    {
        currentHealth = (std::min)(currentHealth, maxHealth);
    }
    if (ImGui::Button("Test OnHit")) HandleHit();
    ImGui::SameLine();
    if (ImGui::Button("Reset Health")) ResetHealth();

    ImGui::SeparatorText("Hit Reaction");
    ImGui::DragFloat("Armor", &armor, 0.1f, 0.0f, 99999.0f);
    ImGui::DragFloat("Ground Medium Impact", &mediumImpactThreshold, 0.1f, 0.0f, 99999.0f);
    ImGui::DragFloat("Ground Heavy Impact", &heavyImpactThreshold, 0.1f, 0.0f, 99999.0f);
    ImGui::DragFloat("Air Heavy Impact", &airHeavyImpactThreshold, 0.1f, 0.0f, 99999.0f);
    ImGui::Checkbox("Use Hit Stop", &useHitStop);
    ImGui::InputScalar("Hit Stop Layer Mask", ImGuiDataType_U32, &hitStopLayerMask);
    ImGui::Checkbox("Use Knock Back", &useKnockBack);

    char reactionActionBuffer_[128];
    strncpy_s(reactionActionBuffer_, reactionActionName.c_str(), sizeof(reactionActionBuffer_));
    if (ImGui::InputText("Reaction Action", reactionActionBuffer_, sizeof(reactionActionBuffer_)))
    {
        reactionActionName = reactionActionBuffer_;
        SetDefaultReactionActionName(reactionActionName);
    }
    char damageActionBuffer_[128];
    strncpy_s(damageActionBuffer_, damageActionName.c_str(), sizeof(damageActionBuffer_));
    if (ImGui::InputText("Damage Action", damageActionBuffer_, sizeof(damageActionBuffer_)))
    {
        damageActionName = damageActionBuffer_;
    }
    DrawActionNameList("Common Reaction Actions", commonReactionActionNames);
    for (int kindIndex_ = 0; kindIndex_ < static_cast<int>(HIT_REACTION_KIND::COUNT); ++kindIndex_)
    {
        const HIT_REACTION_KIND kind_ = static_cast<HIT_REACTION_KIND>(kindIndex_);
        DrawActionNameList(GetHitReactionKindName(kind_), GetReactionActions(kind_));
    }
    char overlayStartBuffer_[128];
    strncpy_s(overlayStartBuffer_, overlayStartActionName.c_str(), sizeof(overlayStartBuffer_));
    if (ImGui::InputText("Overlay Start Action", overlayStartBuffer_, sizeof(overlayStartBuffer_)))
    {
        overlayStartActionName = overlayStartBuffer_;
    }
    char overlayStopBuffer_[128];
    strncpy_s(overlayStopBuffer_, overlayStopActionName.c_str(), sizeof(overlayStopBuffer_));
    if (ImGui::InputText("Overlay Stop Action", overlayStopBuffer_, sizeof(overlayStopBuffer_)))
    {
        overlayStopActionName = overlayStopBuffer_;
    }
    ImGui::DragFloat("Overlay Duration", &overlayDuration, 0.01f, 0.0f, 10.0f);
    ImGui::TextDisabled("Registered Action: Combat / OnHit");
}

void HitReactionComponent::Serialize(nlohmann::json& json_) const
{
    json_["MaxHealth"] = maxHealth;
    json_["CurrentHealth"] = currentHealth;
    json_["Armor"] = armor;
    json_["MediumImpactThreshold"] = mediumImpactThreshold;
    json_["HeavyImpactThreshold"] = heavyImpactThreshold;
    json_["AirHeavyImpactThreshold"] = airHeavyImpactThreshold;
    json_["UseHitStop"] = useHitStop;
    json_["HitStopLayerMask"] = hitStopLayerMask;
    json_["UseKnockBack"] = useKnockBack;
    json_["DamageActionName"] = damageActionName;
    json_["ReactionActionName"] = reactionActionName;
    json_["CommonReactionActionNames"] = commonReactionActionNames;
    json_["ReactionActionNamesByKind"] =
    {
        { "GroundLight", GetReactionActions(HIT_REACTION_KIND::GROUND_LIGHT) },
        { "GroundMedium", GetReactionActions(HIT_REACTION_KIND::GROUND_MEDIUM) },
        { "GroundHeavy", GetReactionActions(HIT_REACTION_KIND::GROUND_HEAVY) },
        { "AirMedium", GetReactionActions(HIT_REACTION_KIND::AIR_MEDIUM) },
        { "AirHeavy", GetReactionActions(HIT_REACTION_KIND::AIR_HEAVY) }
    };
    json_["OverlayStartActionName"] = overlayStartActionName;
    json_["OverlayStopActionName"] = overlayStopActionName;
    json_["OverlayDuration"] = overlayDuration;
}

void HitReactionComponent::Deserialize(const nlohmann::json& json_)
{
    maxHealth = (std::max)(json_.value("MaxHealth", 100.0f), 1.0f);
    currentHealth = (std::clamp)(
        json_.value("CurrentHealth", maxHealth),
        0.0f,
        maxHealth);
    useHitStop = json_.value("UseHitStop", true);
    armor = (std::max)(json_.value("Armor", 0.0f), 0.0f);
    mediumImpactThreshold = (std::max)(json_.value("MediumImpactThreshold", mediumImpactThreshold), 0.0f);
    heavyImpactThreshold = (std::max)(json_.value("HeavyImpactThreshold", heavyImpactThreshold), mediumImpactThreshold);
    airHeavyImpactThreshold = (std::max)(json_.value("AirHeavyImpactThreshold", airHeavyImpactThreshold), 0.0f);
    hitStopLayerMask = json_.value("HitStopLayerMask", 3u);
    useKnockBack = json_.value("UseKnockBack", true);
    damageActionName = json_.value("DamageActionName", std::string{});
    reactionActionName = json_.value("ReactionActionName", std::string{});
    commonReactionActionNames = json_.value("CommonReactionActionNames", std::vector<std::string>{});
    if (json_.contains("ReactionActionNamesByKind") && json_["ReactionActionNamesByKind"].is_object())
    {
        const auto& reactionJson_ = json_["ReactionActionNamesByKind"];
        GetReactionActions(HIT_REACTION_KIND::GROUND_LIGHT) =
            reactionJson_.value("GroundLight", std::vector<std::string>{});
        GetReactionActions(HIT_REACTION_KIND::GROUND_MEDIUM) =
            reactionJson_.value("GroundMedium", std::vector<std::string>{});
        GetReactionActions(HIT_REACTION_KIND::GROUND_HEAVY) =
            reactionJson_.value("GroundHeavy", std::vector<std::string>{});
        GetReactionActions(HIT_REACTION_KIND::AIR_MEDIUM) =
            reactionJson_.value("AirMedium", std::vector<std::string>{});
        GetReactionActions(HIT_REACTION_KIND::AIR_HEAVY) =
            reactionJson_.value("AirHeavy", std::vector<std::string>{});
    }
    else if (!reactionActionName.empty())
    {
        SetDefaultReactionActionName(reactionActionName);
    }
    overlayStartActionName = json_.value(
        "OverlayStartActionName",
        std::string("MaterialOverlay_Start"));
    overlayStopActionName = json_.value(
        "OverlayStopActionName",
        std::string("MaterialOverlay_Stop"));
    overlayDuration = (std::max)(json_.value("OverlayDuration", 0.12f), 0.0f);
}
