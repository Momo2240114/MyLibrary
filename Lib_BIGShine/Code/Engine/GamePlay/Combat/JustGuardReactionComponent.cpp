#include "PrecompiledHeader.h"

#include "JustGuardReactionComponent.h"

#include "Engine/Common/DebugLog.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include <imgui_stdlib.h>

#include <algorithm>
#include <imgui.h>
#include <vector>

ActionRegistry* JustGuardReactionComponent::GetSelectedRegistry()
{
    // GUI設定に応じて、所有者ローカルかSceneグローバルのActionRegistryを選ぶ。
    switch (selectedActionSource)
    {
    case JUST_GUARD_REACTION_ACTION_SOURCE::SELF_LOCAL:
    {
        std::shared_ptr<GameObject> owner_ = GetGameObject();
        if (!owner_) return nullptr;

        return &owner_->localRegistry;
    }
    case JUST_GUARD_REACTION_ACTION_SOURCE::GLOBAL:
    default:
        return &ActionRegistry::Instance();
    }
}

const ActionRegistry* JustGuardReactionComponent::GetSelectedRegistry() const
{
    switch (selectedActionSource)
    {
    case JUST_GUARD_REACTION_ACTION_SOURCE::SELF_LOCAL:
    {
        std::shared_ptr<GameObject> owner_ = gameObject.lock();
        if (!owner_) return nullptr;

        return &owner_->localRegistry;
    }
    case JUST_GUARD_REACTION_ACTION_SOURCE::GLOBAL:
    default:
        return &ActionRegistry::Instance();
    }
}

void JustGuardReactionComponent::ExecuteReaction()
{
    if (attackerReactionEnabled && (selectedActionName.empty() || selectedActionName == "---")) return;
    // JustGuardSystem側でBlackboard更新とOnJustGuardが済んだ後に追加反応を呼ぶ。
    BS_LOG_INFO(
        LOG_CATEGORY::GAME,
        "[JustGuard] Reaction ExecuteReaction entered. Action="
        + selectedActionName);
    if (selectedActionName.empty() || selectedActionName == "---")
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[JustGuard] Reaction skipped: action is empty.");
        return;
    }

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[JustGuard] Reaction skipped: owner missing.");
        return;
    }

    BS_LOG_SUCCESS(LOG_CATEGORY::GAME, "[JustGuard] Reaction executing action.");
    owner_->ExecuteAction(selectedActionName);
}

void JustGuardReactionComponent::Gui()
{
    ImGui::Checkbox("React When Attack Is Just Guarded", &attackerReactionEnabled);
    ImGui::InputText("Attacker Reaction Local Action", &attackerReactionActionName);
    // どのRegistryから反応Actionを選ぶかをEditorで指定する。
    ImGui::TextDisabled("Triggered after the existing OnJustGuard event succeeds.");

    const char* sourceNames_[] =
    {
        "Self Local",
        "Global"
    };

    int sourceIndex_ = static_cast<int>(selectedActionSource);
    if (ImGui::Combo("Action Source", &sourceIndex_, sourceNames_, IM_ARRAYSIZE(sourceNames_)))
    {
        selectedActionSource =
            static_cast<JUST_GUARD_REACTION_ACTION_SOURCE>(sourceIndex_);
        selectedCategory.clear();
        selectedActionName.clear();
    }

    ActionRegistry* registry_ = GetSelectedRegistry();
    if (!registry_)
    {
        ImGui::TextColored(
            ImVec4(1.0f, 0.35f, 0.3f, 1.0f),
            "Action Registry Not Found");
        return;
    }

    std::vector<std::string> categories_ = registry_->GetCategories();
    std::sort(categories_.begin(), categories_.end());

    const char* categoryPreview_ = selectedCategory.empty()
        ? "Select Category"
        : selectedCategory.c_str();

    if (ImGui::BeginCombo("Action Category", categoryPreview_))
    {
        for (const std::string& category_ : categories_)
        {
            const bool selected_ = category_ == selectedCategory;
            if (ImGui::Selectable(category_.c_str(), selected_))
            {
                selectedCategory = category_;
                selectedActionName.clear();
            }

            if (selected_)
            {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    std::vector<std::string> actionNames_;
    if (!selectedCategory.empty())
    {
        actionNames_ = registry_->GetActionNames(selectedCategory);
        std::sort(actionNames_.begin(), actionNames_.end());
    }

    const char* actionPreview_ = selectedActionName.empty()
        ? "Select Action"
        : selectedActionName.c_str();

    if (ImGui::BeginCombo("Reaction Action", actionPreview_))
    {
        for (const std::string& actionName_ : actionNames_)
        {
            if (actionName_ == "---") continue;

            const bool selected_ = actionName_ == selectedActionName;
            if (ImGui::Selectable(actionName_.c_str(), selected_))
            {
                selectedActionName = actionName_;
            }

            if (selected_)
            {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    const bool valid_ =
        !selectedCategory.empty() &&
        !selectedActionName.empty() &&
        selectedActionName != "---" &&
        registry_->HasActionInCategory(selectedCategory, selectedActionName);

    ImGui::TextColored(
        valid_ ? ImVec4(0.3f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.35f, 0.3f, 1.0f),
        valid_ ? "Action Ready" : "Action Not Registered");

    if (ImGui::Button("Test Reaction") && valid_)
    {
        ExecuteReaction();
    }
}

void JustGuardReactionComponent::Serialize(nlohmann::json& json_) const
{
    json_["SelectedActionSource"] = static_cast<int>(selectedActionSource);
    json_["SelectedCategory"] = selectedCategory;
    json_["SelectedActionName"] = selectedActionName;
    json_["AttackerReactionEnabled"] = attackerReactionEnabled;
    json_["AttackerReactionActionName"] = attackerReactionActionName;
}

void JustGuardReactionComponent::Deserialize(const nlohmann::json& json_)
{
    selectedActionSource = static_cast<JUST_GUARD_REACTION_ACTION_SOURCE>(
        json_.value(
            "SelectedActionSource",
            static_cast<int>(JUST_GUARD_REACTION_ACTION_SOURCE::GLOBAL)));

    selectedCategory = json_.value("SelectedCategory", std::string("Sequence"));
    selectedActionName = json_.value("SelectedActionName", std::string{});
    attackerReactionEnabled = json_.value("AttackerReactionEnabled", false);
    attackerReactionActionName = json_.value("AttackerReactionActionName", std::string{});
    lastReactedAttackInstanceId = 0u;
}

void JustGuardReactionComponent::RequestAttackerReaction(uint32_t attackInstanceId_, uint32_t hitGroupId_)
{
    BS_PROFILE_SCOPE("JustGuardReactionComponent::RequestAttackerReaction");
    if (!IsEnabled() || !attackerReactionEnabled || attackerReactionActionName.empty() ||
        (attackInstanceId_ != 0u && attackInstanceId_ == lastReactedAttackInstanceId)) return;
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    if (const auto health_ = owner_->GetComponent<HealthComponent>()) if (health_->IsDead()) return;
    auto action_ = owner_->GetLocalRegistry().GetAction(owner_->BuildActionName(attackerReactionActionName));
    if (!action_) action_ = owner_->GetLocalRegistry().GetAction(attackerReactionActionName);
    if (!action_) return;
    // 要求前に記録し、Actionからの再入や別HitBoxの通知で同じ反応を重ねない。
    lastReactedAttackInstanceId = attackInstanceId_;
    if (const auto blackboard_ = owner_->GetOrAddComponent<BlackboardComponent>())
    {
        blackboard_->SetInt("GuardReaction", "AttackInstanceId", static_cast<int>(attackInstanceId_));
        blackboard_->SetInt("GuardReaction", "HitGroupId", static_cast<int>(hitGroupId_));
        blackboard_->SetBool("GuardReaction", "AttackJustGuarded", true);
    }
    action_();
}
