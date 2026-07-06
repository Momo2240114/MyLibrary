#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

enum class JUST_DODGE_REACTION_ACTION_SOURCE
{
    SELF_LOCAL,
    GLOBAL
};

class ActionRegistry;


class JustDodgeReactionComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static const std::string name_ = "JustDodgeReactionComponent";
        return name_;
    }

    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    void ExecuteReaction();

private:
    ActionRegistry* GetSelectedRegistry();
    const ActionRegistry* GetSelectedRegistry() const;

private:
    JUST_DODGE_REACTION_ACTION_SOURCE selectedActionSource = JUST_DODGE_REACTION_ACTION_SOURCE::GLOBAL;

    std::string selectedCategory = "Sequence";
    std::string selectedActionName;
};