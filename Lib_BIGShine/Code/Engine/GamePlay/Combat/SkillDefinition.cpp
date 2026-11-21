#include "PrecompiledHeader.h"
#include "SkillDefinition.h"
#include "Engine/Debug/TracyProfiler.h"
#include <cmath>
#include <array>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <Windows.h>

namespace
{
    constexpr std::array<const char*, 8> SKILL_CATEGORY_NAMES
    {
        "ATTACK", "RUSH", "LAUNCHER", "PROJECTILE", "HEAL", "BUFF", "COUNTER", "UTILITY"
    };

    // UTF-8のバイト列をchar8_t文字列として渡し、日本語パスをOS用の形式へ変換する。
    std::filesystem::path MakeSkillLibraryPath(const std::string& path_)
    {
        return std::filesystem::path(std::u8string(path_.begin(), path_.end()));
    }

    // 上下・前後の移動値は負値も許可し、非数値だけを拒否する。
    float ReadSkillMovement(const nlohmann::json& json_, const char* key_)
    {
        const float value_ = json_.value(key_, 0.0f);
        if (!std::isfinite(value_))
        {
            throw std::runtime_error(std::string("Invalid movement: ") + key_);
        }
        return value_;
    }

    // 不正な時間値を拒否し、ランタイムが終了不能になる設定を保存しない。
    float ReadSkillTime(const nlohmann::json& json_, const char* key_, float default_)
    {
        const float value_ = json_.value(key_, default_);
        if (!std::isfinite(value_) || value_ < 0.0f)
        {
            throw std::runtime_error(std::string("Invalid time: ") + key_);
        }
        return value_;
    }
}

const char* GetSkillCategoryName(SkillCategory category_)
{
    const auto index_ = static_cast<std::size_t>(category_);
    return index_ < SKILL_CATEGORY_NAMES.size() ? SKILL_CATEGORY_NAMES[index_] : "UTILITY";
}

bool DeserializeSkillDefinition(const nlohmann::json& json_, SkillDefinition& definition_, std::string& error_)
{
    try
    {
        SkillDefinition loaded_;
        loaded_.name = json_.at("Name").get<std::string>();
        if (loaded_.name.empty())
        {
            throw std::runtime_error("Skill name is empty.");
        }
        loaded_.enabled = json_.value("Enabled", true);
        const std::string category_ = json_.value("Category", std::string("UTILITY"));
        bool foundCategory_ = false;
        for (std::size_t index_ = 0; index_ < SKILL_CATEGORY_NAMES.size(); ++index_)
        {
            if (category_ != SKILL_CATEGORY_NAMES[index_])
            {
                continue;
            }
            loaded_.category = static_cast<SkillCategory>(index_);
            foundCategory_ = true;
            break;
        }
        if (!foundCategory_)
        {
            throw std::runtime_error("Unknown Skill Category: " + category_);
        }
        loaded_.canUseOnGround = json_.value("CanUseOnGround", true);
        loaded_.canUseInAir = json_.value("CanUseInAir", false);
        loaded_.cooldown = ReadSkillTime(json_, "Cooldown", 0.0f);
        loaded_.cleanupAction = json_.value("CleanupAction", std::string());
        if (!json_.at("Phases").is_array())
        {
            throw std::runtime_error("Phases must be an array.");
        }
        for (const auto& phaseJson_ : json_.at("Phases"))
        {
            SkillPhase phase_;
            phase_.animationState = phaseJson_.at("Animation").get<std::string>();
            phase_.airAnimationState = phaseJson_.value("AirAnimation", std::string());
            phase_.sequence = phaseJson_.value("Sequence", std::string());
            phase_.motionWarpPreset = phaseJson_.value("MotionWarpPreset", std::string());
            phase_.continueSequence = phaseJson_.value("ContinueSequence", false);
            phase_.playAnimationFromSequence = phaseJson_.value("PlayAnimationFromSequence", false);
            phase_.waitForAnimationEnd = phaseJson_.value("WaitForAnimationEnd", false);
            phase_.enterAction = phaseJson_.value("EnterAction", std::string());
            phase_.exitAction = phaseJson_.value("ExitAction", std::string());
            const std::string end_ = phaseJson_.value("End", std::string("ANIMATION_END"));
            if (end_ == "DURATION")
            {
                phase_.endCondition = SkillPhaseEnd::DURATION;
            }
            else if (end_ == "ACTION_END")
            {
                phase_.endCondition = SkillPhaseEnd::ACTION_END;
            }
            else if (end_ == "GROUND_CONTACT")
            {
                phase_.endCondition = SkillPhaseEnd::GROUND_CONTACT;
            }
            else if (end_ != "ANIMATION_END")
            {
                throw std::runtime_error("Unknown phase end: " + end_);
            }
            phase_.duration = ReadSkillTime(phaseJson_, "Duration", 0.0f);
            phase_.maximumDuration = ReadSkillTime(phaseJson_, "MaximumDuration", 10.0f);
            if (phase_.maximumDuration <= 0.0f)
            {
                throw std::runtime_error("MaximumDuration must be positive.");
            }
            phase_.blendDuration = ReadSkillTime(phaseJson_, "BlendDuration", 0.1f);
            phase_.animationStartTime = ReadSkillTime(phaseJson_, "AnimationStartTime", 0.0f);
            phase_.canDodgeCancel = phaseJson_.value("CanDodgeCancel", false);
            phase_.canActionBranch = phaseJson_.value("CanActionBranch", false);
            phase_.canBufferInput = phaseJson_.value("CanBufferInput", false);
            phase_.enableGuard = phaseJson_.value("EnableGuard", false);
            phase_.preventFlinch = phaseJson_.value("PreventFlinch", false);
            phase_.forwardSpeed = ReadSkillMovement(phaseJson_, "ForwardSpeed");
            phase_.forwardSpeedCurveName = phaseJson_.value("ForwardSpeedCurve", std::string());
            phase_.warpHeight = ReadSkillTime(phaseJson_, "WarpHeight", 0.0f);
            phase_.verticalImpulse = ReadSkillMovement(phaseJson_, "VerticalImpulse");
            phase_.verticalSpeed = ReadSkillMovement(phaseJson_, "VerticalSpeed");
            phase_.verticalSpeedCurveName = phaseJson_.value("VerticalSpeedCurve", std::string());
            phase_.suspendGravity = phaseJson_.value("SuspendGravity", false);
            if ((phase_.verticalSpeed != 0.0f || !phase_.verticalSpeedCurveName.empty()) && !phase_.suspendGravity)
            {
                throw std::runtime_error("VerticalSpeed requires SuspendGravity.");
            }
            loaded_.phases.push_back(phase_);
        }
        definition_ = std::move(loaded_);
        error_.clear();
        return true;
    }
    catch (const std::exception& exception_)
    {
        error_ = exception_.what();
        return false;
    }
}

void SerializeSkillDefinition(const SkillDefinition& definition_, nlohmann::json& json_)
{
    json_["Name"] = definition_.name;
    json_["Enabled"] = definition_.enabled;
    json_["Category"] = GetSkillCategoryName(definition_.category);
    json_["CanUseOnGround"] = definition_.canUseOnGround;
    json_["CanUseInAir"] = definition_.canUseInAir;
    json_["Cooldown"] = definition_.cooldown;
    json_["CleanupAction"] = definition_.cleanupAction;
    const nlohmann::json previousPhases_ = json_.value("Phases", nlohmann::json::array());
    json_["Phases"] = nlohmann::json::array();
    for (std::size_t index_ = 0; index_ < definition_.phases.size(); ++index_)
    {
        const auto& phase_ = definition_.phases[index_];
        nlohmann::json phaseJson_ = index_ < previousPhases_.size() && previousPhases_[index_].is_object()
            ? previousPhases_[index_] : nlohmann::json::object();
        phaseJson_["Animation"] = phase_.animationState;
        phaseJson_["AirAnimation"] = phase_.airAnimationState;
        phaseJson_["Sequence"] = phase_.sequence;
        phaseJson_["MotionWarpPreset"] = phase_.motionWarpPreset;
        phaseJson_["ContinueSequence"] = phase_.continueSequence;
        phaseJson_["PlayAnimationFromSequence"] = phase_.playAnimationFromSequence;
        phaseJson_["WaitForAnimationEnd"] = phase_.waitForAnimationEnd;
        phaseJson_["EnterAction"] = phase_.enterAction;
        phaseJson_["ExitAction"] = phase_.exitAction;
        phaseJson_["End"] = phase_.endCondition == SkillPhaseEnd::DURATION ? "DURATION"
            : phase_.endCondition == SkillPhaseEnd::ACTION_END ? "ACTION_END"
            : phase_.endCondition == SkillPhaseEnd::GROUND_CONTACT ? "GROUND_CONTACT" : "ANIMATION_END";
        phaseJson_["Duration"] = phase_.duration;
        phaseJson_["MaximumDuration"] = phase_.maximumDuration;
        phaseJson_["BlendDuration"] = phase_.blendDuration;
        phaseJson_["AnimationStartTime"] = phase_.animationStartTime;
        phaseJson_["CanDodgeCancel"] = phase_.canDodgeCancel;
        phaseJson_["CanActionBranch"] = phase_.canActionBranch;
        phaseJson_["CanBufferInput"] = phase_.canBufferInput;
        phaseJson_["EnableGuard"] = phase_.enableGuard;
        phaseJson_["PreventFlinch"] = phase_.preventFlinch;
        phaseJson_["ForwardSpeed"] = phase_.forwardSpeed;
        phaseJson_["ForwardSpeedCurve"] = phase_.forwardSpeedCurveName;
        phaseJson_["WarpHeight"] = phase_.warpHeight;
        phaseJson_["VerticalImpulse"] = phase_.verticalImpulse;
        phaseJson_["VerticalSpeed"] = phase_.verticalSpeed;
        phaseJson_["VerticalSpeedCurve"] = phase_.verticalSpeedCurveName;
        phaseJson_["SuspendGravity"] = phase_.suspendGravity;
        json_["Phases"].push_back(std::move(phaseJson_));
    }
}

bool LoadSkillDefinitions(const std::string& path_, std::vector<SkillDefinition>& definitions_,
    nlohmann::json& document_, std::string& error_)
{
    BS_PROFILE_SCOPE("SkillDefinition::Load");
    try
    {
        std::ifstream stream_(MakeSkillLibraryPath(path_));
        if (!stream_)
        {
            throw std::runtime_error("Cannot open skill library: " + path_);
        }
        nlohmann::json loadedDocument_ = nlohmann::json::parse(stream_);
        if (!loadedDocument_.at("Skills").is_array())
        {
            throw std::runtime_error("Skills must be an array.");
        }
        std::vector<SkillDefinition> loaded_;
        std::set<std::string> names_;
        for (const auto& json_ : loadedDocument_.at("Skills"))
        {
            SkillDefinition definition_;
            if (!DeserializeSkillDefinition(json_, definition_, error_))
            {
                return false;
            }
            if (!names_.insert(definition_.name).second)
            {
                throw std::runtime_error("Duplicate skill name: " + definition_.name);
            }
            loaded_.push_back(std::move(definition_));
        }
        definitions_ = std::move(loaded_);
        document_ = std::move(loadedDocument_);
        error_.clear();
        return true;
    }
    catch (const std::exception& exception_)
    {
        error_ = exception_.what();
        return false;
    }
}

bool SaveSkillDefinitions(const std::string& path_, const nlohmann::json& document_, std::string& error_)
{
    BS_PROFILE_SCOPE("SkillDefinition::Save");
    try
    {
        // 保存前にも全定義を検証し、不正な編集で読めないライブラリを作らない。
        if (!document_.at("Skills").is_array())
        {
            throw std::runtime_error("Skills must be an array.");
        }
        std::set<std::string> names_;
        for (const auto& json_ : document_.at("Skills"))
        {
            SkillDefinition definition_;
            if (!DeserializeSkillDefinition(json_, definition_, error_))
            {
                return false;
            }
            if (!names_.insert(definition_.name).second)
            {
                throw std::runtime_error("Duplicate skill name: " + definition_.name);
            }
        }
        const auto destination_ = MakeSkillLibraryPath(path_);
        const auto temporary_ = MakeSkillLibraryPath(path_ + ".tmp");
        if (!destination_.parent_path().empty())
        {
            std::filesystem::create_directories(destination_.parent_path());
        }
        std::ofstream stream_(temporary_, std::ios::binary | std::ios::trunc);
        if (!stream_)
        {
            throw std::runtime_error("Cannot write skill library.");
        }
        // バイナリ書き込みでBOMを付けず、JSONにもCRLFを明示する。
        const std::string text_ = document_.dump(4, ' ', false, nlohmann::json::error_handler_t::strict) + "\n";
        for (const char character_ : text_)
        {
            if (character_ == '\n')
            {
                stream_.put('\r');
            }
            stream_.put(character_);
        }
        stream_.flush();
        if (!stream_)
        {
            throw std::runtime_error("Skill library write failed.");
        }
        stream_.close();
        // Windowsのrenameは既存ファイルを置換しないため、同一ボリュームで置換する。
        if (!MoveFileExW(temporary_.c_str(), destination_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            throw std::runtime_error("Skill library replacement failed.");
        }
        error_.clear();
        return true;
    }
    catch (const std::exception& exception_)
    {
        error_ = exception_.what();
        return false;
    }
}
