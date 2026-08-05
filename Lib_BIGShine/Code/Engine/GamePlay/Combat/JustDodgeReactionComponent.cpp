#include "PrecompiledHeader.h"

#include "JustDodgeReactionComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Common/DebugLog.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"

#include <algorithm>
#include <imgui.h>
#include <vector>

ActionRegistry* JustDodgeReactionComponent::GetSelectedRegistry()
{
    // GUI設定に応じて、所有者ローカルかSceneグローバルのActionRegistryを選ぶ。
    switch (selectedActionSource)
    {
    case JUST_DODGE_REACTION_ACTION_SOURCE::SELF_LOCAL:
    {
        std::shared_ptr<GameObject> owner_ = GetGameObject();
        if (!owner_) return nullptr;

        return &owner_->localRegistry;
    }
    case JUST_DODGE_REACTION_ACTION_SOURCE::GLOBAL:
    default:
        return &ActionRegistry::Instance();
    }
}

const ActionRegistry* JustDodgeReactionComponent::GetSelectedRegistry() const
{
    switch (selectedActionSource)
    {
    case JUST_DODGE_REACTION_ACTION_SOURCE::SELF_LOCAL:
    {
        std::shared_ptr<GameObject> owner_ = gameObject.lock();
        if (!owner_) return nullptr;

        return &owner_->localRegistry;
    }
    case JUST_DODGE_REACTION_ACTION_SOURCE::GLOBAL:
    default:
        return &ActionRegistry::Instance();
    }
}

void JustDodgeReactionComponent::ExecuteReaction()
{
    // JustDodgeSystem側でBlackboard更新とOnJustDodgeが済んだ後に呼ばれる追加反応。
    BS_LOG_INFO(
        LOG_CATEGORY::GAME,
        "[JustDodge] Reaction ExecuteReaction entered. Action="
        + selectedActionName);
    if (selectedActionName.empty() || selectedActionName == "---")
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[JustDodge] Reaction skipped: action is empty.");
        return;
    }

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[JustDodge] Reaction skipped: owner missing.");
        return;
    }

    // ExecuteActionはLocal/Global両方を探せるため、実行時はAction名だけで呼ぶ。
    BS_LOG_SUCCESS(LOG_CATEGORY::GAME, "[JustDodge] Reaction executing action.");
    owner_->ExecuteAction(selectedActionName);
}

void JustDodgeReactionComponent::Gui()
{
    // どのRegistryから反応Actionを選ぶかをEditorで指定する。
    ImGui::TextDisabled("Triggered after the existing OnJustDodge event succeeds.");

    const char* sourceNames_[] =
    {
        "Self Local",
        "Global"
    };

    int sourceIndex_ = static_cast<int>(selectedActionSource);
    if (ImGui::Combo("Action Source", &sourceIndex_, sourceNames_, IM_ARRAYSIZE(sourceNames_)))
    {
        selectedActionSource = static_cast<JUST_DODGE_REACTION_ACTION_SOURCE>(sourceIndex_);
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
    // 選択肢を安定表示するため、カテゴリとAction名はソートする。
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

    // 登録済みActionかどうかを即時表示し、名前変更後の取りこぼしに気づきやすくする。
    ImGui::TextColored(
        valid_ ? ImVec4(0.3f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.35f, 0.3f, 1.0f),
        valid_ ? "Action Ready" : "Action Not Registered");

    if (ImGui::Button("Test Reaction") && valid_)
    {
        ExecuteReaction();
    }
}

void JustDodgeReactionComponent::Serialize(nlohmann::json& json_) const
{
    json_["SelectedActionSource"] = static_cast<int>(selectedActionSource);
    json_["SelectedCategory"] = selectedCategory;
    json_["SelectedActionName"] = selectedActionName;
}

void JustDodgeReactionComponent::Deserialize(const nlohmann::json& json_)
{
    selectedActionSource = static_cast<JUST_DODGE_REACTION_ACTION_SOURCE>(
        json_.value(
            "SelectedActionSource",
            static_cast<int>(JUST_DODGE_REACTION_ACTION_SOURCE::GLOBAL)));

    selectedCategory = json_.value("SelectedCategory", std::string("Sequence"));
    selectedActionName = json_.value("SelectedActionName", std::string{});

    if (selectedActionName == "JustDodgeEventPool_Play")
    {
        // 古い専用Action名は廃止済みなので、読み込み時に未選択へ戻す。
        selectedCategory = "Sequence";
        selectedActionName.clear();
    }
}
