#include "PrecompiledHeader.h"

#include "ActionSequence.h"
#include "PrefabSpawnActionComponent.h"
#include "ObjectDeleteActionComponent.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include <imgui_stdlib.h>
#include "ImSequencer.h"

#include <algorithm>
#include <cmath>
#include <fstream>

namespace
{
    // ImSequencerはFrame単位で編集するため、ActionSequence内部の秒数と相互変換する。
    constexpr float SEQUENCE_FPS = 60.0f;
    constexpr unsigned int ACTION_COLOR = 0xFF4FA3FF;

    // ImSequencerへSequenceDataを渡すためのAdapter。
    // Editor上の移動/追加/削除をActionReferenceのexecutionTimeへ反映する。
    struct ActionSequenceAdapter : public ImSequencer::SequenceInterface
    {
        SequenceData* sequence = nullptr;
        int* currentFrame = nullptr;

        int ToFrame(float time_) const
        {
            // 秒指定のAction時間をTimeline表示用のFrameへ変換する。
            return static_cast<int>(std::round(time_ * SEQUENCE_FPS));
        }

        float ToTime(int frame_) const
        {
            // Editorで動かしたFrame位置を、実行に使う秒数へ戻す。
            return static_cast<float>(frame_) / SEQUENCE_FPS;
        }

        void SyncFrames()
        {
            if (!sequence) return;

            // executionTimeを正として、表示用の開始/終了Frameを再同期する。
            for (auto& action_ : sequence->actions)
            {
                action_.timelineStartFrame = ToFrame(action_.executionTime);
                action_.timelineEndFrame = action_.timelineStartFrame + 1;
            }
        }

        int GetFrameMin() const override { return 0; }
        int GetFrameMax() const override
        {
            return sequence ? (std::max)(1, ToFrame(sequence->duration)) : 1;
        }
        int GetItemCount() const override
        {
            return sequence ? static_cast<int>(sequence->actions.size()) : 0;
        }
        int GetItemTypeCount() const override { return 1; }
        const char* GetItemTypeName(int typeIndex_) const override { return "Action"; }

        const char* GetItemLabel(int index_) const override
        {
            if (!sequence || index_ < 0 || index_ >= static_cast<int>(sequence->actions.size())) return "";
            const auto& action_ = sequence->actions[index_];
            // Action未選択時もTimeline上で空欄にせず、選択待ちだと分かる表示にする。
            return action_.actionName.empty() ? "(Select Action)" : action_.actionName.c_str();
        }

        void Get(int index_, int** start_, int** end_, int* type_, unsigned int* color_) override
        {
            if (!sequence || index_ < 0 || index_ >= static_cast<int>(sequence->actions.size())) return;

            // ImSequencerが直接Frame値を編集できるよう、ActionReference内のFrame値のポインタを渡す。
            auto& action_ = sequence->actions[index_];
            if (start_) *start_ = &action_.timelineStartFrame;
            if (end_) *end_ = &action_.timelineEndFrame;
            if (type_) *type_ = 0;
            if (color_) *color_ = ACTION_COLOR;
        }

        void EndEdit() override
        {
            if (!sequence) return;

            // 編集終了時にFrame値を秒数へ戻し、Sequence長がAction位置を下回らないよう補正する。
            for (auto& action_ : sequence->actions)
            {
                action_.timelineStartFrame = (std::max)(0, action_.timelineStartFrame);
                action_.timelineEndFrame = action_.timelineStartFrame + 1;
                action_.executionTime = ToTime(action_.timelineStartFrame);
                if (!action_.animationState.empty() && action_.useNormalizedTime)
                    action_.executionTime = (std::min)(1.0f, action_.executionTime);
                sequence->duration = (std::max)(sequence->duration, action_.executionTime);
            }
        }

        void Add(int typeIndex_) override
        {
            if (!sequence) return;

            // Add時は現在のTimelineカーソル位置へActionを配置する。
            ActionReference action_;
            action_.executionTime = currentFrame ? ToTime(*currentFrame) : 0.0f;
            sequence->actions.push_back(action_);
            SyncFrames();
        }

        void Del(int index_) override
        {
            if (!sequence || index_ < 0 || index_ >= static_cast<int>(sequence->actions.size())) return;
            // Timeline上で選択されたActionをSequenceから削除する。
            sequence->actions.erase(sequence->actions.begin() + index_);
        }

        void Duplicate(int index_) override
        {
            if (!sequence || index_ < 0 || index_ >= static_cast<int>(sequence->actions.size())) return;

            // 複製直後に完全に重ならないよう、1Frameだけ後ろへずらす。
            auto action_ = sequence->actions[index_];
            const float maximumTime_ = !action_.animationState.empty() && action_.useNormalizedTime ? 1.0f : sequence->duration;
            action_.executionTime = (std::min)(maximumTime_, action_.executionTime + (1.0f / SEQUENCE_FPS));
            sequence->actions.push_back(action_);
            SyncFrames();
        }
    };
}

void ActionSequence::Start()
{
    // Component開始時に登録済みSequenceをActionRegistryへ公開する。
    RegisterSequence("");
}

// 中断・Scene破棄では、完了通知を発行せず生成階層を終了する。
void ActionSequence::OnDestroy()
{
    pendingActions.clear();
    pendingCompletionAction.clear();
    CleanupSpawnedObjects(true);
}

// 既存の呼び出し口を維持し、生成物の所有権は専用Componentへ移す。
void ActionSequence::TrackSpawnedObject(const std::shared_ptr<GameObject>& object_)
{
    const auto owner_ = GetGameObject();
    if (const auto spawner_ = owner_ ? owner_->GetComponent<PrefabSpawnActionComponent>() : nullptr)
        spawner_->TrackSpawnedObject(object_);
}

void ActionSequence::PromoteSpawnedObject(const std::shared_ptr<GameObject>& object_)
{
    const auto owner_ = GetGameObject();
    if (const auto spawner_ = owner_ ? owner_->GetComponent<PrefabSpawnActionComponent>() : nullptr)
        spawner_->CommitActor(object_);
}

void ActionSequence::CleanupSpawnedObjects(bool includePersistent_)
{
    const auto owner_ = GetGameObject();
    if (const auto deleter_ = owner_ ? owner_->GetComponent<ObjectDeleteActionComponent>() : nullptr)
        deleter_->CleanupSpawnedObjects(includePersistent_);
}

void ActionSequence::Update(float elapsedTime_)
{
    BS_PROFILE_SCOPE("ActionSequence::Update");
    const unsigned int generation_ = playbackGeneration;
    remainingDuration -= (std::max)(elapsedTime_, 0.0f);
    for (auto& pending_ : pendingActions)
        if (pending_.animationState.empty()) pending_.timer -= (std::max)(elapsedTime_, 0.0f);
    const auto owner_ = GetGameObject();
    const auto animation_ = owner_ ? owner_->GetComponent<AnimationControllerComponent>() : nullptr;
    // 呼び出し先がSequenceを再生してもIteratorを壊さないよう、実行前に予約を取り出す。
    for (std::size_t index_ = 0; index_ < pendingActions.size();)
    {
        const auto& pending_ = pendingActions[index_];
        bool ready_ = pending_.timer <= 0.0f;
        if (ready_ && !pending_.waitForObject.empty())
        {
            const auto awaited_ = owner_ && owner_->manager ? owner_->GetManager().Find(pending_.waitForObject) : nullptr;
            if (!awaited_ || !awaited_->IsHierarchyReady())
            {
                // このFrameで消費した時計を戻し、後続Actionを生成完了より先に実行しない。
                remainingDuration += (std::max)(elapsedTime_, 0.0f);
                for (auto& waiting_ : pendingActions)
                    if (waiting_.animationState.empty()) waiting_.timer += (std::max)(elapsedTime_, 0.0f);
                return;
            }
        }
        if (!pending_.animationState.empty())
        {
            const auto state_ = animation_ ? animation_->GetCurrentState() : nullptr;
            const float duration_ = animation_ ? animation_->GetCurrentSampleDuration() : 0.0f;
            const float position_ = animation_ ? animation_->GetAnimatorTime() : 0.0f;
            // 別段の予約を先に消費せず、そのStateが実際に再生されたときだけ判定する。
            ready_ = state_ && state_->name == pending_.animationState &&
                (!pending_.useNormalizedTime || duration_ > 0.0001f) &&
                position_ >= pending_.timer * (pending_.useNormalizedTime ? duration_ : 1.0f);
        }
        if (!ready_) { ++index_; continue; }
        const PendingAction action_ = pending_;
        pendingActions.erase(pendingActions.begin() + index_);
        ApplyBlackboardWrites(action_.blackboardWrites);
        ExecuteAction(action_.actionName);
        if (generation_ != playbackGeneration) return;
    }
    if (remainingDuration <= 0.0f && pendingActions.empty() && !pendingCompletionAction.empty())
    {
        const std::string completionAction_ = pendingCompletionAction;
        pendingCompletionAction.clear();
        if (cleanupObjectsOnCompletion) CleanupSpawnedObjects();
        ExecuteAction(completionAction_);
    }
}


void ActionSequence::StartSequence(int index_)
{
    BS_PROFILE_SCOPE("ActionSequence::StartSequence");
    if (index_ < 0 || index_ >= (int)sequences.size()) return;

    pendingActions.clear();
    ++playbackGeneration;
    remainingDuration = sequences[index_].duration;
    pendingCompletionAction = sequences[index_].completionAction;
    cleanupObjectsOnCompletion = sequences[index_].cleanupObjectsOnCompletion;
    interruptionAction = sequences[index_].interruptionAction;

    // Editorで並び順を変更しても実行時刻を優先し、同時刻だけ保存順を維持する。
    for (const ActionReference& action_ : sequences[index_].actions) {
        if (action_.actionName.empty() && action_.waitForObject.empty()) continue;
        pendingActions.push_back({ action_.waitForObject, action_.executionTime, action_.actionName, action_.blackboardWrites,
            action_.animationState, action_.useNormalizedTime });
    }
    std::stable_sort(pendingActions.begin(), pendingActions.end(),
        [](const PendingAction& left_, const PendingAction& right_) { return left_.timer < right_.timer; });
    Update(0.0f);
}

void ActionSequence::EnsureImmediateSequence(
    const std::string& sequenceName_,
    const std::vector<ActionReference>& actions_)
{
    if (sequenceName_.empty()) return;

    const auto sequenceIterator_ = std::find_if(
        sequences.begin(),
        sequences.end(),
        [&sequenceName_](const SequenceData& sequence_)
        {
            return sequence_.name == sequenceName_;
        });
    if (sequenceIterator_ == sequences.end())
    {
        // Combatなど外部コードから即時Sequenceを要求された場合、Editor用データとしても保持する。
        SequenceData sequence_{};
        sequence_.name = sequenceName_;
        sequence_.duration = 0.1f;
        sequence_.actions = actions_;
        for (ActionReference& action_ : sequence_.actions)
        {
            action_.executionTime = 0.0f;
        }
        sequences.push_back(sequence_);
    }

    RegisterSequence(sequenceName_);
}

void ActionSequence::RegisterSequence(const std::string& sequenceName_)
{
    (void)sequenceName_;
    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_) return;
    ActionRegistry& localRegistry_ = owner_->GetLocalRegistry();

    // Sequence名をActionとして登録し、外部から「Sequenceカテゴリ + 名前」で再生できるようにする。
    for (int index_ = 0; index_ < static_cast<int>(sequences.size()); ++index_) {
        if (sequences[index_].name.empty()) continue;

        const int sequenceIndex_ = index_;
        const std::string sequenceName_ = sequences[index_].name;
        const std::weak_ptr<GameObject> weakOwner_ = owner_;
        const auto startSequence_ = [weakOwner_, sequenceIndex_]() {
            const auto owner_ = weakOwner_.lock();
            if (!owner_ || owner_->IsPendingDestroy() || !owner_->manager) return;
            if (const auto sequence_ = owner_->GetComponent<ActionSequence>()) sequence_->StartSequence(sequenceIndex_);
            };
        localRegistry_.RegisterAction(
            "Sequence",
            sequenceName_,
            startSequence_);
        if (registerGlobal)
        {
            // GameModeなどObjectに依存しない進行制御から呼べるよう、Sequence名そのものもGlobalへ公開する。
            ActionRegistry::Instance().RemoveAction("Sequence", sequenceName_);
            owner_->RegisterNamedGlobalAction("Sequence", sequenceName_, startSequence_);
            owner_->RegisterGlobalAction("Sequence", sequenceName_, startSequence_);
        }
    }
}

void ActionSequence::Gui()
{
    ImGui::Checkbox("Register Global", &registerGlobal);
    ImGui::TextDisabled("Sequences are always registered to the owner locally.");
    ImGui::TextDisabled("Edit sequences from Window > Action Sequence Editor.");
}

bool ActionSequence::LoadMissingSequences(const std::string& path_)
{
    BS_PROFILE_SCOPE("ActionSequence::LoadMissingSequences");
    try
    {
        std::ifstream stream_(path_);
        if (!stream_) return false;
        // 一時データで既存の復元処理を使い、Owner側の再生予約と編集済みデータを維持する。
        ActionSequence defaults_;
        defaults_.Deserialize(nlohmann::json::parse(stream_));
        for (const auto& sequence_ : defaults_.sequences)
        {
            const bool exists_ = std::any_of(sequences.begin(), sequences.end(),
                [&sequence_](const SequenceData& current_) { return current_.name == sequence_.name; });
            if (!exists_) sequences.push_back(sequence_);
        }
        RegisterSequence("");
        return true;
    }
    catch (const nlohmann::json::exception&)
    {
        return false;
    }
}

void ActionSequence::ApplyBlackboardWrites(
    const std::vector<BlackboardValueWrite>& writes_)
{
    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_ || writes_.empty()) return;

    // Action実行直前にBlackboardへ値を書き、呼び出し先へ簡易引数のように渡す。
    const std::shared_ptr<BlackboardComponent> blackboard_ =
        owner_->GetComponent<BlackboardComponent>();
    if (!blackboard_) return;

    for (const BlackboardValueWrite& write_ : writes_)
    {
        if (write_.category.empty() || write_.key.empty()) continue;

        switch (write_.type)
        {
        case BLACKBOARD_VALUE_TYPE::FLOAT:
            blackboard_->SetFloat(write_.category, write_.key, write_.floatValue);
            break;
        case BLACKBOARD_VALUE_TYPE::INT:
            blackboard_->SetInt(write_.category, write_.key, write_.intValue);
            break;
        case BLACKBOARD_VALUE_TYPE::BOOL:
            blackboard_->SetBool(write_.category, write_.key, write_.boolValue);
            break;
        }
    }
}

void ActionSequence::ExecuteAction(const std::string& actionName_)
{
    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (owner_) owner_->ExecuteAction(actionName_);
}

void ActionSequence::DrawEditorGui()
{
    // OwnerのLocalActionと、共有GlobalActionの両方からSequence内で呼ぶActionを選べるようにする。
    auto& globalReg = ActionRegistry::Instance();
    auto owner = gameObject.lock();

    ImGui::Checkbox("Register Global", &registerGlobal);
    ImGui::TextDisabled("OFF: Local only / ON: Local and global");
    ImGui::Separator();

    struct CategoryEntry { std::string name; bool isLocal; };
    std::vector<CategoryEntry> allCategories;

    if (owner) {
        // LocalActionはOwner固有の行動なので、同名CategoryでもGlobalより優先して候補へ並べる。
        for (const auto& cat : owner->localRegistry.GetCategories()) {
            allCategories.push_back({ cat, true });
        }
    }
    for (const auto& cat : globalReg.GetCategories()) {
        allCategories.push_back({ cat, false });
    }

    for (int sequenceIndex_ = 0; sequenceIndex_ < (int)sequences.size(); ++sequenceIndex_) {
        ImGui::PushID(sequenceIndex_);
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap;
        auto& sequence_ = sequences[sequenceIndex_];

        if (ImGui::CollapsingHeader(sequence_.name.empty() ? "Unnamed Sequence" : sequence_.name.c_str(), nullptr, flags))
        {
            // Sequence名はRegistryへ登録されるAction名でもあるため、ここで編集できるようにする。
            char buf[64];
            strcpy_s(buf, sequence_.name.c_str());
            if (ImGui::InputText("Sequence Name", buf, sizeof(buf))) {
                sequence_.name = buf;
            }

            ImGui::SameLine();
            if (ImGui::Button("Delete List")) {
                sequences.erase(sequences.begin() + sequenceIndex_);
                ImGui::PopID();
                --sequenceIndex_;
                continue;
            }

            ImGui::SetNextItemWidth(180.0f);
            char completionBuffer_[256]{};
            strncpy_s(completionBuffer_, sequence_.completionAction.c_str(), _TRUNCATE);
            if (ImGui::InputText("Completion Action", completionBuffer_, sizeof(completionBuffer_)))
                sequence_.completionAction = completionBuffer_;
            ImGui::InputText("Interruption Action", &sequence_.interruptionAction);
            ImGui::Checkbox("Cleanup Spawned Objects On Completion", &sequence_.cleanupObjectsOnCompletion);

            ImGui::SetNextItemWidth(180.0f);
            if (ImGui::DragFloat("Timeline Length", &sequence_.duration, 0.1f, 0.1f, 600.0f, "%.2f sec")) {
                // Timeline長は最低0.1秒を保証し、SequencerのFrame範囲が0にならないようにする。
                sequence_.duration = (std::max)(0.1f, sequence_.duration);
            }

            ImGui::SameLine();
            if (ImGui::Button("Sort by Start Time")) {
                // 実行順を見た目と合わせたいときの手動整列。選択Indexはずれるため解除する。
                std::stable_sort(
                    sequence_.actions.begin(),
                    sequence_.actions.end(),
                    [](const ActionReference& left_, const ActionReference& right_)
                    {
                        return left_.executionTime < right_.executionTime;
                    });
                sequence_.selectedAction = -1;
            }

            ActionSequenceAdapter adapter_;
            adapter_.sequence = &sequence_;
            adapter_.currentFrame = &sequence_.currentFrame;
            // Sequencer表示前に秒数からFrameへ同期し、Editor上の位置を最新化する。
            adapter_.SyncFrames();

            const bool showSequencer_ = ImGui::BeginChild("ActionSequenceTimeline", ImVec2(0, 260), true);
            if (showSequencer_) {
                ImSequencer::Sequencer(
                    &adapter_,
                    &sequence_.currentFrame,
                    &sequence_.timelineExpanded,
                    &sequence_.selectedAction,
                    &sequence_.firstFrame,
                    ImSequencer::SEQUENCER_EDIT_ALL |
                    ImSequencer::SEQUENCER_ADD |
                    ImSequencer::SEQUENCER_DEL |
                    ImSequencer::SEQUENCER_CHANGE_FRAME);
            }
            ImGui::EndChild();

            if (ImGui::Button("Register & Play", ImVec2(-1, 25))) {
                // 編集中のSequenceをRegistryへ反映して、その場で再生確認する。
                RegisterSequence("");
                StartSequence(sequenceIndex_);
            }

            if (sequence_.selectedAction >= 0 && sequence_.selectedAction < static_cast<int>(sequence_.actions.size())) {
                auto& action_ = sequence_.actions[sequence_.selectedAction];
                ImGui::SeparatorText("Selected Action");

                ImGui::SetNextItemWidth(180.0f);
                const std::string categoryLabel_ = action_.category.empty() ? "Category" : action_.category;
                if (ImGui::BeginCombo("Category", categoryLabel_.c_str())) {
                    for (const auto& entry_ : allCategories) {
                        // [L]/[G]でLocal/Globalの出所を見分け、同じ名前のCategoryでも判断できるようにする。
                        const std::string label_ = (entry_.isLocal ? "[L] " : "[G] ") + entry_.name;
                        if (ImGui::Selectable(label_.c_str(), action_.category == entry_.name)) {
                            action_.category = entry_.name;
                            action_.actionName.clear();
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::SetNextItemWidth(260.0f);
                if (!action_.category.empty()) {
                    const std::string actionLabel_ = action_.actionName.empty() ? "Select Action" : action_.actionName;
                    if (ImGui::BeginCombo("Action", actionLabel_.c_str())) {
                        std::vector<std::string> actionNames_;
                        if (owner && owner->localRegistry.HasCategory(action_.category)) {
                            // Localに同Categoryがある場合はOwner固有Actionを優先して表示する。
                            actionNames_ = owner->localRegistry.GetActionNames(action_.category);
                        }
                        else {
                            actionNames_ = globalReg.GetActionNames(action_.category);
                        }

                        for (const auto& actionName_ : actionNames_) {
                            if (ImGui::Selectable(actionName_.c_str(), action_.actionName == actionName_)) {
                                action_.actionName = actionName_;
                            }
                        }
                        ImGui::EndCombo();
                    }
                }
                else {
                    ImGui::TextDisabled("Select a category first.");
                }

                ImGui::SetNextItemWidth(180.0f);
                ImGui::InputText("Wait For Spawned Object", &action_.waitForObject);
                ImGui::InputText("Animation State (empty: real time)", &action_.animationState);
                if (!action_.animationState.empty()) ImGui::Checkbox("Normalized Animation Time", &action_.useNormalizedTime);
                const float maximumTime_ = !action_.animationState.empty() && action_.useNormalizedTime ? 1.0f : sequence_.duration;
                if (ImGui::DragFloat("Execution Time", &action_.executionTime, 0.01f, 0.0f, maximumTime_,
                    !action_.animationState.empty() && action_.useNormalizedTime ? "%.3f normalized" : "%.3f sec")) {
                    // 数値入力で変更した場合も、Timeline表示のFrameへ即反映する。
                    action_.executionTime = (std::clamp)(action_.executionTime, 0.0f, maximumTime_);
                    adapter_.SyncFrames();
                }

                // Blackboard WritesはAction呼び出し直前だけ書く一時パラメータ。
                // 同じActionをSequenceごとに違う値で使い回せるようにする。
                ImGui::SeparatorText("Blackboard Writes");
                int removeWriteIndex_ = -1;
                for (int writeIndex_ = 0;
                    writeIndex_ < static_cast<int>(action_.blackboardWrites.size());
                    ++writeIndex_)
                {
                    BlackboardValueWrite& write_ = action_.blackboardWrites[writeIndex_];
                    ImGui::PushID(writeIndex_);

                    char categoryBuffer_[64];
                    strcpy_s(categoryBuffer_, write_.category.c_str());
                    if (ImGui::InputText("Blackboard Category", categoryBuffer_, sizeof(categoryBuffer_)))
                    {
                        write_.category = categoryBuffer_;
                    }

                    char keyBuffer_[64];
                    strcpy_s(keyBuffer_, write_.key.c_str());
                    if (ImGui::InputText("Key", keyBuffer_, sizeof(keyBuffer_)))
                    {
                        write_.key = keyBuffer_;
                    }

                    int typeIndex_ = static_cast<int>(write_.type);
                    const char* typeNames_[] = { "Float", "Int", "Bool" };
                    if (ImGui::Combo("Type", &typeIndex_, typeNames_, 3))
                    {
                        // 保存時はEnum値として保持し、Guiでは分かりやすい型名に変換する。
                        write_.type = static_cast<BLACKBOARD_VALUE_TYPE>(typeIndex_);
                    }

                    switch (write_.type)
                    {
                    case BLACKBOARD_VALUE_TYPE::FLOAT:
                        ImGui::DragFloat("Value", &write_.floatValue, 0.1f);
                        break;
                    case BLACKBOARD_VALUE_TYPE::INT:
                        ImGui::DragInt("Value", &write_.intValue, 1.0f);
                        break;
                    case BLACKBOARD_VALUE_TYPE::BOOL:
                        ImGui::Checkbox("Value", &write_.boolValue);
                        break;
                    }

                    if (ImGui::Button("Remove Write")) removeWriteIndex_ = writeIndex_;
                    ImGui::Separator();
                    ImGui::PopID();
                }

                if (removeWriteIndex_ >= 0)
                {
                    action_.blackboardWrites.erase(
                        action_.blackboardWrites.begin() + removeWriteIndex_);
                }
                if (ImGui::Button("Add Blackboard Write"))
                {
                    action_.blackboardWrites.push_back(BlackboardValueWrite{});
                }
            }
        }
        ImGui::Separator();
        ImGui::PopID();
    }

    if (ImGui::Button("Add New Sequence", ImVec2(-1, 35))) {
        sequences.push_back(SequenceData());
    }
}


void ActionSequence::Serialize(nlohmann::json& j) const {
    // SequenceDataをJSONへ保存する。Actionの時刻は現在形式では絶対秒数として記録する。
    nlohmann::json seqArray = nlohmann::json::array();
    for (const auto& seq : sequences) {
        nlohmann::json seqItem;
        seqItem["name"] = seq.name;
        seqItem["duration"] = seq.duration;
        seqItem["completionAction"] = seq.completionAction;
        seqItem["cleanupObjectsOnCompletion"] = seq.cleanupObjectsOnCompletion;
        seqItem["interruptionAction"] = seq.interruptionAction;

        nlohmann::json actArray = nlohmann::json::array();
        for (const auto& act : seq.actions) {
            nlohmann::json actItem_ = {
                {"cat", act.category},
                {"name", act.actionName},
                {"time", act.executionTime}
            };

            nlohmann::json writesArray_ = nlohmann::json::array();
            for (const BlackboardValueWrite& write_ : act.blackboardWrites)
            {
                // Blackboardへ書く値は型ごとに使う欄が違うため、全型の値を保存してEditor復元を単純にする。
                writesArray_.push_back({
                    {"category", write_.category},
                    {"key", write_.key},
                    {"type", static_cast<int>(write_.type)},
                    {"floatValue", write_.floatValue},
                    {"intValue", write_.intValue},
                    {"boolValue", write_.boolValue}
                });
            }
            actItem_["blackboardWrites"] = writesArray_;
            actItem_["waitForObject"] = act.waitForObject;
            actItem_["animationState"] = act.animationState;
            actItem_["useNormalizedTime"] = act.useNormalizedTime;
            actArray.push_back(actItem_);
        }
        seqItem["actions"] = actArray;
        seqArray.push_back(seqItem);
    }
    j["Sequences"] = seqArray;
    j["RegisterGlobal"] = registerGlobal;
}

void ActionSequence::Deserialize(const nlohmann::json& j)
{
    // Scene読み込み時にSequenceを復元し、最後にRegistryへ再登録する。
    registerGlobal = j.value("RegisterGlobal", false);
    if (!j.contains("Sequences") || !j["Sequences"].is_array()) return;
    sequences.clear();

    for (const auto& seqItem : j["Sequences"]) {
        if (!seqItem.is_object()) continue;

        SequenceData data;
        data.name = seqItem.value("name", "New Sequence");
        data.duration = seqItem.value("duration", 10.0f);
        data.completionAction = seqItem.value("completionAction", std::string{});
        data.cleanupObjectsOnCompletion = seqItem.value("cleanupObjectsOnCompletion", false);
        data.interruptionAction = seqItem.value("interruptionAction", std::string{});

        if (seqItem.contains("actions") && seqItem["actions"].is_array()) {
            float accumulatedTime_ = 0.0f;
            for (const auto& actItem : seqItem["actions"]) {
                if (actItem.is_object()) {
                    ActionReference ref;
                    ref.category = actItem.value("cat", "");
                    ref.actionName = actItem.value("name", "");
                    ref.waitForObject = actItem.value("waitForObject", std::string{});
                    ref.animationState = actItem.value("animationState", std::string());
                    ref.useNormalizedTime = actItem.value("useNormalizedTime", false);
                    if (actItem.contains("time")) {
                        // 現在形式はSequence開始からの絶対秒数。
                        ref.executionTime = actItem.value("time", 0.0f);
                        accumulatedTime_ = ref.executionTime;
                    }
                    else {
                        // 旧形式はAction間の相対delay保存だったため、累積して絶対秒数へ変換する。
                        accumulatedTime_ += actItem.value("delay", 0.0f);
                        ref.executionTime = accumulatedTime_;
                    }
                    data.duration = (std::max)(data.duration, ref.executionTime);

                    if (actItem.contains("blackboardWrites") &&
                        actItem["blackboardWrites"].is_array())
                    {
                        for (const auto& writeItem_ : actItem["blackboardWrites"])
                        {
                            if (!writeItem_.is_object()) continue;

                            // 古いデータで値が足りなくても既定値で復元し、型番号は範囲内に丸める。
                            BlackboardValueWrite write_;
                            write_.category = writeItem_.value("category", std::string("Attack"));
                            write_.key = writeItem_.value("key", std::string("Damage"));
                            const int typeIndex_ = (std::clamp)(writeItem_.value("type", 0), 0, 2);
                            write_.type = static_cast<BLACKBOARD_VALUE_TYPE>(typeIndex_);
                            write_.floatValue = writeItem_.value("floatValue", 0.0f);
                            write_.intValue = writeItem_.value("intValue", 0);
                            write_.boolValue = writeItem_.value("boolValue", false);
                            ref.blackboardWrites.push_back(write_);
                        }
                    }

                    if (!ref.actionName.empty() || !ref.waitForObject.empty()) {
                        data.actions.push_back(ref);
                    }
                }
            }
        }
        sequences.push_back(data);
    }
    RegisterSequence("");
}

// 別のSequenceへ切り替わっている場合は、その予約や終了通知を消さない。
void ActionSequence::StopPlayback(unsigned int generation_)
{
    BS_PROFILE_SCOPE("ActionSequence::StopPlayback");
    if (generation_ != playbackGeneration) return;
    ++playbackGeneration;
    pendingActions.clear();
    pendingCompletionAction.clear();
    remainingDuration = 0.0f;
    CleanupSpawnedObjects(true);
    const auto abort_ = interruptionAction;
    interruptionAction.clear();
    if (!abort_.empty()) ExecuteAction(abort_);
}
