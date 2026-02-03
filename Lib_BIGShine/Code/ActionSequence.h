#pragma once

#include "ComponentBase.h"

struct ActionReference {
    std::string category;
    std::string actionName;
    float delay = 0.0f; // 実行までの待ち時間（秒）
};

struct SequenceData {
    std::string name = "New Sequence";
    std::vector<ActionReference> actions; // std::string から構造体へ変更
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

    // GUIで登録したアクション名を一斉に実行するアクションを登録
    void RegisterSequence(const std::string& sequenceName);

    void Gui() override;

    void Serialize(nlohmann::json& j) const;
    void Deserialize(const nlohmann::json& j);

private:
    std::vector<SequenceData> sequences; // 複数のアクションリストを保持
    std::vector<PendingAction> pendingActions; // 現在進行中のディレイアクション
};