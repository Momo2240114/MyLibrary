#include "ActionSequence.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"

void ActionSequence::Start()
{
    RegisterSequence("");
}

void ActionSequence::Update(float et)
{
    auto owner = gameObject.lock();
    for (auto it = pendingActions.begin(); it != pendingActions.end(); ) {
        it->timer -= et;
        if (it->timer <= 0.0f) {

            bool executed = false;
            // 1. まず自分のLocalRegistryを探す
            if (owner && owner->localRegistry.HasAction(it->actionName)) {
                owner->localRegistry.Call(it->actionName);
                executed = true;
            }

            // 2. なければGlobalを探す
            if (!executed) {
                ActionRegistry::Instance().Call(it->actionName);
            }

            it = pendingActions.erase(it);
        }
        else {
            ++it;
        }
    }
}


void ActionSequence::StartSequence(int index)
{
    if (index < 0 || index >= (int)sequences.size()) return;

    pendingActions.clear();

    float accumulatedTime = 0.0f;
    for (const auto& act : sequences[index].actions) {
        accumulatedTime += act.delay;

        if (accumulatedTime <= 0.0f) {
            ActionRegistry::Instance().Call(act.actionName);
        }
        else {
            // Updateで処理するために予約
            pendingActions.push_back({ accumulatedTime, act.actionName });
        }
    }
}

void ActionSequence::RegisterSequence(const std::string& sequenceName)
{
    auto& reg = ActionRegistry::Instance();


    for (int i = 0; i < (int)sequences.size(); ++i) {
        if (sequences[i].name.empty()) continue;

        // インデックスをキャプチャして登録
        int seqIndex = i;
        reg.RegisterAction("Sequence", sequences[i].name, [this, seqIndex]() {
            this->StartSequence(seqIndex);
            });
    }
}

void ActionSequence::Gui()
{
    auto& globalReg = ActionRegistry::Instance();
    auto owner = gameObject.lock();

    // --- 追加: カテゴリリストの構築 (Local と Global を統合) ---
    struct CategoryEntry { std::string name; bool isLocal; };
    std::vector<CategoryEntry> allCategories;

    // 1. Local カテゴリを優先的に追加
    if (owner) {
        for (const auto& cat : owner->localRegistry.GetCategories()) {
            allCategories.push_back({ cat, true });
        }
    }
    // 2. Global カテゴリを追加
    for (const auto& cat : globalReg.GetCategories()) {
        allCategories.push_back({ cat, false });
    }

    for (int s = 0; s < (int)sequences.size(); ++s) {
        ImGui::PushID(s);
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap;

        if (ImGui::CollapsingHeader(sequences[s].name.empty() ? "Unnamed Sequence" : sequences[s].name.c_str(), nullptr, flags))
        {
            // --- 1. 名前入力 ---
            char buf[64];
            strcpy_s(buf, sequences[s].name.c_str());
            if (ImGui::InputText("Sequence Name", buf, sizeof(buf))) {
                sequences[s].name = buf;
            }

            ImGui::SameLine();
            if (ImGui::Button("Delete List")) {
                sequences.erase(sequences.begin() + s);
                ImGui::PopID(); --s; continue;
            }

            // --- 2. アクションスロット ---
            for (int i = 0; i < (int)sequences[s].actions.size(); ++i) {
                ImGui::PushID(i);
                auto& ref = sequences[s].actions[i];

                // A. カテゴリ選択 (Local/Global を判別表示)
                ImGui::SetNextItemWidth(160); // プレフィックス分少し広めに
                std::string catDisp = ref.category.empty() ? "Category" : ref.category;
                if (ImGui::BeginCombo("##cat", catDisp.c_str())) {
                    for (const auto& entry : allCategories) {
                        // ラベルに [L] や [G] を付けて区別する
                        std::string label = (entry.isLocal ? "[L] " : "[G] ") + entry.name;
                        if (ImGui::Selectable(label.c_str(), ref.category == entry.name)) {
                            ref.category = entry.name;
                            ref.actionName = ""; // リセット
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::SameLine();

                // B. アクション選択
                ImGui::SetNextItemWidth(200);
                if (!ref.category.empty()) {
                    std::string actDisp = ref.actionName.empty() ? "Select Action" : ref.actionName;
                    if (ImGui::BeginCombo("##act", actDisp.c_str())) {

                        // --- 修正: 選択されたカテゴリが Local か Global かを HasCategory で判断 ---
                        std::vector<std::string> actionNames;
                        if (owner && owner->localRegistry.HasCategory(ref.category)) {
                            actionNames = owner->localRegistry.GetActionNames(ref.category);
                        }
                        else {
                            actionNames = globalReg.GetActionNames(ref.category);
                        }

                        for (const auto& name : actionNames) {
                            if (ImGui::Selectable(name.c_str(), ref.actionName == name)) {
                                ref.actionName = name;
                            }
                        }
                        ImGui::EndCombo();
                    }
                }
                else {
                    ImGui::TextDisabled("(Select Category First)");
                }

                ImGui::SameLine();
                if (ImGui::Button("X")) {
                    sequences[s].actions.erase(sequences[s].actions.begin() + i);
                    --i;
                }

                ImGui::Text("delay");
                ImGui::SameLine();
                ImGui::DragFloat("##delay", &ref.delay, 0.1f, 0.0f, 100.0f);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Execution delay (seconds)");

                ImGui::PopID();
            }

            if (ImGui::Button("+ Add Slot")) {
                sequences[s].actions.push_back(ActionReference());
            }

            if (ImGui::Button("Register & Play", ImVec2(-1, 25))) {
                RegisterSequence("");
                StartSequence(s);
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

        nlohmann::json actArray = nlohmann::json::array();
        for (const auto& act : seq.actions) {
            // delay を保存に追加
            actArray.push_back({
                {"cat", act.category},
                {"name", act.actionName},
                {"delay", act.delay}
                });
        }
        seqItem["actions"] = actArray;
        seqArray.push_back(seqItem);
    }
    j["Sequences"] = seqArray;
}

void ActionSequence::Deserialize(const nlohmann::json& j)
{
    if (!j.contains("Sequences") || !j["Sequences"].is_array()) return;
    sequences.clear();

    for (const auto& seqItem : j["Sequences"]) {
        if (!seqItem.is_object()) continue;

        SequenceData data;
        data.name = seqItem.value("name", "New Sequence");

        if (seqItem.contains("actions") && seqItem["actions"].is_array()) {
            for (const auto& actItem : seqItem["actions"]) {
                if (actItem.is_object()) {
                    ActionReference ref;
                    ref.category = actItem.value("cat", "");
                    ref.actionName = actItem.value("name", "");
                    // delay を読み込みに追加（デフォルト 0.0f）
                    ref.delay = actItem.value("delay", 0.0f);

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