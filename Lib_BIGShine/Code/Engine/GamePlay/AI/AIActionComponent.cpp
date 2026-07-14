#include "AIActionComponent.h"

#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>

namespace
{
    const char* ToString(AI_ACTION_EXECUTE_TARGET target_)
    {
        switch (target_)
        {
        case AI_ACTION_EXECUTE_TARGET::OWNER: return "Owner";
        case AI_ACTION_EXECUTE_TARGET::GLOBAL: return "Global";
        default: return "Owner";
        }
    }

    AI_ACTION_EXECUTE_TARGET ExecuteTargetFromString(const std::string& value_)
    {
        if (value_ == "Global") return AI_ACTION_EXECUTE_TARGET::GLOBAL;
        return AI_ACTION_EXECUTE_TARGET::OWNER;
    }
}

void AIActionComponent::Start()
{
    EnsureBlackboard();
    RegisterActions();
}

void AIActionComponent::Gui()
{
    EnsureBlackboard();

    if (ImGui::Button(IM_U8("AIアクション追加##AIActionComponent_AddAiActionButton")))
    {
        AIActionDefinition definition_;
        definition_.aiActionName = "AI.Action" + std::to_string(actions.size());
        actions.push_back(definition_);
        selectedActionIndex = static_cast<int>(actions.size()) - 1;
        RegisterActions();
    }

    ImGui::SameLine();
    if (ImGui::Button(IM_U8("アクション登録##AIActionComponent_RegisterActionsButton")))
    {
        RegisterActions();
        statusText = "Registered";
    }

    if (!statusText.empty()) ImGui::Text("%s", statusText.c_str());

    int removeIndex_ = -1;
    for (int index_ = 0; index_ < static_cast<int>(actions.size()); ++index_)
    {
        AIActionDefinition& definition_ = actions[index_];
        ImGui::PushID(index_);

        const bool selected_ = selectedActionIndex == index_;
        if (ImGui::Selectable(definition_.aiActionName.c_str(), selected_))
        {
            selectedActionIndex = index_;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove"))
        {
            removeIndex_ = index_;
        }

        ImGui::PopID();
    }

    if (removeIndex_ >= 0)
    {
        actions.erase(actions.begin() + removeIndex_);
        selectedActionIndex = -1;
        RegisterActions();
    }

    if (selectedActionIndex < 0 || selectedActionIndex >= static_cast<int>(actions.size())) return;

    AIActionDefinition& definition_ = actions[selectedActionIndex];
    ImGui::SeparatorText(IM_U8("AIアクション##AIActionComponent_AiActionHeader"));
    ImGui::InputText(IM_U8("AIアクション名##AIActionComponent_AiActionNameInput"), &definition_.aiActionName);
    ImGui::InputText(IM_U8("実行アクション名##AIActionComponent_ExecuteActionNameInput"), &definition_.executeActionName);

    const char* targetNames_[] = { "Owner", "Global" };
    int target_ = static_cast<int>(definition_.executeTarget);
    if (ImGui::Combo(IM_U8("実行対象##AIActionComponent_ExecuteTargetCombo"), &target_, targetNames_, IM_ARRAYSIZE(targetNames_)))
    {
        definition_.executeTarget = static_cast<AI_ACTION_EXECUTE_TARGET>(target_);
    }

    ImGui::Checkbox(IM_U8("アクション終了を待つ##AIActionComponent_WaitForActionEndCheckbox"), &definition_.waitForActionEnd);
    ImGui::Checkbox(IM_U8("開始時にActionEndedをクリア##AIActionComponent_ClearActionEndedOnStartCheckbox"), &definition_.clearActionEndedOnStart);
    ImGui::Checkbox(IM_U8("IsActingを設定##AIActionComponent_SetIsActingCheckbox"), &definition_.setIsActing);

    ImGui::SeparatorText(IM_U8("ブラックボード##AIActionComponent_BlackboardHeader"));
    ImGui::InputText(IM_U8("カテゴリ##AIActionComponent_BlackboardCategoryInput"), &definition_.blackboardCategory);
    ImGui::InputText(IM_U8("CurrentActionキー##AIActionComponent_CurrentActionKeyInput"), &definition_.currentActionKey);
    ImGui::InputText(IM_U8("ActionEndedキー##AIActionComponent_ActionEndedKeyInput"), &definition_.actionEndedKey);
    ImGui::InputText(IM_U8("IsActingキー##AIActionComponent_IsActingKeyInput"), &definition_.isActingKey);

    ImGui::SeparatorText("Animation Graph");
    ImGui::InputText("Trigger", &definition_.animationTriggerName);
    ImGui::InputText("State", &definition_.animationStateName);
    ImGui::InputText("Anim Category", &definition_.animationBlackboardCategory);
    ImGui::InputText("Anim CurrentAction Key", &definition_.animationCurrentActionKey);
    ImGui::InputText("Anim ActionEnded Key", &definition_.animationActionEndedKey);
    ImGui::InputText("Anim IsActing Key", &definition_.animationIsActingKey);
    ImGui::InputText("Anim End Trigger", &definition_.animationActionEndTriggerName);
    ImGui::Checkbox("Write Anim Blackboard", &definition_.writeAnimationBlackboard);

    if (ImGui::Button(IM_U8("実行テスト##AIActionComponent_TestExecuteButton")))
    {
        statusText = ExecuteAIAction(definition_.aiActionName) ? "Executed" : "Execute failed";
    }
    ImGui::SameLine();
    if (ImGui::Button(IM_U8("終了通知##AIActionComponent_NotifyEndButton")))
    {
        NotifyActionEnd(definition_.blackboardCategory);
        statusText = "ActionEnd notified";
    }
}

void AIActionComponent::Serialize(nlohmann::json& json_) const
{
    json_["Actions"] = nlohmann::json::array();
    for (const AIActionDefinition& definition_ : actions)
    {
        json_["Actions"].push_back({
            { "AIActionName", definition_.aiActionName },
            { "ExecuteActionName", definition_.executeActionName },
            { "BlackboardCategory", definition_.blackboardCategory },
            { "CurrentActionKey", definition_.currentActionKey },
            { "ActionEndedKey", definition_.actionEndedKey },
            { "IsActingKey", definition_.isActingKey },
            { "AnimationTriggerName", definition_.animationTriggerName },
            { "AnimationStateName", definition_.animationStateName },
            { "AnimationBlackboardCategory", definition_.animationBlackboardCategory },
            { "AnimationCurrentActionKey", definition_.animationCurrentActionKey },
            { "AnimationActionEndedKey", definition_.animationActionEndedKey },
            { "AnimationIsActingKey", definition_.animationIsActingKey },
            { "AnimationActionEndTriggerName", definition_.animationActionEndTriggerName },
            { "WaitForActionEnd", definition_.waitForActionEnd },
            { "ClearActionEndedOnStart", definition_.clearActionEndedOnStart },
            { "SetIsActing", definition_.setIsActing },
            { "WriteAnimationBlackboard", definition_.writeAnimationBlackboard },
            { "ExecuteTarget", ToString(definition_.executeTarget) },
            });
    }
}

void AIActionComponent::Deserialize(const nlohmann::json& json_)
{
    actions.clear();

    if (json_.contains("Actions") && json_["Actions"].is_array())
    {
        for (const nlohmann::json& actionJson_ : json_["Actions"])
        {
            AIActionDefinition definition_;
            definition_.aiActionName = actionJson_.value("AIActionName", std::string("AI.Action"));
            definition_.executeActionName = actionJson_.value("ExecuteActionName", std::string{});
            definition_.blackboardCategory = actionJson_.value("BlackboardCategory", std::string(AIBlackboardKeys::CATEGORY_AI));
            definition_.currentActionKey = actionJson_.value("CurrentActionKey", std::string(AIBlackboardKeys::CURRENT_ACTION));
            definition_.actionEndedKey = actionJson_.value("ActionEndedKey", std::string(AIBlackboardKeys::ACTION_ENDED));
            definition_.isActingKey = actionJson_.value("IsActingKey", std::string(AIBlackboardKeys::IS_ACTING));
            definition_.animationTriggerName = actionJson_.value("AnimationTriggerName", std::string{});
            definition_.animationStateName = actionJson_.value("AnimationStateName", std::string{});
            definition_.animationBlackboardCategory = actionJson_.value("AnimationBlackboardCategory", std::string(AIBlackboardKeys::CATEGORY_ANIM));
            definition_.animationCurrentActionKey = actionJson_.value("AnimationCurrentActionKey", std::string(AIBlackboardKeys::CURRENT_ACTION));
            definition_.animationActionEndedKey = actionJson_.value("AnimationActionEndedKey", std::string(AIBlackboardKeys::ACTION_ENDED));
            definition_.animationIsActingKey = actionJson_.value("AnimationIsActingKey", std::string(AIBlackboardKeys::IS_ACTING));
            definition_.animationActionEndTriggerName = actionJson_.value("AnimationActionEndTriggerName", std::string{});
            definition_.waitForActionEnd = actionJson_.value("WaitForActionEnd", true);
            definition_.clearActionEndedOnStart = actionJson_.value("ClearActionEndedOnStart", true);
            definition_.setIsActing = actionJson_.value("SetIsActing", true);
            definition_.writeAnimationBlackboard = actionJson_.value("WriteAnimationBlackboard", true);
            definition_.executeTarget = ExecuteTargetFromString(actionJson_.value("ExecuteTarget", std::string("Owner")));
            actions.push_back(definition_);
        }
    }

    RegisterActions();
}

bool AIActionComponent::ExecuteAIAction(const std::string& aiActionName_)
{
    AIActionDefinition* definition_ = FindDefinition(aiActionName_);
    if (!definition_) return false;
    if (!definition_->executeActionName.empty() &&
        definition_->executeActionName == definition_->aiActionName)
    {
        return false;
    }

    EnsureBlackboard();
    if (blackboardComponent)
    {
        blackboardComponent->SetString(
            definition_->blackboardCategory,
            definition_->currentActionKey,
            definition_->aiActionName);

        if (definition_->clearActionEndedOnStart)
        {
            blackboardComponent->SetBool(
                definition_->blackboardCategory,
                definition_->actionEndedKey,
                false);
        }

        if (definition_->setIsActing)
        {
            blackboardComponent->SetBool(
                definition_->blackboardCategory,
                definition_->isActingKey,
                true);
        }
    }

    ApplyAnimationActionStart(*definition_);

    std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return false;

    if (!definition_->executeActionName.empty())
    {
        if (definition_->executeTarget == AI_ACTION_EXECUTE_TARGET::GLOBAL)
        {
            ActionRegistry::Instance().Call(definition_->executeActionName);
        }
        else
        {
            owner_->ExecuteAction(definition_->executeActionName);
        }
    }

    if (!definition_->waitForActionEnd)
    {
        NotifyActionEnd(definition_->blackboardCategory);
    }

    return true;
}

void AIActionComponent::NotifyActionEnd(const std::string& category_)
{
    EnsureBlackboard();
    if (!blackboardComponent) return;

    AIActionDefinition* currentDefinition_ = nullptr;
    for (AIActionDefinition& definition_ : actions)
    {
        if (definition_.blackboardCategory != category_) continue;

        std::string currentAction_;
        if (!blackboardComponent->GetString(category_, definition_.currentActionKey, currentAction_)) continue;
        if (currentAction_ != definition_.aiActionName) continue;

        currentDefinition_ = &definition_;
        break;
    }
    if (currentDefinition_)
    {
        ApplyAnimationActionEnd(*currentDefinition_);
    }

    blackboardComponent->SetBool(category_, AIBlackboardKeys::ACTION_ENDED, true);
    blackboardComponent->SetBool(category_, AIBlackboardKeys::IS_ACTING, false);

    for (const AIActionDefinition& definition_ : actions)
    {
        if (definition_.blackboardCategory != category_) continue;
        blackboardComponent->SetBool(definition_.blackboardCategory, definition_.actionEndedKey, true);
        blackboardComponent->SetBool(definition_.blackboardCategory, definition_.isActingKey, false);
    }
}

bool AIActionComponent::IsActionEnded(const std::string& category_) const
{
    if (!blackboardComponent) return false;

    bool ended_ = false;
    return blackboardComponent->GetBool(category_, AIBlackboardKeys::ACTION_ENDED, ended_) && ended_;
}

void AIActionComponent::EnsureBlackboard()
{
    std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    auto blackboard_ = owner_->GetComponent<BlackboardComponent>();
    if (!blackboard_)
    {
        blackboard_ = owner_->AddComponent<BlackboardComponent>();
    }
    blackboardComponent = blackboard_.get();
}

void AIActionComponent::RegisterActions()
{
    std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    for (const std::string& actionName_ : registeredActionNames)
    {
        owner_->GetLocalRegistry().RemoveAction(AIBlackboardKeys::CATEGORY_AI, actionName_);
    }
    registeredActionNames.clear();

    for (const AIActionDefinition& definition_ : actions)
    {
        RegisterBridgeAction(definition_);
    }

    owner_->RegisterAction(AIBlackboardKeys::CATEGORY_AI, "AIActionEnd", [this]()
        {
            NotifyActionEnd(AIBlackboardKeys::CATEGORY_AI);
        });
    owner_->RegisterAction(AIBlackboardKeys::CATEGORY_AI, "NotifyAIActionEnd", [this]()
        {
            NotifyActionEnd(AIBlackboardKeys::CATEGORY_AI);
        });
    owner_->RegisterAction(AIBlackboardKeys::CATEGORY_AI, "AI.NotifyActionEnd", [this]()
        {
            NotifyActionEnd(AIBlackboardKeys::CATEGORY_AI);
        });
    registeredActionNames.push_back(owner_->BuildActionName("AIActionEnd"));
    registeredActionNames.push_back(owner_->BuildActionName("NotifyAIActionEnd"));
    registeredActionNames.push_back(owner_->BuildActionName("AI.NotifyActionEnd"));
}

void AIActionComponent::RegisterBridgeAction(const AIActionDefinition& definition_)
{
    std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || definition_.aiActionName.empty()) return;

    const std::string actionName_ = definition_.aiActionName;
    owner_->RegisterAction(AIBlackboardKeys::CATEGORY_AI, actionName_, [this, actionName_]()
        {
            ExecuteAIAction(actionName_);
        });
    registeredActionNames.push_back(owner_->BuildActionName(actionName_));
}

void AIActionComponent::ApplyAnimationActionStart(const AIActionDefinition& definition_)
{
    std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    std::shared_ptr<AnimationControllerComponent> animation_ = owner_->GetComponent<AnimationControllerComponent>();
    if (!animation_) return;

    const std::string previousCategory_ = animation_->GetBlackboardCategory();
    animation_->SetBlackboardCategory(definition_.animationBlackboardCategory);

    if (definition_.writeAnimationBlackboard)
    {
        animation_->SetParam(definition_.animationCurrentActionKey, definition_.aiActionName);
        animation_->SetParam(definition_.animationActionEndedKey, false);
        animation_->SetParam(definition_.animationIsActingKey, true);
    }
    if (!definition_.animationTriggerName.empty())
    {
        animation_->SetTrigger(definition_.animationTriggerName);
    }
    if (!definition_.animationStateName.empty())
    {
        animation_->ChangeState(definition_.animationStateName);
    }

    animation_->SetBlackboardCategory(previousCategory_);
}

void AIActionComponent::ApplyAnimationActionEnd(const AIActionDefinition& definition_)
{
    std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    std::shared_ptr<AnimationControllerComponent> animation_ = owner_->GetComponent<AnimationControllerComponent>();
    if (!animation_) return;

    const std::string previousCategory_ = animation_->GetBlackboardCategory();
    animation_->SetBlackboardCategory(definition_.animationBlackboardCategory);

    if (definition_.writeAnimationBlackboard)
    {
        animation_->SetParam(definition_.animationActionEndedKey, true);
        animation_->SetParam(definition_.animationIsActingKey, false);
    }
    if (!definition_.animationActionEndTriggerName.empty())
    {
        animation_->SetTrigger(definition_.animationActionEndTriggerName);
    }

    animation_->SetBlackboardCategory(previousCategory_);
}

AIActionDefinition* AIActionComponent::FindDefinition(const std::string& aiActionName_)
{
    for (AIActionDefinition& definition_ : actions)
    {
        if (definition_.aiActionName == aiActionName_) return &definition_;
    }
    return nullptr;
}

const AIActionDefinition* AIActionComponent::FindDefinition(const std::string& aiActionName_) const
{
    for (const AIActionDefinition& definition_ : actions)
    {
        if (definition_.aiActionName == aiActionName_) return &definition_;
    }
    return nullptr;
}

REGISTER_COMPONENT(AIActionComponent, "AIActionComponent")
