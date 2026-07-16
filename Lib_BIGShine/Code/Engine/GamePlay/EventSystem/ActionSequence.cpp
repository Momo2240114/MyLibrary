#include "PrecompiledHeader.h"

#include "ActionSequence.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "ImSequencer.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float SEQUENCE_FPS = 60.0f;
    constexpr unsigned int ACTION_COLOR = 0xFF4FA3FF;

    struct ActionSequenceAdapter : public ImSequencer::SequenceInterface
    {
        SequenceData* sequence = nullptr;
        int* currentFrame = nullptr;

        int ToFrame(float time_) const
        {
            return static_cast<int>(std::round(time_ * SEQUENCE_FPS));
        }

        float ToTime(int frame_) const
        {
            return static_cast<float>(frame_) / SEQUENCE_FPS;
        }

        void SyncFrames()
        {
            if (!sequence) return;

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
            return action_.actionName.empty() ? "(Select Action)" : action_.actionName.c_str();
        }

        void Get(int index_, int** start_, int** end_, int* type_, unsigned int* color_) override
        {
            if (!sequence || index_ < 0 || index_ >= static_cast<int>(sequence->actions.size())) return;

            auto& action_ = sequence->actions[index_];
            if (start_) *start_ = &action_.timelineStartFrame;
            if (end_) *end_ = &action_.timelineEndFrame;
            if (type_) *type_ = 0;
            if (color_) *color_ = ACTION_COLOR;
        }

        void EndEdit() override
        {
            if (!sequence) return;

            for (auto& action_ : sequence->actions)
            {
                action_.timelineStartFrame = (std::max)(0, action_.timelineStartFrame);
                action_.timelineEndFrame = action_.timelineStartFrame + 1;
                action_.executionTime = ToTime(action_.timelineStartFrame);
                sequence->duration = (std::max)(sequence->duration, action_.executionTime);
            }
        }

        void Add(int typeIndex_) override
        {
            if (!sequence) return;

            ActionReference action_;
            action_.executionTime = currentFrame ? ToTime(*currentFrame) : 0.0f;
            sequence->actions.push_back(action_);
            SyncFrames();
        }

        void Del(int index_) override
        {
            if (!sequence || index_ < 0 || index_ >= static_cast<int>(sequence->actions.size())) return;
            sequence->actions.erase(sequence->actions.begin() + index_);
        }

        void Duplicate(int index_) override
        {
            if (!sequence || index_ < 0 || index_ >= static_cast<int>(sequence->actions.size())) return;

            auto action_ = sequence->actions[index_];
            action_.executionTime = (std::min)(sequence->duration, action_.executionTime + (1.0f / SEQUENCE_FPS));
            sequence->actions.push_back(action_);
            SyncFrames();
        }
    };
}

void ActionSequence::Start()
{
    RegisterSequence("");
}

void ActionSequence::Update(float elapsedTime_)
{
    for (auto iterator_ = pendingActions.begin(); iterator_ != pendingActions.end(); ) {
        iterator_->timer -= elapsedTime_;
        if (iterator_->timer <= 0.0f) {
            ApplyBlackboardWrites(iterator_->blackboardWrites);
            ExecuteAction(iterator_->actionName);
            iterator_ = pendingActions.erase(iterator_);
        }
        else {
            ++iterator_;
        }
    }
}


void ActionSequence::StartSequence(int index_)
{
    if (index_ < 0 || index_ >= (int)sequences.size()) return;

    pendingActions.clear();

    for (const ActionReference& action_ : sequences[index_].actions) {
        if (action_.actionName.empty()) continue;

        if (action_.executionTime <= 0.0f) {
            ApplyBlackboardWrites(action_.blackboardWrites);
            ExecuteAction(action_.actionName);
        }
        else {
            pendingActions.push_back({
                action_.executionTime,
                action_.actionName,
                action_.blackboardWrites });
        }
    }
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

    for (int index_ = 0; index_ < static_cast<int>(sequences.size()); ++index_) {
        if (sequences[index_].name.empty()) continue;

        const int sequenceIndex_ = index_;
        const std::string sequenceName_ = sequences[index_].name;
        const auto startSequence_ = [this, sequenceIndex_]() {
            StartSequence(sequenceIndex_);
            };
        localRegistry_.RegisterAction(
            "Sequence",
            sequenceName_,
            startSequence_);
        if (registerGlobal)
        {
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

void ActionSequence::ApplyBlackboardWrites(
    const std::vector<BlackboardValueWrite>& writes_)
{
    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_ || writes_.empty()) return;

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
    auto& globalReg = ActionRegistry::Instance();
    auto owner = gameObject.lock();

    ImGui::Checkbox("Register Global", &registerGlobal);
    ImGui::TextDisabled("OFF: Local only / ON: Local and global");
    ImGui::Separator();

    struct CategoryEntry { std::string name; bool isLocal; };
    std::vector<CategoryEntry> allCategories;

    if (owner) {
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
            if (ImGui::DragFloat("Timeline Length", &sequence_.duration, 0.1f, 0.1f, 600.0f, "%.2f sec")) {
                sequence_.duration = (std::max)(0.1f, sequence_.duration);
            }

            ImGui::SameLine();
            if (ImGui::Button("Sort by Start Time")) {
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
                if (ImGui::DragFloat("Execution Time", &action_.executionTime, 0.01f, 0.0f, sequence_.duration, "%.3f sec")) {
                    action_.executionTime = (std::clamp)(action_.executionTime, 0.0f, sequence_.duration);
                    adapter_.SyncFrames();
                }

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
    nlohmann::json seqArray = nlohmann::json::array();
    for (const auto& seq : sequences) {
        nlohmann::json seqItem;
        seqItem["name"] = seq.name;
        seqItem["duration"] = seq.duration;

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
    registerGlobal = j.value("RegisterGlobal", false);
    if (!j.contains("Sequences") || !j["Sequences"].is_array()) return;
    sequences.clear();

    for (const auto& seqItem : j["Sequences"]) {
        if (!seqItem.is_object()) continue;

        SequenceData data;
        data.name = seqItem.value("name", "New Sequence");
        data.duration = seqItem.value("duration", 10.0f);

        if (seqItem.contains("actions") && seqItem["actions"].is_array()) {
            float accumulatedTime_ = 0.0f;
            for (const auto& actItem : seqItem["actions"]) {
                if (actItem.is_object()) {
                    ActionReference ref;
                    ref.category = actItem.value("cat", "");
                    ref.actionName = actItem.value("name", "");
                    if (actItem.contains("time")) {
                        ref.executionTime = actItem.value("time", 0.0f);
                        accumulatedTime_ = ref.executionTime;
                    }
                    else {
                        // Legacy ActionSequence data stored relative delays.
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

                    if (!ref.actionName.empty()) {
                        data.actions.push_back(ref);
                    }
                }
            }
        }
        sequences.push_back(data);
    }
    RegisterSequence("");
}
