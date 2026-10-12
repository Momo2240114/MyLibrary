#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include <cstdint>

enum class JUST_GUARD_REACTION_ACTION_SOURCE
{
    // 所有GameObjectのLocalRegistryからActionを探す。
    SELF_LOCAL,

    // Scene共通のActionRegistryからActionを探す。
    GLOBAL
};

class ActionRegistry;

// JustGuard成功後に、Editorで選択したActionを実行するComponent。
class JustGuardReactionComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static const std::string name_ = "JustGuardReactionComponent";
        return name_;
    }

    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    // JustGuard成功時に、選択されたActionを所有GameObjectから実行する。
    void ExecuteReaction();
    // 弾かれた攻撃の所有者へ、設定済みのLocal Actionによる中断要求を渡す。
    void RequestAttackerReaction(uint32_t attackInstanceId_, uint32_t hitGroupId_);

private:
    // GUIで選択中の参照先に応じたActionRegistryを返す。
    ActionRegistry* GetSelectedRegistry();
    const ActionRegistry* GetSelectedRegistry() const;

private:
    // 反応ActionをLocal/Globalどちらから選ぶか。
    JUST_GUARD_REACTION_ACTION_SOURCE selectedActionSource =
        JUST_GUARD_REACTION_ACTION_SOURCE::GLOBAL;

    // 実行するActionのカテゴリと名前。
    std::string selectedCategory = "Sequence";
    std::string selectedActionName;
    bool attackerReactionEnabled = false; // 未設定の敵は攻撃を維持し、対象側で反応を有効化する。
    std::string attackerReactionActionName;
    uint32_t lastReactedAttackInstanceId = 0u; // HitGroupが異なる複数HitBoxでも同じ攻撃には一度だけ反応する。
};
