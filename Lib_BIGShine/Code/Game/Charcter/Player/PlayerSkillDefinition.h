#pragma once
#include "PlayerState.h"
#include "Engine/GamePlay/Combat/SkillDefinition.h"
#include <string>
#include <vector>
#include <array>
#include <cstddef>

// 既存Player APIの型名を維持し、実行データは敵と共通化する。
using PlayerSkillPhaseEnd = SkillPhaseEnd;
using PlayerSkillPhase = SkillPhase;

// Skillの内容やCooldownを含めず、4枠の装備名と既存State識別子の対応だけを保持する。
struct PlayerSkillLoadout
{
    static constexpr std::size_t SLOT_COUNT = 4;
    // Slot0=□、Slot1=△、Slot2=○、Slot3=×。既存Action番号を変えずにStateへ接続する。
    static PLAYER_ACTION GetSlotAction(std::size_t slotIndex_);
    static int GetSlotIndex(PLAYER_ACTION action_);
    const std::string& GetSkillName(std::size_t slotIndex_) const;
    bool SetSkillName(std::size_t slotIndex_, const std::string& skillName_);
    // 装備名だけをSceneへ保存し、実行時の残り時間を混ぜない。
    void Serialize(nlohmann::json& json_) const;
    // 全項目の型を確認してから装備名を更新し、失敗時は元の装備を維持する。
    bool Deserialize(const nlohmann::json& json_);

private:
    std::array<std::string, SLOT_COUNT> skillNames{ "PhantomRush", "Breakthrough", "RisingJudgment", "Heal" };
};

// 複合入力は装備名を参照せず、対象スロットだけを決定する。
struct PlayerSkillInputBinding
{
    // 装備先の枠番号。実際のスキル名はLoadoutから解決する。
    int slotIndex = 1;
    // 保持中の修飾入力と、同時に押し始めた発動入力のAction名。
    std::string modifierAction = "Player.SkillModifier";
    std::string triggerAction = "Player.StrongAttack";
    // 同時成立した候補のうち大きい値を優先し、同値なら先に登録した候補を使う。
    int priority = 100;
};

// 共通Skill定義に旧JSONの初期割り当て情報だけを保持する互換用の型。
struct PlayerSkillDefinition : SkillDefinition
{
    PLAYER_ACTION action = PLAYER_ACTION::NONE; // 旧Action項目の移行専用。実行時はLoadoutから解決する。
};

// 共通ライブラリ全体を装備候補として読み込み、新旧形式の入力表を解決する。
bool LoadPlayerSkillDefinitions(const std::string& path_,
    std::vector<PlayerSkillInputBinding>& inputs_, std::vector<PlayerSkillDefinition>& definitions_);
