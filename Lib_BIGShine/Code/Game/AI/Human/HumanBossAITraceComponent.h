#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/AI/AIBehaviorGraph.h"

#include <cstdint>
#include <deque>
#include <string>

class HumanBossAIComponent;

// 人型ボスの判断時点を固定長履歴へ保存し、進行状態とプレイヤー履歴の関係を確認する。
class HumanBossAITraceComponent : public Component
{
public:
    const std::string& GetName() const override;
    // リトライやシーン再開時に以前の追跡結果を残さない。
    void Start() override;
    // 通常AIが停止中でもGameModeの状態変化を観測する。
    void UpdateUnscaled(float deltaTime_) override;
    // Inspectorから記録設定と履歴を確認する。
    void Gui() override;
    // Inspectorとは別の追跡ウィンドウを表示する。
    void RenderUI() override;
    // 履歴本体はシーンへ保存せず、記録設定だけを保存する。
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    bool IsRecording() const
    {
        return IsEnabled() && recordingEnabled;
    }

    // 実際のグラフ評価から候補と通過条件を収集し、AIの再評価は行わない。
    void BeginEvaluation();
    void FinishEvaluation();
    void ObserveActionWeight(const std::string& actionName_, float weight_, const std::string& reason_);
    void ObserveGraphWeight(const std::string& actionName_, float baseWeight_, float modifier_);
    void ObserveNode(const AIBehaviorNode& node_, AI_BEHAVIOR_STATUS status_, const AIBehaviorContext& context_);
    // 同じ状態の連続記録を抑え、選択・終了・停止理由の変化だけを保存する。
    void Capture(const HumanBossAIComponent& ai_, const char* event_, bool force_ = false);

private:
    // 各行が当時の状態を所有し、選択後に現在値で書き換わらないようにする。
    struct TraceEntry
    {
        std::uint64_t id = 0;
        float time = 0.0f;
        std::string event;
        nlohmann::json snapshot;
    };

    void ClearHistory();
    void DrawHistory();
    void DrawSnapshot(const nlohmann::json& snapshot_);
    // ユーザーが選んだJSONファイルへ、現在保持している履歴を書き出す。
    bool ExportTrace(const std::string& path_) const;

    bool recordingEnabled = true;
    int maxEntries = 256;
    bool showWindow = false;
    bool followLatest = true;
    bool evaluating = false;
    std::uint64_t nextEntryId = 1;
    std::uint64_t selectedEntryId = 0;
    std::uint64_t startTime = 0;
    std::uint64_t evaluationTime = 0;
    std::deque<TraceEntry> entries;
    std::string lastSignature;
    std::string graphAction;
    std::string exportStatus;
    ImGuiTextFilter historyFilter;
    nlohmann::json evaluatedWeights = nlohmann::json::array();
    nlohmann::json evaluatedNodes = nlohmann::json::array();
};
