#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

enum class BLACKBOARD_VALUE_TYPE
{
    FLOAT,
    INT,
    BOOL
};

struct BlackboardValueWrite
{
    std::string category = "Attack";
    std::string key = "Damage";
    BLACKBOARD_VALUE_TYPE type = BLACKBOARD_VALUE_TYPE::FLOAT;
    float floatValue = 0.0f;
    int intValue = 0;
    bool boolValue = false;
};

struct ActionReference {
    std::string category;
    std::string actionName;
    float executionTime = 0.0f;
    int timelineStartFrame = 0;
    int timelineEndFrame = 1;
    std::vector<BlackboardValueWrite> blackboardWrites;
};

struct SequenceData {
    std::string name = "New Sequence";
    std::vector<ActionReference> actions;
    float duration = 10.0f;
    int currentFrame = 0;
    int firstFrame = 0;
    int selectedAction = -1;
    bool timelineExpanded = true;
};

struct PendingAction {
    float timer;
    std::string actionName;
    std::vector<BlackboardValueWrite> blackboardWrites;
};

class ActionSequence : public Component {
public:
    const std::string& GetName() const override {
        static std::string name = "ActionSequence";
        return name;
    }
    void Start();
    void Update(float elapsedTime_) override;
    void StartSequence(int index_);
    void SetRegisterGlobal(bool registerGlobal_) { registerGlobal = registerGlobal_; }
    void EnsureImmediateSequence(
        const std::string& sequenceName_,
        const std::vector<ActionReference>& actions_);

    // Register actions that start each configured sequence.
    void RegisterSequence(const std::string& sequenceName_);

    void Gui() override;
    void DrawEditorGui();

    void Serialize(nlohmann::json& j) const;
    void Deserialize(const nlohmann::json& j);

private:
    void ApplyBlackboardWrites(const std::vector<BlackboardValueWrite>& writes_);
    void ExecuteAction(const std::string& actionName_);

    std::vector<SequenceData> sequences;
    std::vector<PendingAction> pendingActions;
    bool registerGlobal = false;
};
