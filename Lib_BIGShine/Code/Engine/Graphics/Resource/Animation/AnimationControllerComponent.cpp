#include "AnimationControllerComponent.h"
#include "AnimSequenceAdapter.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "AnimationDataAsset.h"

#include <imgui.h>
#include <imgui_stdlib.h> // std::string用
#include  <iostream >  // C++標準入出力
#include  <fstream >  // C++ファイル入出力


namespace ed = ax::NodeEditor;


const std::string ASSET_DIR = "Data/Animation/StateData/";

// ヘルパー関数: パスを構築する
std::string GetFullAssetPath(const std::string& assetName) {
    // 拡張子 .json がなければ付与する
    std::string name = assetName;
    if (name.find(".json") == std::string::npos) {
        name += ".json";
    }
    return ASSET_DIR + name;
}


AnimationControllerComponent::~AnimationControllerComponent()
{
    if (nodeEditorContext) 
    {
        ed::DestroyEditor(nodeEditorContext);
        nodeEditorContext = nullptr;
    }
}

void AnimationControllerComponent::Start()
{
    auto MyObject = GetGameObject().get();
    modelComp = MyObject->GetComponent<GltfModelCommponennt>().get();
    // 同一オブジェクトからカーブコンポーネントを取得（取得できない場合は nullptr）

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

    // 設定が必要な場合は nodeEditorConfig を編集してから渡します
    nodeEditorContext =ed::CreateEditor(&nodeEditorConfig);
}

void AnimationControllerComponent::Update(float deltaTime)
{
    if (!modelComp) return;
    if (!curvecomp)
    {
        auto MyObject = GetGameObject().get();
        curvecomp = MyObject->GetComponent<CurveComponent>().get();
    }

    // 1. ノードグラフに基づく遷移判定
    EvaluateGraphTransitions();

    // ポーズ計算とブレンド
    animator.Update(deltaTime, modelComp, curvecomp);
    // 計算された姿勢をモデルに書き戻し
    modelComp->SetNodePoses(animator.GetCurrentPose());
    // 行列の最終更新
    modelComp->UpdateTransform();
}

void AnimationControllerComponent::Gui()
{
    if (!modelComp) return;

    ImGui::Text("Model: %s", modelComp->GetModelName().c_str());

    // --- ウィンドウ・エディタ管理ボタン ---
    ImGui::SeparatorText("Editor Controls");
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
    // ノードエディタ表示ボタン
    if (ImGui::Button(m_ShowNodeEditor ? "Hide Node Graph" : "Show Node Graph", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 0)))
    {
        m_ShowNodeEditor = !m_ShowNodeEditor;
    }
    ImGui::SameLine();
    // ここで前回作成したノードエディタ描画関数を呼ぶ（実装済みであれば）
    if (m_ShowNodeEditor) { OnGuiNodeEditor(); }


    // --- アセット管理セクション ---
    ImGui::SeparatorText("Animation Asset");
    // パス入力 (imgui_stdlib.h 使用)
    ImGui::InputText("Path", &m_assetFilePath);
    if (ImGui::Button("Save to File")) {SaveAsset(m_assetFilePath);}
    ImGui::SameLine();
    if (ImGui::Button("Load from File")) { LoadAsset(m_assetFilePath);}

    ImGui::Separator();


    // --- 準備：カーブ名の取得 ---
    std::vector<std::string> curveNames;
    if (curvecomp) {
        curveNames = curvecomp->GetTrackNames();
    }

    // --- 1. 新規ステート作成 (折りたたみ) ---
    if (ImGui::CollapsingHeader("Add New State", ImGuiTreeNodeFlags_None))
    {
        static char newStateName[64] = "CustomState";
        static int selectedAnimIdx = 0;
        ImGui::InputText("State Name", newStateName, 64);

        const auto& anims = modelComp->GetAnimations();
        if (ImGui::BeginCombo("Base Animation", anims[selectedAnimIdx].name.c_str()))
        {
            for (int i = 0; i < (int)anims.size(); i++)
            {
                if (ImGui::Selectable(anims[i].name.c_str(), selectedAnimIdx == i)) selectedAnimIdx = i;
            }
            ImGui::EndCombo();
        }
        if (ImGui::Button("Add State", ImVec2(-1, 0))) { AddState(newStateName, selectedAnimIdx); }
    }

    ImGui::Separator();

    // --- 2. ステート一覧 (選択用) ---
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Registered Animation States");

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
    // リスト領域を少し短くし、下に編集パネルのスペースを作る
    if (ImGui::BeginChild("StatesRegion", ImVec2(0, 200), true))
    {
        std::string targetToDelete = "";

        for (auto& [name, statePtr] : states)
        {
            ImGui::PushID(name.c_str());

            bool isCurrent = (animator.GetCurrentState() == statePtr.get());
            bool isEditing = (editingState == statePtr.get());

            // 実行中ステートの背景色変更
            if (isCurrent) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.4f, 0.2f, 1.0f));
            }

            // Selectable を使用して「現在選んでいる感」を出す
            // 左クリックで編集対象(editingState)を切り替え
            if (ImGui::Selectable(name.c_str(), isEditing, ImGuiSelectableFlags_AllowOverlap)) {
                editingState = statePtr.get();
            }

            if (isCurrent) ImGui::PopStyleColor();

            // 右側に操作ボタンを配置
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 65);
            if (ImGui::Button("Play")) { ChangeState(name, 0.2f); }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.1f, 0.1f, 1.0f));
            if (ImGui::Button("X")) { targetToDelete = name; }
            ImGui::PopStyleColor();

            ImGui::PopID();
        }

        if (!targetToDelete.empty()) {
            if (editingState && editingState->name == targetToDelete) editingState = nullptr;
            RemoveState(targetToDelete);
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // --- 3. 選択されたステートの詳細編集パネル ---
    ImGui::SeparatorText("State Properties");
    if (editingState)
    {
        // 編集中のステート名を表示
        ImGui::Text("Editing: %s", editingState->name.c_str());

        // --- ここに元の open の中にあった編集ロジックを集約 ---
        const char* typeNames[] = { "Single", "Blend1D" };
        int currentType = (int)editingState->type;
        if (ImGui::Combo("Type", &currentType, typeNames, IM_ARRAYSIZE(typeNames))) {
            editingState->type = (AnimStateType)currentType;
        }

        if (editingState->type == AnimStateType::Single) {
            const auto& anims = modelComp->GetAnimations();
            if (ImGui::BeginCombo("Animation", anims[editingState->animationIndex].name.c_str())) {
                for (int i = 0; i < (int)anims.size(); i++) {
                    if (ImGui::Selectable(anims[i].name.c_str(), editingState->animationIndex == i))
                        editingState->animationIndex = i;
                }
                ImGui::EndCombo();
            }
        }
        else if (editingState->type == AnimStateType::Blend1D) {
            const auto& anims = modelComp->GetAnimations();
            auto ComboAnim = [&](const char* label, int& targetIdx) {
                std::string cName = (targetIdx >= 0) ? anims[targetIdx].name : "None";
                if (ImGui::BeginCombo(label, cName.c_str())) {
                    for (int i = 0; i < (int)anims.size(); i++) {
                        if (ImGui::Selectable(anims[i].name.c_str(), targetIdx == i)) targetIdx = i;
                    }
                    ImGui::EndCombo();
                }
                };
            ComboAnim("Anim A", editingState->animIndexA);
            ComboAnim("Anim B", editingState->animIndexB);
            ImGui::InputText("Blend Param", &editingState->blendParamName);
        }

        ImGui::DragFloat("Speed", &editingState->playbackSpeed, 0.05f);
        ImGui::Checkbox("Loop", &editingState->isLoop);

        if (ImGui::Button("Edit Events in Sequencer", ImVec2(-1, 0))) {
            showSequencerWindow = true;
        }
    }
    else
    {
        ImGui::TextDisabled("Select a state from the list or node editor to edit.");
    }


    // --- 3. Blackboard (変数) の監視セクション ---
    if (ImGui::CollapsingHeader("Blackboard (Params)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // --- 変数の新規追加エリア ---
        static char newVarName[64] = "";
        static int selectedType = 0; // 0:Float, 1:Int, 2:Bool, 3:Vector2
        const char* typeNames[] = { "Float", "Int", "Bool", "Vector2" };

        ImGui::SeparatorText("Add New Parameter");
        ImGui::SetNextItemWidth(150.0f);
        ImGui::InputText("Name", newVarName, IM_ARRAYSIZE(newVarName));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        ImGui::Combo("Type", &selectedType, typeNames, IM_ARRAYSIZE(typeNames));
        ImGui::SameLine();

      
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

    if (!editingState)
    {
        ImGui::TextDisabled("Select a state to edit events.");
        ImGui::End();
        return;
        
    }
    else
    {
        // staticをやめるか、毎回ターゲットを明示的に更新する
        AnimSequenceAdapter adapter;
        adapter.targetState = const_cast<AnimState*>(editingState);
        adapter.duration = modelComp->GetAnimationDuration(editingState->animationIndex);
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
    // コンポーネント自体はパスだけを記録する
    j["AssetPath"] = m_assetFilePath;

    //nlohmann::json statesArray = nlohmann::json::array();
    //for (auto const& [name, statePtr] : states) {
    //    if (!statePtr) continue;

    //    // AnimStateのto_jsonを明示的または暗黙的に呼び出す
    //    nlohmann::json s = *statePtr;
    //    statesArray.push_back(s);
    //}
    //j["States"] = statesArray;

    //if (currentState) j["CurrentStateName"] = currentState->name;

}

void AnimationControllerComponent::Deserialize(const nlohmann::json& j)
{
    if (j.contains("AssetPath")) {
        m_assetFilePath = j.value("AssetPath", "");
        LoadAsset(m_assetFilePath);
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

    //if (j.contains("CurrentStateName"))
    //{
    //    std::string currentName = j.value("CurrentStateName", "");
    //    if (!currentName.empty() && states.count(currentName))
    //    {
    //        const AnimState* nextState = states[currentName].get();
    //        currentState = nextState;
    //        // アニメーターの中身（ポインタとデータ）を強制同期
    //        animator.CrossFadeTo(nextState, 0.0f);
    //    }
    //}
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

    // グラフ用ノードデータの作成
    AnimGraphNode node;
    node.id = ed::NodeId(GetNextID());
    node.stateName = stateName;
    node.inputPin = ed::PinId(GetNextID());
    node.outputPin = ed::PinId(GetNextID());
    node.pos = ImVec2(0, 0); // 初期座標

    graphNodes.push_back(node);
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

AnimGraphNode* AnimationControllerComponent::AFindNodeByPin(ed::PinId pinId)
{
    for (auto& node : graphNodes) 
    {
        // ノードが持つ入力ピンまたは出力ピンのIDが、探しているIDと一致するかチェック
        if (node.inputPin == pinId || node.outputPin == pinId) {
            return &node;
        }
    }
    return nullptr; // 見つからなかった場合
}


void AnimationControllerComponent::EvaluateGraphTransitions()
{
    // --- グラフによる遷移チェック ---
    const AnimState* currentState = animator.GetCurrentState();
    if (currentState)
    {
        for (auto& transition : graphTransitions)
        {
            // 現在のステートの出力ピンから出ているリンクか？
            auto* startNode = AFindNodeByPin(transition.startPin);
            if (startNode && startNode->stateName == currentState->name) {

                // 全ての条件を満たしているか判定
                bool isMet = true;
                for (auto& cond : transition.conditions) {
                    if (!EvaluateCondition(cond)) {
                        isMet = false;
                        break;
                    }
                }

                if (isMet) {
                    auto* endNode = AFindNodeByPin(transition.endPin);
                    ChangeState(endNode->stateName, transition.duration);
                    break;
                }
            }
        }
    }
}

bool AnimationControllerComponent::EvaluateCondition(const AnimCondition& cond)
{
    // --- 1. アニメーション終了判定 ---
    if (cond.type == ConditionType::AnimationEnd) {
        const auto* state = animator.GetCurrentState();
        float duration = modelComp->GetAnimationDuration(
            state->type == AnimStateType::Single ? state->animationIndex : state->animIndexA
        );
        // 現在の再生時間が長さ以上なら終了とみなす
        return animator.GetCurrentFadeTime() >= duration;
    }

    // --- 2. 変数・トリガー判定 ---
    if (blackboard.find(cond.parameterName) == blackboard.end()) return false;
    const auto& bbValue = blackboard[cond.parameterName];

    // variantから数値(float)として評価値を取り出すヘルパー
    auto ToFloat = [](const BlackBoardValue& v) -> float {
        return std::visit([](auto&& arg) -> float {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, float>) return arg;
            if constexpr (std::is_same_v<T, int>)   return (float)arg;
            if constexpr (std::is_same_v<T, bool>)  return arg ? 1.0f : 0.0f;
            if constexpr (std::is_same_v<T, DirectX::SimpleMath::Vector2>) return arg.Length();
            return 0.0f;
            }, v);
        };

    float currentVal = ToFloat(bbValue);

    // トリガーの場合は「true(1.0)であるか」のみを見る（演算子は基本無視）
    if (cond.type == ConditionType::Trigger) {
        return currentVal >= 0.5f;
    }

    // 通常の変数比較
    switch (cond.op) {
    case ConditionOp::Greater:  return currentVal > cond.threshold;
    case ConditionOp::Less:     return currentVal < cond.threshold;
    case ConditionOp::Equal:    return std::abs(currentVal - cond.threshold) < 0.001f;
    case ConditionOp::NotEqual: return std::abs(currentVal - cond.threshold) >= 0.001f;
    }

    return false;
}
void AnimationControllerComponent::LoadAsset(const std::string& assetName)
{
    std::string fullPath = GetFullAssetPath(assetName);

    std::ifstream file(fullPath);
    if (!file.is_open()) return;

    nlohmann::json j;
    file >> j;

    AnimationDataAsset asset;
    asset.Deserialize(j);

    states.clear();
    for (auto& s : asset.states) {
        states[s.name] = std::make_unique<AnimState>(s);
    }

    m_assetFilePath = assetName;

    if (!asset.defaultStateName.empty() && states.count(asset.defaultStateName)) 
    {
        ChangeState(asset.defaultStateName, 0.0f);
    }
}


void AnimationControllerComponent::SaveAsset(const std::string& assetName)
{
    // ディレクトリが存在しない場合は作成する
    if (!std::filesystem::exists(ASSET_DIR)) {
        std::filesystem::create_directories(ASSET_DIR);
    }

    std::string fullPath = GetFullAssetPath(assetName);

    AnimationDataAsset asset;
    for (auto& [name, ptr] : states) {
        asset.states.push_back(*ptr);
    }
    if (currentState) asset.defaultStateName = currentState->name;

    nlohmann::json j;
    asset.Serialize(j);

    std::ofstream file(fullPath);
    if (file.is_open()) {
        file << j.dump(4);
        m_assetFilePath = assetName; // エディタ上での表示用に名前を保存
    }

    //AnimationDataAsset asset;
    //for (auto& [name, ptr] : states) {
    //    asset.states.push_back(*ptr);
    //}
    //if (currentState) asset.defaultStateName = currentState->name;

    //nlohmann::json j;
    //asset.Serialize(j);

    //std::ofstream file(path);
    //file << j.dump(4);
    //m_assetFilePath = path;
}

void AnimationControllerComponent::OnGuiNodeEditor()
{
    ImGui::Begin("Animation Graph Editor");
    ed::SetCurrentEditor(nodeEditorContext);
    ed::Begin("Animation Graph");

    // 1. ノードの描画
    for (auto& node : graphNodes) {
        ed::BeginNode(node.id);
        ImGui::Text("%s", node.stateName.c_str());

        // 入力ピン（左側）
        ed::BeginPin(node.inputPin, ed::PinKind::Input);
        ImGui::Text("-> In");
        ed::EndPin();

        ImGui::SameLine();

        // 出力ピン（右側）
        ed::BeginPin(node.outputPin, ed::PinKind::Output);
        ImGui::Text("Out ->");
        ed::EndPin();
        ed::EndNode();
    }

    // 2. リンクの描画
    for (auto& link : graphTransitions) {
        ed::Link(link.id, link.startPin, link.endPin);
    }

    // 3. リンク作成のハンドリング
    if (ed::BeginCreate()) {
        ed::PinId startPinId, endPinId;
        if (ed::QueryNewLink(&startPinId, &endPinId)) {

            // AFindNodeByPin を使ってピンの属性をチェック
            auto startNode = AFindNodeByPin(startPinId);
            auto endNode = AFindNodeByPin(endPinId);

            // 自己接続防止 ＆ 常に「出力から入力へ」繋ぐように制限
            bool canConnect = (startNode != endNode);
            // ※ startPinId が出力ピン、endPinId が入力ピンであることを確認するのが理想的

            if (canConnect && ed::AcceptNewItem()) {
                graphTransitions.emplace_back(AnimTransition{
                    ed::LinkId(GetNextID()), startPinId, endPinId
                    });
            }
        }
    }
    ed::EndCreate();

    // 4. 削除のハンドリング
    if (ed::BeginDelete()) {
        ed::LinkId linkId;
        while (ed::QueryDeletedLink(&linkId)) {
            if (ed::AcceptDeletedItem()) {
                // graphTransitions から一致するIDを消す
                graphTransitions.erase(
                    std::remove_if(graphTransitions.begin(), graphTransitions.end(),
                        [&](const auto& t) { return t.id == linkId; }),
                    graphTransitions.end()
                );
            }
        }
    }
    ed::EndDelete();

    // 5. 選択状態のハンドリング
    if (ed::HasSelectionChanged()) {
        ed::NodeId selectedNodeId;
        // 選択されているノードが1つ以上ある場合
        if (ed::GetSelectedNodes(&selectedNodeId, 1) > 0) {
            // IDから該当する graphNodes を探す
            auto it = std::find_if(graphNodes.begin(), graphNodes.end(),
                [&](const auto& n) { return n.id == selectedNodeId; });

            if (it != graphNodes.end()) {
                // states マップから AnimState の生ポインタを取得して切り替え
                if (states.count(it->stateName)) {
                    editingState = states[it->stateName].get();

                    // シーケンサーを表示している場合は対象を更新
                    // ※アダプターにセットする関数などがあればここで呼ぶ
                    // animSequenceAdapter.targetState = m_EditingState;
                }
            }
        }
    }

    // --- 5.5 リンク選択のハンドリング ---
    static ed::LinkId selectedLinkId;
    if (ed::GetSelectedLinks(&selectedLinkId, 1) > 0) {
        // 選択されたリンク ID から遷移データを特定
        auto it = std::find_if(graphTransitions.begin(), graphTransitions.end(),
            [&](const auto& t) { return t.id == selectedLinkId; });

        if (it != graphTransitions.end()) {
            // ノードエディタのウィンドウとは別に、詳細編集用パネルを表示
            ImGui::Begin("Transition Properties");

            ImGui::Text("Transition: %s -> %s",
                AFindNodeByPin(it->startPin)->stateName.c_str(),
                AFindNodeByPin(it->endPin)->stateName.c_str());

            ImGui::Separator();
            ImGui::DragFloat("Fade Duration", &it->duration, 0.01f, 0.0f, 2.0f);

            // 条件の編集 (AnimCondition)
            ImGui::TextColored(ImVec4(0, 1, 1, 1), "Conditions");
            if (ImGui::Button("+ Add Condition")) {
                it->conditions.push_back(AnimCondition());
            }

            for (int i = 0; i < (int)it->conditions.size(); i++) {
                ImGui::PushID(i);
                auto& cond = it->conditions[i];

                // 条件タイプ (Variable か AnimationEnd か)
                const char* condTypes[] = { "Variable", "AnimationEnd", "Trigger" };
                int typeIdx = (int)cond.type;
                if (ImGui::Combo("Type", &typeIdx, condTypes, IM_ARRAYSIZE(condTypes))) {
                    cond.type = (ConditionType)typeIdx;
                }

                if (cond.type == ConditionType::Variable || cond.type == ConditionType::Trigger) {
                    ImGui::InputText("Parameter", &cond.parameterName);
                }

                if (cond.type == ConditionType::Variable) {
                    const char* ops[] = { "==", "!=", ">", "<" };
                    int opIdx = (int)cond.op;
                    ImGui::SetNextItemWidth(60);
                    if (ImGui::Combo("Op", &opIdx, ops, IM_ARRAYSIZE(ops))) {
                        cond.op = (ConditionOp)opIdx;
                    }
                    ImGui::SameLine();
                    ImGui::DragFloat("Threshold", &cond.threshold, 0.1f);
                }

                if (ImGui::Button("Remove Condition")) {
                    it->conditions.erase(it->conditions.begin() + i);
                }
                ImGui::Separator();
                ImGui::PopID();
            }

            ImGui::End();
        }
    }

    // 6. 右クリックコンテキストメニューのハンドリング
        // 背景をクリックしたときにポップアップを開く
    ed::Suspend();
    if (ed::ShowBackgroundContextMenu()) {
        ImGui::OpenPopup("Create New Node");
    }
    ed::Resume();

    // ポップアップの中身
    ed::Suspend();
    if (ImGui::BeginPopup("Create New Node")) {
        auto cursorCanvasPos = ed::ScreenToCanvas(ImGui::GetMousePosOnOpeningCurrentPopup());

        ImGui::Text("Create State Node");
        ImGui::Separator();

        // パターンA: 既存の登録済みステートから選んで配置
        // すでにグラフにあるものは除外するか、重複を許すかは運用次第
        for (auto& [name, statePtr] : states) {
            // すでにグラフに存在するかチェック
            auto it = std::find_if(graphNodes.begin(), graphNodes.end(),
                [&](const auto& n) { return n.stateName == name; });

            if (it == graphNodes.end()) { // まだグラフにないステートのみ表示
                if (ImGui::MenuItem(name.c_str())) {
                    // 新規ノードの構築
                    AnimGraphNode newNode;
                    newNode.id = GetNextID();
                    newNode.stateName = name;
                    newNode.inputPin = GetNextID();
                    newNode.outputPin = GetNextID();

                    graphNodes.push_back(newNode);
                }
            }
        }

        ImGui::Separator();
        // パターンB: まったく新しいステートを作成して配置
        if (ImGui::MenuItem("Create Empty State")) {
            std::string newName = "NewState_" + std::to_string(graphNodes.size());
            AddState(newName, 0); // 既存の AddState メソッドを利用

            AnimGraphNode newNode;
            newNode.id = GetNextID();
            newNode.stateName = newName;
            newNode.inputPin = GetNextID();
            newNode.outputPin = GetNextID();

            graphNodes.push_back(newNode);
            ed::SetNodePosition(newNode.id, cursorCanvasPos);
        }

        ImGui::EndPopup();
    }
    ed::Resume();

    ed::End();
    ImGui::End();
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
        {"type", (int)s.type}, // enumをintとして保存
        {"speed", s.playbackSpeed},
        {"loop", s.isLoop},
        {"speedCurveName", s.speedCurveName},
        {"events", s.events},
        // Single用
        {"animationIndex", s.animationIndex},
        // Blend1D用
        {"animIndexA", s.animIndexA},
        {"animIndexB", s.animIndexB},
        {"blendParamName", s.blendParamName}
    };
}

void from_json(const nlohmann::json& j, AnimState& s)
{
    s.name = j.value("name", "");
    s.type = (AnimStateType)j.value("type", (int)AnimStateType::Single);
    s.playbackSpeed = j.value("speed", 1.0f);
    s.isLoop = j.value("loop", true);
    s.speedCurveName = j.value("speedCurveName", "");

    // Single用復元
    s.animationIndex = j.value("animationIndex", -1);

    // Blend1D用復元
    s.animIndexA = j.value("animIndexA", -1);
    s.animIndexB = j.value("animIndexB", -1);
    s.blendParamName = j.value("blendParamName", "BlendAlpha");

    if (j.contains("events") && j["events"].is_array()) {
        s.events = j.at("events").get<std::vector<AnimEvent>>();
    }
}