#include "AnimationControllerComponent.h"
#include "AnimSequenceAdapter.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include <imgui.h>
#include <imgui_stdlib.h> // std::string用



struct AnimEvent;
struct AnimState;
void from_json(const nlohmann::json& j, AnimEvent& e);
void to_json(nlohmann::json& j, const AnimEvent& e);
void from_json(const nlohmann::json& j, AnimState& s);
void to_json(nlohmann::json& j, const AnimState& s);

void AnimationControllerComponent::Start()
{
        auto MyObject = GetGameObject().get();
        modelComp = MyObject->GetComponent<GltfModelCommponennt>().get();
        if (!modelComp) return;

        const auto& animations = modelComp->GetAnimations();
        std::string objName = MyObject->GetName();

        for (int i = 0; i < (int)animations.size(); ++i)
        {
            const std::string& animName = animations[i].name;

            // --- 修正ポイント：既存のステートを探す ---
            auto it = states.find(animName);

            if (it == states.end()) {
                // Deserialize で読み込まれていない（新しいアニメーションなど）場合のみ新規作成
                auto newState = std::make_unique<AnimState>();
                newState->name = animName;
                newState->playbackSpeed = 1.0f;
                newState->isLoop = true;
                states[animName] = std::move(newState);
                it = states.find(animName);
            }

            // 共通の更新処理（Indexの紐付けなどは実行時に最新の状態に合わせる）
            auto& state = it->second;
            state->animationIndex = i;

            // ActionRegistryへの登録（これは毎回やってOK）
            std::string fullActionName = objName + "_" + animName;
            ActionRegistry::Instance().RegisterAction("Animation", fullActionName, [this, animName]() {
                this->ChangeState(animName);
                });

            registeredFullNames.push_back(fullActionName);
        }

        // デフォルトアニメーションの再生
        if (currentState) {
            // DeserializeでCurrentStateNameが復元されている場合はそれを使う
            animator.CrossFadeTo(currentState, 0.0f);
        }
        else if (!states.empty()) {
            ChangeState(animations[0].name, 0.0f);
        }
    }

void AnimationControllerComponent::Update(float deltaTime)
{
    if (!modelComp) return;

    // ポーズ計算とブレンド
    animator.Update(deltaTime, modelComp);

    // 計算された姿勢をモデルに書き戻し
    modelComp->SetNodePoses(animator.GetCurrentPose());

    // 行列の最終更新
    modelComp->UpdateTransform();
}

void AnimationControllerComponent::Gui()
{
    if (!modelComp) return;

    ImGui::Text("Model: %s", modelComp->GetModelName().c_str());

    // --- 新規ステート作成 UI (追加分) ---
    if (ImGui::TreeNode("Create New Custom State"))
    {
        static char newStateName[64] = "CustomState";
        static int selectedAnimIdx = 0;

        ImGui::InputText("State Name", newStateName, 64);

        const auto& anims = modelComp->GetAnimations();
        if (ImGui::BeginCombo("Base Animation", anims[selectedAnimIdx].name.c_str()))
        {
            for (int i = 0; i < (int)anims.size(); i++)
            {
                if (ImGui::Selectable(anims[i].name.c_str(), selectedAnimIdx == i)) {
                    selectedAnimIdx = i;
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Add New State"))
        {
            AddState(newStateName, selectedAnimIdx);
        }
        ImGui::TreePop();
    }

    ImGui::Separator();

    // --- 1. ステート一覧と切り替え (削除ボタン追加版) ---
    if (ImGui::TreeNode("States & Transitions"))
    {
        std::string targetToDelete = ""; // 削除予約用

        for (auto& [name, statePtr] : states)
        {
            ImGui::PushID(name.c_str()); // 名前被りによる挙動不備防止

            bool isCurrent = (currentState == statePtr.get());
            if (isCurrent) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0, 1));

            // ステート切り替えボタン
            if (ImGui::Button(name.c_str()))
            {
                ChangeState(name, 0.2f);
            }

            if (isCurrent) ImGui::PopStyleColor();

            ImGui::SameLine();
            ImGui::SetNextItemWidth(50.0f);
            ImGui::DragFloat("##spd", &statePtr->playbackSpeed, 0.05f, 0.0f, 5.0f, "x%.1f");

            ImGui::SameLine();
            ImGui::Checkbox("Loop", &statePtr->isLoop);

            // --- 削除ボタン (追加) ---
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.1f, 0.1f, 1.0f));
            if (ImGui::Button("X"))
            {
                targetToDelete = name;
            }
            ImGui::PopStyleColor();

            ImGui::PopID();

            if (!targetToDelete.empty()) break; // 削除が決定したらループを抜ける
        }

        // 実際の削除処理
        if (!targetToDelete.empty())
        {
            RemoveState(targetToDelete);
        }

        ImGui::TreePop();
    }

    ImGui::Separator();

    // --- ウィンドウ管理ボタン ---
    if (ImGui::Button(showSequencerWindow ? "Close Anim Editor" : "Open Anim Editor"))
    {
        showSequencerWindow = !showSequencerWindow;
    }

    // --- 2. アニメーションシーケンサー (ImSequencer統合・拡張版) ---
    // シーケンサー本体の呼び出し（フラグがtrueの時だけ）
    if (showSequencerWindow)
    {
        ShowSequencerEditor(); // 別関数に分けるとスッキリします
    }

    // --- 3. Blackboard (変数) の監視 ---
    if (ImGui::CollapsingHeader("Blackboard (Params)"))
    {
        for (auto& [key, val] : blackboard)
        {
            ImGui::PushID(key.c_str());
            ImGui::SetNextItemWidth(100.0f);
            ImGui::DragFloat(key.c_str(), &val, 0.1f);

            // 値の推移を簡易グラフ表示
            static std::map<std::string, std::vector<float>> history;
            history[key].push_back(val);
            if (history[key].size() > 50) history[key].erase(history[key].begin());
            ImGui::SameLine();
            ImGui::PlotLines("##graph", history[key].data(), (int)history[key].size(), 0, nullptr, -FLT_MAX, FLT_MAX, ImVec2(100, 20));

            ImGui::PopID();
        }
    }
}

void AnimationControllerComponent::ShowSequencerEditor()
{
    // ウィンドウの初期サイズを指定（初回起動時のみ有効）
    ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_FirstUseEver);

    // 「X」ボタンで閉じられるように第2引数にフラグのアドレスを渡す
    if (!ImGui::Begin("Animation Sequence Editor", &showSequencerWindow))
    {
        ImGui::End();
        return;
    }

    if (!currentState)
    {
        ImGui::TextDisabled("Select a state to edit events.");
    }
    else
    {
        // staticをやめるか、毎回ターゲットを明示的に更新する
        AnimSequenceAdapter adapter;
        adapter.targetState = const_cast<AnimState*>(currentState);
        adapter.duration = modelComp->GetAnimationDuration(currentState->animationIndex);
        adapter.fps = 60.0f;

        static int selectedEntry = -1;
        static int firstFrame = 0;
        static bool expanded = true;

        // animatorの再生時間（秒）をフレームに変換
        int currentFrame = (int)(animator.GetCurrentFadeTime() * adapter.fps);

        // 【重要】シーケンサーの高さを指定し、描画を強制する
        // 第1引数の adapter の前に (ImSequencer::SequenceInterface*)&adapter とキャストが必要な場合があります
        ImGui::PushItemWidth(-1);

        // シーケンサー本体の描画
        // 戻り値などで描画サイズが変わるため、Childなどの枠内で呼ぶのが安全です
        if (ImGui::BeginChild("SequencerRegion", ImVec2(0, 300), true))
        {
            if (ImSequencer::Sequencer(&adapter, &currentFrame, &expanded, &selectedEntry, &firstFrame,
                ImSequencer::SEQUENCER_EDIT_ALL | ImSequencer::SEQUENCER_ADD | ImSequencer::SEQUENCER_DEL))
            {
                // シーケンサー上で操作（ドラッグなど）があった場合、
                // 選択されているアイテムのフレームを秒に変換して書き戻す
                if (selectedEntry >= 0 && selectedEntry < (int)adapter.targetState->events.size())
                {
                    int* pStart, * pEnd;
                    adapter.Get(selectedEntry, &pStart, &pEnd, nullptr, nullptr);
                    auto& ev = adapter.targetState->events[selectedEntry];
                    ev.startTime = adapter.ToTime(*pStart);
                    ev.endTime = adapter.ToTime(*pEnd);
                }
            }
            ImGui::EndChild();
        }


        ImGui::PopItemWidth();

        // --- 詳細編集エリア (要素が選択されている時のみ表示) ---
        if (selectedEntry >= 0 && selectedEntry < (int)adapter.targetState->events.size())
        {
            // バーや名前をクリックした際、ここが描画される
            if (ImGui::BeginChild("EventDetails", ImVec2(0, 0), true))
            {
                auto& ev = adapter.targetState->events[selectedEntry];

                // --- 削除ボタンをヘッダー付近に追加 ---
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Editing Event #%d", selectedEntry);
                ImGui::SameLine();

                // 赤っぽい色で削除ボタンを配置
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
                if (ImGui::Button("Delete Event"))
                {
                    // アダプター経由、または直接 vector から削除
                    adapter.Del(selectedEntry);
                    selectedEntry = -1; // 削除したら選択を解除しないとクラッシュする
                    ImGui::PopStyleColor(2);
                    ImGui::EndChild();
                    ImGui::End(); // 削除直後は描画をスキップ（または return）
                    return;
                }
                ImGui::PopStyleColor(2);

                ImGui::SameLine(ImGui::GetWindowWidth() - 100);
                if (ImGui::Button("Deselect")) { selectedEntry = -1; }

                // 1. 表示ラベルの編集
                ImGui::SetNextItemWidth(200.0f);
                if (ImGui::InputText("Display Label", &ev.eventLabel)) {
                    // std::stringへの直接入力 (imgui_stdlib.h)
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(Name on timeline)");

                ImGui::Separator();

                // アクション選択用ラムダ
                auto ActionField = [&](const char* label, std::string& targetStr) {
                    ImGui::SetNextItemWidth(250.0f);
                    ImGui::InputText(label, &targetStr);

                    if (ImGui::BeginPopupContextItem(label)) {
                        for (auto& cat : ActionRegistry::Instance().GetCategories()) {
                            if (ImGui::BeginMenu(cat.c_str())) {
                                for (auto& actionName : ActionRegistry::Instance().GetActionNames(cat)) {
                                    if (ImGui::MenuItem(actionName.c_str())) {
                                        targetStr = actionName;
                                    }
                                }
                                ImGui::EndMenu();
                            }
                        }
                        ImGui::EndPopup();
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled("(?)");
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Right-click to browse actions");
                    };

                // 各種アクション設定
                ActionField("Start Action", ev.startAction);
                ActionField("Update Action", ev.updateAction);
                ActionField("End Action", ev.endAction);

                ImGui::Separator();

                // 時間の数値入力
                ImGui::PushItemWidth(120.0f);
                if (ImGui::DragFloat("Start (s)", &ev.startTime, 0.01f, 0.0f, ev.endTime)) {
                    ev.frameStart = adapter.ToFrame(ev.startTime);
                }
                ImGui::SameLine();
                if (ImGui::DragFloat("End (s)", &ev.endTime, 0.01f, ev.startTime, adapter.duration)) {
                    ev.frameEnd = adapter.ToFrame(ev.endTime);
                }
                ImGui::PopItemWidth();

                ImGui::EndChild();
            }
        }
        else
        {
            ImGui::TextDisabled("Click an event bar or name to edit details.");
        }

        ImGui::End();
    }
}

void AnimationControllerComponent::ChangeState(const std::string& stateName, float duration)
{
    auto it = states.find(stateName);
    if (it == states.end()) return;

    const AnimState* nextState = it->second.get();
    if (currentState == nextState) return;

    // Animatorクラスに遷移を命令
    animator.CrossFadeTo(nextState, duration);
    currentState = nextState;
}

void AnimationControllerComponent::Serialize(nlohmann::json& j) const
{
    nlohmann::json statesArray = nlohmann::json::array();
    for (auto const& [name, statePtr] : states) {
        if (!statePtr) continue;

        // AnimStateのto_jsonを明示的または暗黙的に呼び出す
        nlohmann::json s = *statePtr;
        statesArray.push_back(s);
    }
    j["States"] = statesArray;

    if (currentState) j["CurrentStateName"] = currentState->name;
    j["Blackboard"] = blackboard;
}

void AnimationControllerComponent::Deserialize(const nlohmann::json& j)
{
    if (j.contains("States") && j["States"].is_array()) {
        for (const auto& sJ : j["States"]) {
            std::string name = sJ.value("name", "");
            if (name.empty()) continue;

            // モデルの有無に関わらず、まずデータを受け取る器を作る
            if (states.find(name) == states.end()) {
                states[name] = std::make_unique<AnimState>();
            }
            // これで AnimEvent 配列などが正しく復元される
            from_json(sJ, *states[name]);
        }
    }

    // 2. あとでモデルが見つかった時のために、animationIndex などの整合性をとる
    if (!modelComp) {
        modelComp = GetGameObject()->GetComponent<GltfModelCommponennt>().get();
    }

    if (modelComp) {
        const auto& anims = modelComp->GetAnimations();
        for (auto& [name, state] : states) {
            // 名前が一致するアニメーションの Index を再設定する
            for (int i = 0; i < (int)anims.size(); ++i) {
                if (anims[i].name == name) {
                    state->animationIndex = i;
                    break;
                }
            }
        }
    }

    // --- 手順3: その他 ---
    if (j.contains("Blackboard"))
    {
        blackboard = j["Blackboard"].get<std::unordered_map<std::string, float>>();
    }

    if (j.contains("CurrentStateName"))
    {
        std::string currentName = j.value("CurrentStateName", "");
        if (!currentName.empty() && states.count(currentName))
        {
            const AnimState* nextState = states[currentName].get();
            currentState = nextState;
            // アニメーターの中身（ポインタとデータ）を強制同期
            animator.CrossFadeTo(nextState, 0.0f);
        }
    }
}

void AnimationControllerComponent::AddState(const std::string& stateName, int animIndex) 
{
    if (states.count(stateName)) return; // 重複チェック

    auto newState = std::make_unique<AnimState>();
    newState->name = stateName;
    newState->animationIndex = animIndex;
    newState->playbackSpeed = 1.0f;
    newState->isLoop = true;
    states[stateName] = std::move(newState);

    // アクションも登録
    std::string objName = GetGameObject()->GetName();
    std::string fullActionName = objName + "_" + stateName;
    ActionRegistry::Instance().RegisterAction("Animation", fullActionName, [this, stateName]() {
        this->ChangeState(stateName);
        });
}

void AnimationControllerComponent::RemoveState(const std::string& stateName)
{
    auto it = states.find(stateName);
    if (it == states.end()) return;

    // --- 安全策: 消そうとしているステートが現在再生中なら、他のステートへ逃がす ---
    if (currentState == it->second.get())
    {
        currentState = nullptr;
        // 他にステートがあれば最初のやつに、なければ停止
        if (states.size() > 1) {
            for (auto& [name, ptr] : states) {
                if (name != stateName) {
                    ChangeState(name, 0.0f);
                    break;
                }
            }
        }
    }

    // AddState で登録した時と同じ形式の名前を構築
    std::string objName = GetGameObject()->GetName();
    std::string fullActionName = objName + "_" + stateName;
    ActionRegistry::Instance().RemoveAction("Animation", fullActionName);

    // 削除実行
    states.erase(it);
}

// =============================================================
// JSON変換関数の実体
// =============================================================

void to_json(nlohmann::json& j, const AnimEvent& e) {
    j = nlohmann::json{
        {"label", e.eventLabel},
        {"startTime", e.startTime},
        {"endTime", e.endTime},
        {"actionStart", e.startAction},
        {"actionUpdate", e.updateAction},
        {"actionEnd", e.endAction}
    };
}

void from_json(const nlohmann::json& j, AnimEvent& e) {
    e.eventLabel = j.value("label", "");
    e.startTime = j.value("startTime", 0.0f);
    e.endTime = j.value("endTime", 0.0f);
    e.startAction = j.value("actionStart", "");
    e.updateAction = j.value("actionUpdate", "");
    e.endAction = j.value("actionEnd", "");
    e.isFired = false;
}

void to_json(nlohmann::json& j, const AnimState& s) {
    j = nlohmann::json
    {
        {"name", s.name},
        {"speed", s.playbackSpeed},
        {"loop", s.isLoop},
        {"events", s.events}
    };
}

void from_json(const nlohmann::json& j, AnimState& s) 
{
    s.name = j.value("name", s.name);
    s.playbackSpeed = j.value("speed", 1.0f);
    s.isLoop = j.value("loop", true);
    if (j.contains("events") && j["events"].is_array()) {
        // ここで AnimEvent の from_json が自動的に呼ばれる
        s.events = j.at("events").get<std::vector<AnimEvent>>();
    }
}