#include "PrecompiledHeader.h"
#include "PlayerInputResolver.h"
#include "Engine/GamePlay/Input/KeyConfig.h"
#include "Engine/Debug/TracyProfiler.h"

int PlayerInputResolver::ResolveSkillSlot(const KeyConfig& keyConfig_,
    const std::vector<PlayerSkillInputBinding>& bindings_)
{
    BS_PROFILE_SCOPE("PlayerInputResolver::ResolveSkillSlot");
    const PlayerSkillInputBinding* selected_ = nullptr;
    for (const auto& binding_ : bindings_)
    {
        // Triggerを先に押してからModifierを追加しても、この条件では発動しない。
        if (!keyConfig_.ActionPressed(binding_.modifierAction) ||
            !keyConfig_.ActionPressedIn(binding_.triggerAction)) continue;
        if (!selected_ || binding_.priority > selected_->priority) selected_ = &binding_;
    }
    return selected_ ? selected_->slotIndex : -1;
}

bool PlayerInputResolver::IsActionConsumed(const KeyConfig& keyConfig_,
    const std::vector<PlayerSkillInputBinding>& bindings_, const std::string& action_)
{
    for (const auto& binding_ : bindings_)
        if (binding_.triggerAction == action_ && keyConfig_.ActionPressed(binding_.modifierAction)) return true;
    return false;
}

PLAYER_ACTION PlayerInputResolver::ResolveSkill(const KeyConfig& keyConfig_,
    const std::vector<PlayerSkillInputBinding>& bindings_)
{
    const int slotIndex_ = ResolveSkillSlot(keyConfig_, bindings_);
    return slotIndex_ >= 0 ? PlayerSkillLoadout::GetSlotAction(static_cast<std::size_t>(slotIndex_)) : PLAYER_ACTION::NONE;
}
