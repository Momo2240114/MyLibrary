#include "PrecompiledHeader.h"
#include "PlayerSkillDefinition.h"
#include "Engine/Debug/TracyProfiler.h"
#include <utility>

namespace
{
    // Player固有のAction番号は変更せず、共通データの任意項目から解決する。
    PLAYER_ACTION ParseSkillAction(const std::string& name_)
    {
        if (name_ == "SkillA") return PLAYER_ACTION::SKILL_A;
        if (name_ == "SkillB") return PLAYER_ACTION::SKILL_B;
        if (name_ == "SkillC") return PLAYER_ACTION::SKILL_C;
        return PLAYER_ACTION::NONE;
    }
}

bool LoadPlayerSkillDefinitions(const std::string& path_,
    std::vector<PlayerSkillInputBinding>& inputs_, std::vector<PlayerSkillDefinition>& definitions_)
{
    BS_PROFILE_SCOPE("PlayerSkillDefinition::Load");
    std::vector<SkillDefinition> commonDefinitions_;
    nlohmann::json document_;
    std::string error_;
    if (!LoadSkillDefinitions(path_, commonDefinitions_, document_, error_)) return false;
    try
    {
        if (!document_.contains("Inputs") || !document_["Inputs"].is_array()) return false;
        std::vector<PlayerSkillInputBinding> loadedInputs_;
        std::vector<PlayerSkillDefinition> loadedDefinitions_;
        for (const auto& inputJson_ : document_["Inputs"])
        {
            PlayerSkillInputBinding input_;
            input_.action = ParseSkillAction(inputJson_.at("Action").get<std::string>());
            input_.modifierAction = inputJson_.at("ModifierAction").get<std::string>();
            input_.triggerAction = inputJson_.at("TriggerAction").get<std::string>();
            input_.priority = inputJson_.value("Priority", 100);
            if (input_.action == PLAYER_ACTION::NONE || input_.modifierAction.empty() || input_.triggerAction.empty()) return false;
            loadedInputs_.push_back(input_);
        }
        for (std::size_t index_ = 0; index_ < commonDefinitions_.size(); ++index_)
        {
            const std::string actionName_ = document_["Skills"][index_].value("Action", std::string());
            // 敵専用Definitionや未割り当てDefinitionはPlayerのスロットへ登録しない。
            if (actionName_.empty() || actionName_ == "None") continue;
            PlayerSkillDefinition definition_;
            static_cast<SkillDefinition&>(definition_) = commonDefinitions_[index_];
            definition_.action = ParseSkillAction(actionName_);
            if (definition_.action == PLAYER_ACTION::NONE) return false;
            for (const auto& existing_ : loadedDefinitions_)
                if (existing_.action == definition_.action) return false;
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
