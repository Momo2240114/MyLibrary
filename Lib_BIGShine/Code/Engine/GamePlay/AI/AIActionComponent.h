#pragma once

#include "AIBlackboardKeys.h"
#include "Engine/GamePlay/Combat/SkillExecution.h"
#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <string>
#include <vector>

class BlackboardComponent;

// AIアクションを実行するBlackboardの参照先。
enum class AI_ACTION_EXECUTE_TARGET
{
    OWNER,
    GLOBAL,
};

// AIアクションとアニメーションへ渡すBlackboardキーをまとめた設定。
struct AIActionDefinition
{
    // BehaviorGraphや外部処理から指定されるAIアクション名。
    std::string aiActionName = "AI.Action";

    // ActionManagerへ実行要求を出す実アクション名。
    std::string executeActionName;
    // 設定時は共通スキルを実行し、空なら既存Action呼び出しを使用する。
    std::string skillName;

    // AI側の状態を書き込むBlackboardカテゴリ。
    std::string blackboardCategory = AIBlackboardKeys::CATEGORY_AI;

    // 現在実行中のAIアクション名を書き込むキー。
    std::string currentActionKey = AIBlackboardKeys::CURRENT_ACTION;

    // AIアクション終了状態を書き込むキー。
    std::string actionEndedKey = AIBlackboardKeys::ACTION_ENDED;

    // AIアクション実行中状態を書き込むキー。
    std::string isActingKey = AIBlackboardKeys::IS_ACTING;

    // アニメーション開始要求に使用するTrigger名。
    std::string animationTriggerName;

    // アニメーション側へ通知するステート名。
    std::string animationStateName;

    // アニメーション側の状態を書き込むBlackboardカテゴリ。
    std::string animationBlackboardCategory = AIBlackboardKeys::CATEGORY_ANIM;

    // アニメーション側の現在アクション名を書き込むキー。
    std::string animationCurrentActionKey = AIBlackboardKeys::CURRENT_ACTION;

    // アニメーション側の終了状態を書き込むキー。
    std::string animationActionEndedKey = AIBlackboardKeys::ACTION_ENDED;

    // アニメーション側の実行中状態を書き込むキー。
    std::string animationIsActingKey = AIBlackboardKeys::IS_ACTING;

    // アニメーション終了要求に使用するTrigger名。
    std::string animationActionEndTriggerName;

    // trueの場合、終了通知が来るまで次の判断へ進まない。
    bool waitForActionEnd = true;

    // 開始時に終了フラグをfalseへ戻すかどうか。
    bool clearActionEndedOnStart = true;

    // 開始時と終了時に実行中フラグを更新するかどうか。
    bool setIsActing = true;

    // アニメーション用Blackboardにも状態を書き込むかどうか。
    bool writeAnimationBlackboard = true;

    // アクションをOwnerとGlobalのどちらへ要求するか。
    AI_ACTION_EXECUTE_TARGET executeTarget = AI_ACTION_EXECUTE_TARGET::OWNER;
};

// AIアクション実行とAnimationControllerへの橋渡しを担当する。
class AIActionComponent : public Component
{
public:
    AIActionComponent() = default;

    const std::string& GetName() const override
    {
        static std::string name_ = "AIActionComponent";
        return name_;
    }

    void Start() override;
    // 共通スキルの更新と、既存AI Action終了通知を接続する。
    void Update(float deltaTime_) override;
    // Workspaceから共通スキルを既存または新規のAI Actionへ割り当てる。
    bool SetSkillForAction(const std::string& actionName_, const std::string& skillName_, const std::string& path_);
    bool ReloadSkillDefinitions(const std::string& path_);
    void StopSkill();
    const std::vector<AIActionDefinition>& GetActions() const { return actions; }
    const SkillExecution& GetSkillExecution() const { return skillExecution; }
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    // 登録済みのAIアクションを開始する。
    bool ExecuteAIAction(const std::string& aiActionName_);

    // 指定カテゴリのアクション終了をBlackboardへ通知する。
    void NotifyActionEnd(const std::string& category_ = AIBlackboardKeys::CATEGORY_AI);

    // 指定カテゴリのアクション終了状態を取得する。
    bool IsActionEnded(const std::string& category_ = AIBlackboardKeys::CATEGORY_AI) const;

private:
    void EnsureBlackboard();
    void RegisterActions();
    void RegisterBridgeAction(const AIActionDefinition& definition_);
    void ApplyAnimationActionStart(const AIActionDefinition& definition_);
    void ApplyAnimationActionEnd(const AIActionDefinition& definition_);
    AIActionDefinition* FindDefinition(const std::string& aiActionName_);
    const AIActionDefinition* FindDefinition(const std::string& aiActionName_) const;

private:
    // 共通実行をこのAIの更新から一度だけ進める。
    SkillExecution skillExecution;
    std::vector<SkillDefinition> skillDefinitions;
    std::string skillDefinitionsPath = "Data/Skills/PlayerSkills.json";
    std::string runningSkillCategory;
    bool skillActionRunning = false;
    std::vector<AIActionDefinition> actions;
    BlackboardComponent* blackboardComponent = nullptr;
    std::vector<std::string> registeredActionNames;
    std::string statusText;
    int selectedActionIndex = -1;
};
