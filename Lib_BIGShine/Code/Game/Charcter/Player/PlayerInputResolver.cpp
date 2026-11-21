#include "PrecompiledHeader.h"
#include "PlayerInputResolver.h"
#include "Engine/GamePlay/Input/KeyConfig.h"
#include "Engine/Debug/TracyProfiler.h"

// 保持中の修飾入力と発動入力の押下が成立した候補から、優先度が最大の枠を選ぶ。
int PlayerInputResolver::ResolveSkillSlot(const KeyConfig& keyConfig_,
    const std::vector<PlayerSkillInputBinding>& bindings_)
{
    BS_PROFILE_SCOPE("PlayerInputResolver::ResolveSkillSlot");
    const PlayerSkillInputBinding* selected_ = nullptr;
    for (const auto& binding_ : bindings_)
    {
        // Triggerを先に押してからModifierを追加しても、この条件では発動しない。
        if (!keyConfig_.ActionPressed(binding_.modifierAction) ||
            !keyConfig_.ActionPressedIn(binding_.triggerAction))
        {
            continue;
        }
        // 同じ優先度なら先に登録した候補を維持し、入力表の並び順で選択を安定させる。
        if (!selected_ || binding_.priority > selected_->priority)
        {
            selected_ = &binding_;
        }
    }
    return selected_ ? selected_->slotIndex : -1;
}

// 修飾入力を保持している発動ボタンは通常行動へ流さず、複合入力との二重処理を防ぐ。
bool PlayerInputResolver::IsActionConsumed(const KeyConfig& keyConfig_,
    const std::vector<PlayerSkillInputBinding>& bindings_, const std::string& action_)
{
    for (const auto& binding_ : bindings_)
    {
        if (binding_.triggerAction == action_ && keyConfig_.ActionPressed(binding_.modifierAction))
        {
            return true;
        }
    }
    return false;
}

// 選ばれた装備枠を既存のAction番号へ変換し、未成立ならNONEを返す。
PLAYER_ACTION PlayerInputResolver::ResolveSkill(const KeyConfig& keyConfig_,
    const std::vector<PlayerSkillInputBinding>& bindings_)
{
    const int slotIndex_ = ResolveSkillSlot(keyConfig_, bindings_);
    return slotIndex_ >= 0 ? PlayerSkillLoadout::GetSlotAction(static_cast<std::size_t>(slotIndex_)) : PLAYER_ACTION::NONE;
}
