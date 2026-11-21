#include "PrecompiledHeader.h"
#include "PlayerSkillDefinition.h"
#include "Engine/Debug/TracyProfiler.h"
#include <utility>

namespace
{
    // Player固有のAction番号は変更せず、共通データの任意項目から解決する。
    PLAYER_ACTION ParseSkillAction(const std::string& name_)
    {
        if (name_ == "SkillA")
        {
            return PLAYER_ACTION::SKILL_A;
        }
        if (name_ == "SkillB")
        {
            return PLAYER_ACTION::SKILL_B;
        }
        if (name_ == "SkillC")
        {
            return PLAYER_ACTION::SKILL_C;
        }
        if (name_ == "SkillD")
        {
            return PLAYER_ACTION::SKILL_D;
        }
        return PLAYER_ACTION::NONE;
    }
}

// 共通スキルと入力表を一時領域へ読み込み、全項目の検証成功後に呼び出し元へ反映する。
bool LoadPlayerSkillDefinitions(const std::string& path_,
    std::vector<PlayerSkillInputBinding>& inputs_, std::vector<PlayerSkillDefinition>& definitions_)
{
    BS_PROFILE_SCOPE("PlayerSkillDefinition::Load");
    std::vector<SkillDefinition> commonDefinitions_;
    nlohmann::json document_;
    std::string error_;
    if (!LoadSkillDefinitions(path_, commonDefinitions_, document_, error_))
    {
        return false;
    }
    try
    {
        if (!document_.contains("Inputs") || !document_["Inputs"].is_array())
        {
            return false;
        }
        std::vector<PlayerSkillInputBinding> loadedInputs_;
        std::vector<PlayerSkillDefinition> loadedDefinitions_;
        for (const auto& inputJson_ : document_["Inputs"])
        {
            if (inputJson_.contains("Slot") && !inputJson_.at("Slot").is_number_integer())
            {
                return false;
            }
            PlayerSkillInputBinding input_;
            input_.slotIndex = inputJson_.contains("Slot") ? inputJson_.at("Slot").get<int>()
                : PlayerSkillLoadout::GetSlotIndex(ParseSkillAction(inputJson_.at("Action").get<std::string>()));
            input_.modifierAction = inputJson_.at("ModifierAction").get<std::string>();
            input_.triggerAction = inputJson_.at("TriggerAction").get<std::string>();
            input_.priority = inputJson_.value("Priority", 100);
            if (input_.slotIndex < 0 || input_.slotIndex >= static_cast<int>(PlayerSkillLoadout::SLOT_COUNT) ||
                input_.modifierAction.empty() || input_.triggerAction.empty())
            {
                return false;
            }
            loadedInputs_.push_back(input_);
        }
        for (std::size_t index_ = 0; index_ < commonDefinitions_.size(); ++index_)
        {
            const std::string actionName_ = document_["Skills"][index_].value("Action", std::string());
            // Action未指定の回復・Buff・Projectileなども装備候補へ含める。
            PlayerSkillDefinition definition_;
            static_cast<SkillDefinition&>(definition_) = commonDefinitions_[index_];
            definition_.action = ParseSkillAction(actionName_);

            loadedDefinitions_.push_back(std::move(definition_));
        }
        inputs_ = std::move(loadedInputs_);
        definitions_ = std::move(loadedDefinitions_);
        return true;
    }
    catch (const nlohmann::json::exception&)
    {
        return false;
    }
}

// 画面上の枠順と既存のAction番号の違いを吸収し、保存済みのAction番号を維持する。
PLAYER_ACTION PlayerSkillLoadout::GetSlotAction(std::size_t slotIndex_)
{
    constexpr std::array<PLAYER_ACTION, SLOT_COUNT> SLOT_ACTIONS
    {
        PLAYER_ACTION::SKILL_B, PLAYER_ACTION::SKILL_A, PLAYER_ACTION::SKILL_C, PLAYER_ACTION::SKILL_D
    };
    return slotIndex_ < SLOT_COUNT ? SLOT_ACTIONS[slotIndex_] : PLAYER_ACTION::NONE;
}

// スキル用Actionから装備枠へ逆引きし、通常行動は枠なしとして扱う。
int PlayerSkillLoadout::GetSlotIndex(PLAYER_ACTION action_)
{
    for (std::size_t slotIndex_ = 0; slotIndex_ < SLOT_COUNT; ++slotIndex_)
    {
        if (GetSlotAction(slotIndex_) == action_)
        {
            return static_cast<int>(slotIndex_);
        }
    }
    return -1;
}

const std::string& PlayerSkillLoadout::GetSkillName(std::size_t slotIndex_) const
{
    static const std::string EMPTY_SKILL_NAME;
    return slotIndex_ < SLOT_COUNT ? skillNames[slotIndex_] : EMPTY_SKILL_NAME;
}

bool PlayerSkillLoadout::SetSkillName(std::size_t slotIndex_, const std::string& skillName_)
{
    if (slotIndex_ >= SLOT_COUNT)
    {
        return false;
    }
    skillNames[slotIndex_] = skillName_;
    return true;
}

// Sceneには各枠の装備名だけを保存し、共通定義や実行中の待ち時間は複製しない。
void PlayerSkillLoadout::Serialize(nlohmann::json& json_) const
{
    json_ = nlohmann::json::object();
    for (std::size_t slotIndex_ = 0; slotIndex_ < SLOT_COUNT; ++slotIndex_)
    {
        json_["Slot" + std::to_string(slotIndex_)] = skillNames[slotIndex_];
    }
}

// 省略された枠は現在値を維持し、不正な型があれば装備全体の更新を取り消す。
bool PlayerSkillLoadout::Deserialize(const nlohmann::json& json_)
{
    if (!json_.is_object())
    {
        return false;
    }
    auto loadedNames_ = skillNames;
    for (std::size_t slotIndex_ = 0; slotIndex_ < SLOT_COUNT; ++slotIndex_)
    {
        const std::string key_ = "Slot" + std::to_string(slotIndex_);
        if (!json_.contains(key_))
        {
            continue;
        }
        if (!json_.at(key_).is_string())
        {
            return false;
        }
        loadedNames_[slotIndex_] = json_.at(key_).get<std::string>();
    }
    skillNames = std::move(loadedNames_);
    return true;
}
