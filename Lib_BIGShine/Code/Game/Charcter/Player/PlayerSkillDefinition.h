#pragma once
#include "PlayerState.h"
#include "Engine/GamePlay/Combat/SkillDefinition.h"
#include <string>
#include <vector>

// 既存Player APIの型名を維持し、実行データは敵と共通化する。
using PlayerSkillPhaseEnd = SkillPhaseEnd;
using PlayerSkillPhase = SkillPhase;

// プレイヤー入力の割り当てだけを共通の攻撃定義から分離する。
struct PlayerSkillInputBinding
{
    PLAYER_ACTION action = PLAYER_ACTION::SKILL_A;
    std::string modifierAction = "Player.SkillModifier";
    std::string triggerAction = "Player.StrongAttack";
    int priority = 100;
};

// 共通攻撃データにPlayer Actionへの割り当てを加える薄い定義。
struct PlayerSkillDefinition : SkillDefinition
{
    PLAYER_ACTION action = PLAYER_ACTION::SKILL_A;
};

// 共通ライブラリからPlayer Actionが設定された定義と入力表だけを取り出す。
bool LoadPlayerSkillDefinitions(const std::string& path_,
    std::vector<PlayerSkillInputBinding>& inputs_, std::vector<PlayerSkillDefinition>& definitions_);
