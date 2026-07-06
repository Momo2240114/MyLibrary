#include "JustDodgeReactionComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"

#include <algorithm>
#include <imgui.h>
#include <vector>

ActionRegistry* JustDodgeReactionComponent::GetSelectedRegistry()
{
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
    if (selectedActionName.empty() || selectedActionName == "---") return;

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    owner_->ExecuteAction(selectedActionName);
}

void JustDodgeReactionComponent::Gui()
{
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
        selectedCategory = "Sequence";
        selectedActionName.clear();
    }
}