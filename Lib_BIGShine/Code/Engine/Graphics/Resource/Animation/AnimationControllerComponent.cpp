#include "AnimationControllerComponent.h"
#include "AnimSequenceAdapter.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "AnimationDataAsset.h"
#include "PoseLibraryResourceManager.h"

#include <imgui.h>
#include <imgui_stdlib.h> // std::string用
#include  <iostream >  // C++標準入出力
#include  <fstream >  // C++ファイル入出力


namespace ed = ax::NodeEditor;

// ベジェ曲線の中点に三角形の矢印を描画するヘルパー
static void DrawLinkArrow(ImDrawList* drawList, const ImVec2& p0, const ImVec2& p3, ImU32 color)
{
    if (p0.x == 0.0f && p0.y == 0.0f) return;
    if (p3.x == 0.0f && p3.y == 0.0f) return;

    float strength = std::fmax(std::abs(p3.x - p0.x) * 0.5f, 50.0f);
    ImVec2 p1 = { p0.x + strength, p0.y };
    ImVec2 p2 = { p3.x - strength, p3.y };

    // t=0.5 の点と接線を計算
    float w1 = 0.125f, w2 = 0.375f, w3 = 0.375f, w4 = 0.125f;
    ImVec2 mid = {
        w1 * p0.x + w2 * p1.x + w3 * p2.x + w4 * p3.x,
        w1 * p0.y + w2 * p1.y + w3 * p2.y + w4 * p3.y
    };
    ImVec2 tan = {
        -3 * 0.25f * p0.x + (3 * 0.25f - 6 * 0.25f) * p1.x + (6 * 0.25f - 3 * 0.25f) * p2.x + 3 * 0.25f * p3.x,
        -3 * 0.25f * p0.y + (3 * 0.25f - 6 * 0.25f) * p1.y + (6 * 0.25f - 3 * 0.25f) * p2.y + 3 * 0.25f * p3.y
    };
    float len = std::sqrt(tan.x * tan.x + tan.y * tan.y);
    if (len > 0.0001f) { tan.x /= len; tan.y /= len; }
    else { tan = { 1.0f, 0.0f }; }

    const float S = 10.0f;
    ImVec2 perp = { -tan.y, tan.x };
    ImVec2 pt1 = { mid.x + tan.x * S,                         mid.y + tan.y * S };
    ImVec2 pt2 = { mid.x - tan.x * S + perp.x * S * 0.7f,         mid.y - tan.y * S + perp.y * S * 0.7f };
    ImVec2 pt3 = { mid.x - tan.x * S - perp.x * S * 0.7f,         mid.y - tan.y * S - perp.y * S * 0.7f };

    drawList->AddTriangleFilled(pt1, pt2, pt3, color);
}


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

    // 【追加】モデルが存在する場合、その真名からポーズライブラリリソースを自動的に取得・錬成する
    if (modelComp)
    {

        std::string modelName = modelComp->GetModelName();

        if (!modelName.empty()) {
            // 世界にただ一つのマネージャーより、このモデル専用の書庫を受け取る
            m_poseLibrary = PoseLibraryResourceManager::Instance().GetOrCreateResource(modelName);
        }
    }

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

    nodeEditorConfig.SettingsFile = "Data/Editor/AnimationEditor.json";     // 自前保存なら任意。外部 json と二重管理を避ける
    nodeEditorConfig.DragButtonIndex = 0;        // 左ドラッグでノード移動
    nodeEditorConfig.SelectButtonIndex = 0;      // 左クリックで選択
    nodeEditorConfig.NavigateButtonIndex = 2;    // 中ドラッグで視点移動
    nodeEditorConfig.ContextMenuButtonIndex = 1; // 右クリックで背景メニュー

    nodeEditorContext = ed::CreateEditor(&nodeEditorConfig);
}

void AnimationControllerComponent::Update(float deltaTime)
{
    if (!modelComp)
    {
        return;
    }
    if (!curvecomp)
    {
        auto MyObject = GetGameObject().get();
        curvecomp = MyObject->GetComponent<CurveComponent>().get();
    }

    // Start時点でリソースが取得できておらず、後からモデルがロードされた場合の安全弁
    if (!m_poseLibrary && !modelComp->GetModelName().empty())
    {
        m_poseLibrary = PoseLibraryResourceManager::Instance().GetOrCreateResource(modelComp->GetModelName());
    }

    // 状態遷移の評価
    EvaluateGraphTransitions();

    //ModelAnimator::Updateを呼ぶ際、ポーズライブラリのリソースポインタを第三引数に授ける
    animator.Update(deltaTime, modelComp, curvecomp, m_poseLibrary);

    // ーーー 抽出されたルートモーションを、 Transform に反映する ーーー
    const AnimState* currentState = animator.GetCurrentState();
    if (currentState && currentState->rootMotion.enable)
    {
        auto delta = animator.GetRootMotionDelta();
        auto transform = GetGameObject()->transform;

        if (transform)
        {
            using namespace DirectX;

            // 1. 度数法のオイラー角をラジアンへと変換する
            float roll = XMConvertToRadians(transform->localRotation.x);
            float pitch = XMConvertToRadians(transform->localRotation.y);
            float yaw = XMConvertToRadians(transform->localRotation.z);

            // 2. 現在の回転から、一時的にクォータニオンの力を編み出す
            XMMATRIX currentRotMat = XMMatrixRotationRollPitchYaw(roll, pitch, yaw);
            SimpleMath::Quaternion currentRot = SimpleMath::Quaternion::CreateFromRotationMatrix(currentRotMat);

            // 3. キャラクターの向いている方向（クォータニオン）に合わせて、ローカルな歩みをワールドの力へ変換
            SimpleMath::Vector3 worldDeltaPos = SimpleMath::Vector3::Transform(delta.deltaPosition, currentRot);

            transform->localPosition.x += worldDeltaPos.x;
            transform->localPosition.y += worldDeltaPos.y;
            transform->localPosition.z += worldDeltaPos.z;

            // 5. アニメーションがもたらす回転の差分（クォータニオン）を掛け合わせ、新たなる魂の向きを計算
            SimpleMath::Quaternion nextRot = currentRot * delta.deltaRotation;

            // 6. 新たなる向きを、そなたの Transform が好む「YXZ順序のオイラー角」へと分解する
            XMFLOAT4 q = nextRot;
            float xsqr = q.x * q.x;

            // Pitch (Y軸周りの回転)
            float t0 = 2.0f * (q.w * q.y + q.x * q.z);
            float t1 = 1.0f - 2.0f * (q.y * q.y + xsqr);
            float nextPitch = atan2f(t0, t1);

            // Roll (X軸周りの回転)
            float t2 = 2.0f * (q.w * q.x - q.y * q.z);
            t2 = t2 > 1.0f ? 1.0f : (t2 < -1.0f ? -1.0f : t2);
            float nextRoll = asinf(t2);

            // Yaw (Z軸周りの回転)
            float t3 = 2.0f * (q.w * q.z + q.x * q.y);
            float t4 = 1.0f - 2.0f * (q.z * q.z + xsqr);
            float nextYaw = atan2f(t3, t4);

            // 8. 座標や回転が変わったことを世界（子階層）に通知する、汚れの印（Dirtyフラグ）を刻む
            transform->MarkDirty();
        }
    }

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

        // ーーー 【追加】ルートモーションの細かな設定エリア ーーー
        ImGui::Spacing();
        if (ImGui::TreeNode("Root Motion Settings"))
        {
            // ルートモーション全体の有効・無効切り替え
            ImGui::Checkbox("Enable Root Motion", &editingState->rootMotion.enable);

            if (editingState->rootMotion.enable)
            {
                ImGui::Indent(); // 視覚的に分かりやすくするためインデントを入れる

                // 位置（XYZ）の適応フラグ。##Pos をつけて一意のIDにする
                ImGui::Text("Apply Position:");
                ImGui::Checkbox("X##Pos", &editingState->rootMotion.applyPositionX); ImGui::SameLine();
                ImGui::Checkbox("Y##Pos", &editingState->rootMotion.applyPositionY); ImGui::SameLine();
                ImGui::Checkbox("Z##Pos", &editingState->rootMotion.applyPositionZ);

                // 回転（XYZ）の適応フラグ。##Rot をつけて一意のIDにする
                ImGui::Text("Apply Rotation:");
                ImGui::Checkbox("X##Rot", &editingState->rootMotion.applyRotationX); ImGui::SameLine();
                ImGui::Checkbox("Y##Rot", &editingState->rootMotion.applyRotationY); ImGui::SameLine();
                ImGui::Checkbox("Z##Rot", &editingState->rootMotion.applyRotationZ);

                ImGui::Unindent(); // インデントを戻す
            }

            ImGui::TreePop();
        }



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
        static char newVarName[64] = "";
        static int selectedType = 0; // 0:Float, 1:Int, 2:Bool, 3:Vector2
        const char* typeNames[] = { "Float", "Int", "Bool", "Vector2" };

        // --- 新規パラメータ追加エリア ---
        ImGui::SeparatorText("Add New Parameter");
        ImGui::SetNextItemWidth(150.0f);
        ImGui::InputText("Name##BB", newVarName, IM_ARRAYSIZE(newVarName));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        ImGui::Combo("Type##BB", &selectedType, typeNames, IM_ARRAYSIZE(typeNames));
        ImGui::SameLine();
        if (ImGui::Button("Add##BB"))
        {
            std::string name(newVarName);
            if (!name.empty() && blackboard.find(name) == blackboard.end())
            {
                switch (selectedType)
                {
                case 0: blackboard[name] = 0.0f;                                   break;
                case 1: blackboard[name] = 0;                                      break;
                case 2: blackboard[name] = false;                                  break;
                case 3: blackboard[name] = DirectX::SimpleMath::Vector2(0.0f, 0.0f); break;
                }
                newVarName[0] = '\0'; // 入力欄をクリア
            }
        }

        // --- 既存パラメータ 一覧・編集・削除 ---
        ImGui::SeparatorText("Parameters");

        if (blackboard.empty())
        {
            ImGui::TextDisabled("No parameters defined.");
        }
        else
        {
            std::string toDelete;
            for (auto& [key, val] : blackboard)
            {
                ImGui::PushID(key.c_str());

                // 変数名
                ImGui::Text("%-20s", key.c_str());
                ImGui::SameLine();

                // 型に応じた編集ウィジェット
                if (auto* f = std::get_if<float>(&val)) {
                    ImGui::SetNextItemWidth(100.0f);
                    ImGui::DragFloat("##v", f, 0.01f);
                }
                else if (auto* i = std::get_if<int>(&val)) {
                    ImGui::SetNextItemWidth(100.0f);
                    ImGui::DragInt("##v", i);
                }
                else if (auto* b = std::get_if<bool>(&val)) {
                    ImGui::Checkbox("##v", b);
                }
                else if (auto* v = std::get_if<DirectX::SimpleMath::Vector2>(&val)) {
                    ImGui::SetNextItemWidth(180.0f);
                    ImGui::DragFloat2("##v", &v->x, 0.01f);
                }

                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.1f, 0.1f, 1.0f));
                if (ImGui::Button("X##del")) { toDelete = key; }
                ImGui::PopStyleColor();

                ImGui::PopID();
            }
            if (!toDelete.empty()) { blackboard.erase(toDelete); }
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
    j["AssetPath"] = m_assetFilePath;

    // --- グラフノードの保存 ---
    nlohmann::json nodesArray = nlohmann::json::array();
    for (auto& node : graphNodes)
    {
        nlohmann::json n;
        n["stateName"] = node.stateName;
        n["id"]        = static_cast<uintptr_t>(node.id.Get());
        n["inputPin"]  = static_cast<uintptr_t>(node.inputPin.Get());
        n["outputPin"] = static_cast<uintptr_t>(node.outputPin.Get());
        // エディタ上の座標を保存
        auto pos = ed::GetNodePosition(node.id);
        n["posX"] = pos.x;
        n["posY"] = pos.y;
        nodesArray.push_back(n);
    }
    j["GraphNodes"] = nodesArray;

    // --- トランジション(リンク+条件)の保存 ---
    nlohmann::json transArray = nlohmann::json::array();
    for (auto& t : graphTransitions)
    {
        nlohmann::json tr;
        tr["from"]     = t.fromStateName;
        tr["to"]       = t.toStateName;
        tr["duration"] = t.duration;
        tr["id"]       = static_cast<uintptr_t>(t.id.Get());

        nlohmann::json condsArray = nlohmann::json::array();
        for (auto& c : t.conditions)
        {
            nlohmann::json cj;
            cj["type"]      = static_cast<int>(c.type);
            cj["param"]     = c.parameterName;
            cj["op"]        = static_cast<int>(c.op);
            cj["threshold"] = c.threshold;
            condsArray.push_back(cj);
        }
        tr["conditions"] = condsArray;
        transArray.push_back(tr);
    }
    j["GraphTransitions"] = transArray;
}

void AnimationControllerComponent::Deserialize(const nlohmann::json& j)
{
    if (j.contains("AssetPath")) {
        m_assetFilePath = j.value("AssetPath", "");
        LoadAsset(m_assetFilePath);
    }

    if (!modelComp) {
        modelComp = GetGameObject()->GetComponent<GltfModelCommponennt>().get();
    }

    if (modelComp) {
        const auto& anims = modelComp->GetAnimations();
        for (auto& [name, state] : states) {
            for (int i = 0; i < (int)anims.size(); ++i) {
                if (anims[i].name == name) {
                    state->animationIndex = i;
                    break;
                }
            }
        }
    }

    // --- グラフノードの復元 ---
    if (j.contains("GraphNodes") && j["GraphNodes"].is_array())
    {
        graphNodes.clear();
        uintptr_t maxId = 0;

        for (auto& n : j["GraphNodes"])
        {
            AnimGraphNode node;
            node.stateName = n.value("stateName", "");
            auto rawId = n.value("id", (uintptr_t)0);
            auto rawIn = n.value("inputPin", (uintptr_t)0);
            auto rawOut = n.value("outputPin", (uintptr_t)0);
            node.id = ed::NodeId(rawId);
            node.inputPin = ed::PinId(rawIn);
            node.outputPin = ed::PinId(rawOut);
            node.pos = ImVec2(n.value("posX", 0.0f), n.value("posY", 0.0f));
            graphNodes.push_back(node);

            maxId = (std::max)({ maxId, rawId, rawIn, rawOut });
        }

        // IDカウンタを復元済みIDの最大値より大きい値に更新
        // ※ GetNextID() が内部カウンタを持つ前提。以下を実装側に合わせて調整すること。
        // SetIDCounter(maxId + 1);
    }

    // --- トランジションの復元 ---
    if (j.contains("GraphTransitions") && j["GraphTransitions"].is_array())
    {
        graphTransitions.clear();
        uintptr_t maxId = 0;

        for (auto& tr : j["GraphTransitions"])
        {
            AnimTransition t;
            t.fromStateName = tr.value("from", "");
            t.toStateName = tr.value("to", "");
            t.duration = tr.value("duration", 0.2f);
            auto rawId = tr.value("id", (uintptr_t)0);
            t.id = ed::LinkId(rawId);
            maxId = (std::max)(maxId, rawId);

            if (tr.contains("conditions") && tr["conditions"].is_array())
            {
                for (auto& cj : tr["conditions"])
                {
                    AnimCondition c;
                    c.type = static_cast<ConditionType>(cj.value("type", 0));
                    c.parameterName = cj.value("param", "");
                    c.op = static_cast<ConditionOp>(cj.value("op", 0));
                    c.threshold = cj.value("threshold", 0.0f);
                    t.conditions.push_back(c);
                }
            }
            graphTransitions.push_back(t);
        }
    }

    m_needsPositionRestore = true;
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

AnimGraphNode* AnimationControllerComponent::FindNodeByName(const std::string& name)
{
    for (auto& node : graphNodes)
    {
        if (node.stateName == name)
        {
            return &node;
        }
    }
    return nullptr;
}


void AnimationControllerComponent::SetIDCounterFloor(uintptr_t minVal)
{
    if (m_NextID <= minVal)
        m_NextID = minVal + 1;
}
void AnimationControllerComponent::EvaluateGraphTransitions()
{
    const AnimState* currentState = animator.GetCurrentState();
    if (!currentState) return;

    for (auto& transition : graphTransitions)
    {
        // 始まりのノードが、現在のステートと一致するか確認
        if (transition.fromStateName == currentState->name) {

            // 全条件チェック
            bool isMet = true;
            for (auto& cond : transition.conditions) {
                if (!EvaluateCondition(cond)) {
                    isMet = false;
                    break;
                }
            }

            if (isMet) {
                // Trigger条件のBlackboard値をリセット
                for (auto& cond : transition.conditions)
                {
                    if (cond.type == ConditionType::Trigger)
                    {
                        auto it = blackboard.find(cond.parameterName);
                        if (it != blackboard.end())
                        {
                            it->second = false; // bool false でリセット
                        }
                    }
                }
                ChangeState(transition.toStateName, transition.duration);
                break;
            }
        }
    }
}
bool AnimationControllerComponent::EvaluateCondition(const AnimCondition& cond)
{
    // 1. アニメーション終了判定
    if (cond.type == ConditionType::AnimationEnd) {
        const auto* state = animator.GetCurrentState();
        if (!state) return false;

        float duration = modelComp->GetAnimationDuration(
            state->type == AnimStateType::Single ? state->animationIndex : state->animIndexA
        );
        return animator.GetCurrentFadeTime() >= duration;
    }

    // 2. 変数・トリガー判定
    if (blackboard.find(cond.parameterName) == blackboard.end()) return false;
    const auto& bbValue = blackboard.at(cond.parameterName);

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

    if (cond.type == ConditionType::Trigger) return currentVal >= 0.5f;

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
    if (!nodeEditorContext) return;

    ed::SetCurrentEditor(nodeEditorContext);

    ImGui::Begin("Animation Graph Editor");

    if (ImGui::Button("全表示 (Focus All)"))
        ed::NavigateToContent();

    ImGui::SameLine();
    ImGui::TextDisabled("左ドラッグ: ノード移動 / 中ドラッグ: 視点移動 / 右クリック: ノード追加");

    ed::Begin("Animation Graph", ImVec2(0.0f, 0.0f));


    // ピン座標記録マップ (矢印描画用)
    std::unordered_map<uintptr_t, ImVec2> pinPositions;

    // ================================================================
    // 1. ノードの描画
    // ================================================================
    for (auto& node : graphNodes)
    {
        const bool isActive = (node.stateName == _runtimeActiveStateName);

        // 背景色
        ed::PushStyleColor(ed::StyleColor_NodeBg,
            isActive ? ImVec4(0.1f, 0.4f, 0.6f, 1.0f)
            : ImVec4(0.2f, 0.2f, 0.2f, 1.0f));

        // アクティブ時: 枠線パルス
        if (isActive)
        {
            float pulse = (sinf((float)ImGui::GetTime() * 8.0f) + 1.0f) * 0.5f;
            ed::PushStyleColor(ed::StyleColor_NodeBorder,
                ImVec4(0.2f + pulse * 0.4f, 0.7f + pulse * 0.3f, 1.0f, 1.0f));
            ed::PushStyleVar(ed::StyleVar_NodeBorderWidth, 2.5f + pulse * 2.0f);
        }

        ed::BeginNode(node.id);

        // ステート名 (アクティブ時は>  付き)
        std::string label = isActive ? (">" + node.stateName) : node.stateName;
        if (isActive)
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s", label.c_str());
        else
            ImGui::TextUnformatted(label.c_str());

        // Input ピン
        ed::BeginPin(node.inputPin, ed::PinKind::Input);
        ImGui::Text("-> In");
        ed::EndPin();
        pinPositions[(uintptr_t)node.inputPin.Get()] = {
            ImGui::GetItemRectMin().x + ImGui::GetItemRectSize().x * 0.5f,
            ImGui::GetItemRectMin().y + ImGui::GetItemRectSize().y * 0.5f
        };

        ImGui::SameLine();

        // Output ピン
        ed::BeginPin(node.outputPin, ed::PinKind::Output);
        ImGui::Text("Out ->");
        ed::EndPin();
        pinPositions[(uintptr_t)node.outputPin.Get()] = {
            ImGui::GetItemRectMin().x + ImGui::GetItemRectSize().x * 0.5f,
            ImGui::GetItemRectMin().y + ImGui::GetItemRectSize().y * 0.5f
        };

        ed::EndNode();

        if (isActive)
        {
            ed::PopStyleVar();    // NodeBorderWidth
            ed::PopStyleColor();  // NodeBorder
        }
        ed::PopStyleColor(); // NodeBg

        // ロード直後の座標復元
        if (m_needsPositionRestore)
            ed::SetNodePosition(node.id, node.pos);
    }

    // 右クリックで追加したノードをその場に配置
    if (_nodeToPlaceHash != 0)
    {
        for (auto& n : graphNodes) {
            if ((uintptr_t)n.id.Get() == _nodeToPlaceHash) {
                ed::SetNodePosition(n.id, _newNodeSpawnPos);
                ed::SelectNode(n.id);
                break;
            }
        }
        _nodeToPlaceHash = 0;
    }

    // ================================================================
    // 2. リンクの描画 + 矢印のオーバーレイ
    // ================================================================
    for (auto& link : graphTransitions)
    {
        auto* startNode = FindNodeByName(link.fromStateName);
        auto* endNode = FindNodeByName(link.toStateName);
        if (startNode && endNode)
            ed::Link(link.id, startNode->outputPin, endNode->inputPin);
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    for (auto& link : graphTransitions)
    {
        auto* startNode = FindNodeByName(link.fromStateName);
        auto* endNode = FindNodeByName(link.toStateName);
        if (!startNode || !endNode) continue;

        auto srcKey = (uintptr_t)startNode->outputPin.Get();
        auto dstKey = (uintptr_t)endNode->inputPin.Get();
        if (pinPositions.count(srcKey) && pinPositions.count(dstKey))
        {
            ImU32 color = (link.fromStateName == _runtimeActiveStateName)
                ? IM_COL32(100, 200, 255, 255)
                : IM_COL32(200, 200, 200, 255);
            DrawLinkArrow(drawList, pinPositions[srcKey], pinPositions[dstKey], color);
        }
    }

    if (m_needsPositionRestore)
    {
        ed::NavigateToContent();
        m_needsPositionRestore = false;
    }

    // ================================================================
    // 3. リンク作成 (安全なピン判定 + 重複チェック)
    // ================================================================
    if (ed::BeginCreate())
    {
        auto getPinInfo = [&](uintptr_t pinId, bool& isInput, std::string& nodeName) -> bool {
            for (auto& n : graphNodes) {
                if ((uintptr_t)n.inputPin.Get() == pinId) { isInput = true;  nodeName = n.stateName; return true; }
                if ((uintptr_t)n.outputPin.Get() == pinId) { isInput = false; nodeName = n.stateName; return true; }
            }
            return false;
            };

        ed::PinId startPinId, endPinId;
        if (ed::QueryNewLink(&startPinId, &endPinId))
        {
            bool isStartInput = false, isEndInput = false;
            std::string srcName, dstName;

            if (getPinInfo((uintptr_t)startPinId.Get(), isStartInput, srcName) &&
                getPinInfo((uintptr_t)endPinId.Get(), isEndInput, dstName))
            {
                if (isStartInput == isEndInput)
                {
                    // Input同士 / Output同士は拒否
                    ed::RejectNewItem(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), 2.0f);
                }
                else
                {
                    // 逆方向ドラッグ対応
                    const std::string& finalFrom = isStartInput ? dstName : srcName;
                    const std::string& finalTo = isStartInput ? srcName : dstName;

                    if (ed::AcceptNewItem(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), 2.0f))
                    {
                        bool exists = false;
                        for (auto& t : graphTransitions)
                            if (t.fromStateName == finalFrom && t.toStateName == finalTo) { exists = true; break; }

                        if (!exists)
                            graphTransitions.push_back(AnimTransition{
                                finalFrom, finalTo, 0.2f, {}, ed::LinkId(GetNextID())
                                });
                    }
                }
            }
            else
            {
                ed::RejectNewItem(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), 2.0f);
            }
        }

        ed::PinId unusedPin;
        if (ed::QueryNewNode(&unusedPin)) ed::RejectNewItem();
    }
    ed::EndCreate();

    // ================================================================
    // 4. 削除 (ノード削除時は関連遷移も大掃除)
    // ================================================================
    if (ed::BeginDelete())
    {
        ed::LinkId linkId;
        while (ed::QueryDeletedLink(&linkId))
        {
            if (ed::AcceptDeletedItem())
            {
                graphTransitions.erase(
                    std::remove_if(graphTransitions.begin(), graphTransitions.end(),
                        [&](const auto& t) { return t.id == linkId; }),
                    graphTransitions.end());
            }
        }

        ed::NodeId nodeId;
        while (ed::QueryDeletedNode(&nodeId))
        {
            if (ed::AcceptDeletedItem())
            {
                std::string deletedName;
                for (auto& n : graphNodes)
                    if (n.id == nodeId) { deletedName = n.stateName; break; }

                if (!deletedName.empty())
                {
                    // 関連する遷移をすべて削除
                    graphTransitions.erase(
                        std::remove_if(graphTransitions.begin(), graphTransitions.end(),
                            [&](const auto& t) {
                                return t.fromStateName == deletedName || t.toStateName == deletedName;
                            }),
                        graphTransitions.end());

                    graphNodes.erase(
                        std::remove_if(graphNodes.begin(), graphNodes.end(),
                            [&](const auto& n) { return n.stateName == deletedName; }),
                        graphNodes.end());

                    if (editingState && editingState->name == deletedName)
                        editingState = nullptr;

                    RemoveState(deletedName);
                }
            }
        }
    }
    ed::EndDelete();

    // ================================================================
    // 5. 選択状態ハンドリング
    // ================================================================
    if (ed::HasSelectionChanged())
    {
        ed::NodeId selectedNodeId;
        if (ed::GetSelectedNodes(&selectedNodeId, 1) > 0)
        {
            auto it = std::find_if(graphNodes.begin(), graphNodes.end(),
                [&](const auto& n) { return n.id == selectedNodeId; });
            if (it != graphNodes.end() && states.count(it->stateName))
                editingState = states[it->stateName].get();
        }
    }

    // ================================================================
    // 5.5 リンク選択 → Transition Properties ウィンドウ
    // ================================================================
    static ed::LinkId selectedLinkId;
    if (ed::GetSelectedLinks(&selectedLinkId, 1) > 0)
    {
        auto it = std::find_if(graphTransitions.begin(), graphTransitions.end(),
            [&](const auto& t) { return t.id == selectedLinkId; });

        if (it != graphTransitions.end())
        {
            ImGui::Begin("Transition Properties");

            auto* fromNode = FindNodeByName(it->fromStateName);
            auto* toNode = FindNodeByName(it->toStateName);
            if (fromNode && toNode)
                ImGui::Text("Transition: %s -> %s", fromNode->stateName.c_str(), toNode->stateName.c_str());
            else
                ImGui::Text("Transition: (Invalid Nodes)");

            ImGui::Separator();
            ImGui::DragFloat("Fade Duration", &it->duration, 0.01f, 0.0f, 2.0f);

            ImGui::TextColored(ImVec4(0, 1, 1, 1), "Conditions");
            if (ImGui::Button("+ Add Condition"))
                it->conditions.push_back(AnimCondition());

            for (int i = 0; i < (int)it->conditions.size(); i++)
            {
                ImGui::PushID(i);
                auto& cond = it->conditions[i];

                const char* condTypes[] = { "Variable", "AnimationEnd", "Trigger" };
                int typeIdx = (int)cond.type;
                if (ImGui::Combo("Type", &typeIdx, condTypes, IM_ARRAYSIZE(condTypes)))
                    cond.type = (ConditionType)typeIdx;

                // パラメータ名: Blackboardが空でなければドロップダウン
                if (cond.type == ConditionType::Variable || cond.type == ConditionType::Trigger)
                {
                    if (!blackboard.empty())
                    {
                        std::vector<const char*> keys;
                        for (auto& [k, v] : blackboard) keys.push_back(k.c_str());
                        int curIdx = 0;
                        for (int k = 0; k < (int)keys.size(); k++)
                            if (cond.parameterName == keys[k]) { curIdx = k; break; }
                        ImGui::SetNextItemWidth(160.0f);
                        if (ImGui::Combo("Parameter", &curIdx, keys.data(), (int)keys.size()))
                            cond.parameterName = keys[curIdx];
                    }
                    else
                    {
                        ImGui::InputText("Parameter", &cond.parameterName);
                        ImGui::SameLine();
                        ImGui::TextDisabled("(Blackboardにパラメータを追加してください)");
                    }
                }

                if (cond.type == ConditionType::Variable)
                {
                    const char* ops[] = { "==", "!=", ">", "<" };
                    int opIdx = (int)cond.op;
                    ImGui::SetNextItemWidth(60);
                    if (ImGui::Combo("Op", &opIdx, ops, IM_ARRAYSIZE(ops)))
                        cond.op = (ConditionOp)opIdx;
                    ImGui::SameLine();
                    ImGui::DragFloat("Threshold", &cond.threshold, 0.1f);
                }

                if (ImGui::Button("Remove Condition"))
                    it->conditions.erase(it->conditions.begin() + i);

                ImGui::Separator();
                ImGui::PopID();
            }

            ImGui::End();
        }
    }

    // ================================================================
    // 6. 右クリックコンテキストメニュー
    // ================================================================
    ed::Suspend();
    if (ed::ShowBackgroundContextMenu()) {
        ImGui::OpenPopup("Create New Node");
        _newNodeSpawnPos = ed::ScreenToCanvas(ImGui::GetMousePos());
    }
    ed::Resume();

    ed::Suspend();
    if (ImGui::BeginPopup("Create New Node"))
    {
        ImGui::Text("Create State Node");
        ImGui::Separator();

        for (auto& [name, statePtr] : states)
        {
            auto it = std::find_if(graphNodes.begin(), graphNodes.end(),
                [&](const auto& n) { return n.stateName == name; });

            if (it == graphNodes.end())
            {
                if (ImGui::MenuItem(name.c_str()))
                {
                    AnimGraphNode newNode;
                    newNode.id = GetNextID();
                    newNode.stateName = name;
                    newNode.inputPin = GetNextID();
                    newNode.outputPin = GetNextID();
                    graphNodes.push_back(newNode);
                    _nodeToPlaceHash = (uintptr_t)newNode.id.Get();
                }
            }
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Create Empty State"))
        {
            std::string newName = "NewState_" + std::to_string(graphNodes.size());
            AddState(newName, 0); // graphNodes にも追加される

            for (auto& n : graphNodes) {
                if (n.stateName == newName) {
                    _nodeToPlaceHash = (uintptr_t)n.id.Get();
                    break;
                }
            }
        }
        ImGui::EndPopup();
    }
    ed::Resume();

    ed::End();
    ed::SetCurrentEditor(nullptr);
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
        {"sequencer", s.sequencerDataName},
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
        {"blendParamName", s.blendParamName},
        // Custom用 【追加】
        {"customAnimIndex", s.customAnimIndex},
        {"rootMotion", s.rootMotion},
    };
}

void from_json(const nlohmann::json& j, AnimState& s)
{
    s.sequencerDataName = j.value("sequencer", "");
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

    // Custom用 【追加】
    s.customAnimIndex = j.value("customAnimIndex", -1);
    s.rootMotion = j.value("rootMotion", RootMotionSettings{});
    if (j.contains("events") && j["events"].is_array()) {
        s.events = j.at("events").get<std::vector<AnimEvent>>();
    }
}