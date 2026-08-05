#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

enum class JUST_DODGE_REACTION_ACTION_SOURCE
{
    // 所有GameObjectのLocalRegistryからActionを探す。
    SELF_LOCAL,

    // Scene共通のActionRegistryからActionを探す。
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

    // JustDodge成功時に、選択されたActionRegistryからActionを実行する。
    void ExecuteReaction();

private:
    // GUIで選択中の参照先に応じたActionRegistryを返す。
    ActionRegistry* GetSelectedRegistry();
    const ActionRegistry* GetSelectedRegistry() const;

private:
    // 反応ActionをLocal/Globalどちらから呼ぶか。
    JUST_DODGE_REACTION_ACTION_SOURCE selectedActionSource = JUST_DODGE_REACTION_ACTION_SOURCE::GLOBAL;

    // 実行するActionのカテゴリと名前。
    std::string selectedCategory = "Sequence";
    std::string selectedActionName;
};
