#pragma once
#include "PlayerSkillDefinition.h"

class KeyConfig;

// 演出内容を参照せず、複合入力の優先スロットActionと通常入力の消費を決定する。
class PlayerInputResolver
{
public:
    // Modifier HoldとTrigger押下が成立した候補から最高優先度のスロット番号を返す。
    static int ResolveSkillSlot(const KeyConfig& keyConfig_,
        const std::vector<PlayerSkillInputBinding>& bindings_);
    // 既存のキャンセル入力確認APIには、スロットをState識別子へ変換して返す。
    static PLAYER_ACTION ResolveSkill(const KeyConfig& keyConfig_,
        const std::vector<PlayerSkillInputBinding>& bindings_);
    // 対応するModifierを保持中の通常Actionを後段へ流さない。
    static bool IsActionConsumed(const KeyConfig& keyConfig_,
        const std::vector<PlayerSkillInputBinding>& bindings_, const std::string& action_);
};
