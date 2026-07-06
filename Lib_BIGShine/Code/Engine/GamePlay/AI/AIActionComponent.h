#pragma once

#include "AIBlackboardKeys.h"
#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <string>
#include <vector>

class BlackboardComponent;

enum class AI_ACTION_EXECUTE_TARGET
{
    OWNER,
    GLOBAL,
};

struct AIActionDefinition
{
    std::string aiActionName = "AI.Action";
    std::string executeActionName;
    std::string blackboardCategory = AIBlackboardKeys::CATEGORY_AI;
    std::string currentActionKey = AIBlackboardKeys::CURRENT_ACTION;
    std::string actionEndedKey = AIBlackboardKeys::ACTION_ENDED;
    std::string isActingKey = AIBlackboardKeys::IS_ACTING;
    std::string animationTriggerName;
    std::string animationStateName;
    std::string animationBlackboardCategory = AIBlackboardKeys::CATEGORY_ANIM;
    std::string animationCurrentActionKey = AIBlackboardKeys::CURRENT_ACTION;
    std::string animationActionEndedKey = AIBlackboardKeys::ACTION_ENDED;
    std::string animationIsActingKey = AIBlackboardKeys::IS_ACTING;
    std::string animationActionEndTriggerName;
    bool waitForActionEnd = true;
    bool clearActionEndedOnStart = true;
    bool setIsActing = true;
    bool writeAnimationBlackboard = true;
    AI_ACTION_EXECUTE_TARGET executeTarget = AI_ACTION_EXECUTE_TARGET::OWNER;
};

class AIActionComponent : public Component
{
public:
    AIActionComponent() = default;

    const std::string& GetName() const override
    {
        static std::string name_ = "AIActionComponent";
        return name_;
    }

    void Start() override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    bool ExecuteAIAction(const std::string& aiActionName_);
    void NotifyActionEnd(const std::string& category_ = AIBlackboardKeys::CATEGORY_AI);
    bool IsActionEnded(const std::string& category_ = AIBlackboardKeys::CATEGORY_AI) const;

private:
    void EnsureBlackboard();
    void RegisterActions();
    void RegisterBridgeAction(const AIActionDefinition& definition_);
    void ApplyAnimationActionStart(const AIActionDefinition& definition_);
    void ApplyAnimationActionEnd(const AIActionDefinition& definition_);
    AIActionDefinition* FindDefinition(const std::string& aiActionName_);
    const AIActionDefinition* FindDefinition(const std::string& aiActionName_) const;

private:
    std::vector<AIActionDefinition> actions;
    BlackboardComponent* blackboardComponent = nullptr;
    std::vector<std::string> registeredActionNames;
    std::string statusText;
    int selectedActionIndex = -1;
};
