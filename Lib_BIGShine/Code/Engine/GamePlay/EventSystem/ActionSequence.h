#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

struct ActionReference {
    std::string category;
    std::string actionName;
    float executionTime = 0.0f;
    int timelineStartFrame = 0;
    int timelineEndFrame = 1;
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
};

class ActionSequence : public Component {
public:
    const std::string& GetName() const override {
        static std::string name = "ActionSequence";
        return name;
    }
    void Start();
    void Update(float et) override;
    void StartSequence(int index);

    // Register actions that start each configured sequence.
    void RegisterSequence(const std::string& sequenceName);

    void Gui() override;
    void DrawEditorGui();

    void Serialize(nlohmann::json& j) const;
    void Deserialize(const nlohmann::json& j);

private:
    std::vector<SequenceData> sequences;
    std::vector<PendingAction> pendingActions;
};
