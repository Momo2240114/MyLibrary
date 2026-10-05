#pragma once
#include "PlayerSkillDefinition.h"

class KeyConfig;

// 演出内容を参照せず、複合入力の優先Actionと通常入力の消費を決定する。
class PlayerInputResolver
{
public:
    // Modifier HoldとTrigger押下が成立した候補から最高優先度を返す。
    static PLAYER_ACTION ResolveSkill(const KeyConfig& keyConfig_,
        const std::vector<PlayerSkillInputBinding>& bindings_);
    // 対応するModifierを保持中の通常Actionを後段へ流さない。
    static bool IsActionConsumed(const KeyConfig& keyConfig_,
        const std::vector<PlayerSkillInputBinding>& bindings_, const std::string& action_);
};
