#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

enum class BLACKBOARD_VALUE_TYPE
{
    FLOAT,
    INT,
    BOOL
};

// Action実行前にBlackboardへ書き込む値。
// 攻撃IDやカメラ演出の位置など、Action側へ渡したい一時データに使う。
struct BlackboardValueWrite
{
    std::string category = "Attack";
    std::string key = "Damage";
    BLACKBOARD_VALUE_TYPE type = BLACKBOARD_VALUE_TYPE::FLOAT;
    float floatValue = 0.0f;
    int intValue = 0;
    bool boolValue = false;
};

// Timeline上の1つのAction呼び出し。
// category/actionNameでActionRegistryを参照し、executionTimeで再生タイミングを決める。
struct ActionReference {
    std::string category;
    std::string actionName;
    float executionTime = 0.0f;
    int timelineStartFrame = 0;
    int timelineEndFrame = 1;
    std::vector<BlackboardValueWrite> blackboardWrites;
};

// 複数Actionを時間軸でまとめたシーケンスデータ。
// Animation以外からも、カメラ・VFX・Combatの一連演出を呼べるようにする。
struct SequenceData {
    std::string name = "New Sequence";
    std::vector<ActionReference> actions;
    float duration = 10.0f;
    std::string completionAction; // 全Actionの処理後、Sequence終了時に通知するGlobal Action。
    int currentFrame = 0;
    int firstFrame = 0;
    int selectedAction = -1;
    bool timelineExpanded = true;
};

struct PendingAction {
    // StartSequence後、実行までの残り時間。
    float timer;
    // timerが0以下になったときに呼ぶAction名。
    std::string actionName;
    // Action実行直前にBlackboardへ流し込む一時値。
    std::vector<BlackboardValueWrite> blackboardWrites;
};

// Editorで組んだAction列を再生するComponent。
// 再生中はPendingActionに積み、指定時間にActionRegistryへ実行を流す。
class ActionSequence : public Component {
public:
    const std::string& GetName() const override {
        static std::string name = "ActionSequence";
        return name;
    }
    void Start();
    void Update(float elapsedTime_) override;
    void StartSequence(int index_);
    // 外部Stateは開始時の世代を保存し、自分の予約だけを停止する。
    unsigned int GetPlaybackGeneration() const { return playbackGeneration; }
    void StopPlayback(unsigned int generation_);
    void SetRegisterGlobal(bool registerGlobal_) { registerGlobal = registerGlobal_; }
    // 指定名の即時実行Sequenceが無ければ作成し、外部から呼べるActionとして登録する。
    void EnsureImmediateSequence(
        const std::string& sequenceName_,
        const std::vector<ActionReference>& actions_);

    // 各Sequenceを開始するActionを、OwnerのLocalRegistryへ登録する。
    void RegisterSequence(const std::string& sequenceName_);

    void Gui() override;
    void DrawEditorGui();

    void Serialize(nlohmann::json& j) const;
    void Deserialize(const nlohmann::json& j);

private:
    // Action実行前にBlackboardへ値を書き、Action側が必要なパラメータを読めるようにする。
    void ApplyBlackboardWrites(const std::vector<BlackboardValueWrite>& writes_);
    // OwnerのActionRegistryからAction名を解決して実行する。
    void ExecuteAction(const std::string& actionName_);

    // Editorで作成したSequence一覧。
    std::vector<SequenceData> sequences;
    // 現在再生中で、まだ実行時間に達していないAction一覧。
    std::vector<PendingAction> pendingActions;
    // trueならLocalだけでなくGlobalActionにもSequenceを公開する。
    bool registerGlobal = false;
    float remainingDuration = 0.0f;
    std::string pendingCompletionAction;
    unsigned int playbackGeneration = 0; // Action内で別Sequenceが始まった場合に古い予約を中断する。
};
