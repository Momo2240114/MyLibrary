#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

// プレイヤーと敵が共通で使用するPhase終了条件。
enum class SkillPhaseEnd
{
    ANIMATION_END,
    DURATION,
    ACTION_END
};

// 既存Animation EventとActionSequenceを組み合わせる攻撃の一区間。
struct SkillPhase
{
    std::string animationState;
    std::string sequence;
    std::string enterAction;
    std::string exitAction;
    SkillPhaseEnd endCondition = SkillPhaseEnd::ANIMATION_END;
    float duration = 0.0f;
    float maximumDuration = 10.0f;
    float blendDuration = 0.1f;
    bool canDodgeCancel = false;
    bool canActionBranch = false;
    bool canBufferInput = false;
};

// 入力やAI判断を含まず、攻撃の実行内容だけを保持する共通データ。
struct SkillDefinition
{
    std::string name;
    bool enabled = true;
    float cooldown = 0.0f; // 将来用の設定値。AI側の既存Cooldown管理は維持する。
    std::string cleanupAction;
    std::vector<SkillPhase> phases;
};

// 共通項目を読み込み、Player Actionなど呼び出し側固有の項目には依存しない。
bool DeserializeSkillDefinition(const nlohmann::json& json_, SkillDefinition& definition_, std::string& error_);
// 元のJSONへ共通項目だけを書き込み、入力割り当てや未対応項目を保持する。
void SerializeSkillDefinition(const SkillDefinition& definition_, nlohmann::json& json_);
// 共通ライブラリを検証して読み込み、成功時だけ現在の定義を差し替える。
bool LoadSkillDefinitions(const std::string& path_, std::vector<SkillDefinition>& definitions_,
    nlohmann::json& document_, std::string& error_);
// 一時ファイルへの書き込み成功後に置換し、失敗時に既存ファイルを失わない。
bool SaveSkillDefinitions(const std::string& path_, const nlohmann::json& document_, std::string& error_);
