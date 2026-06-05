#include "AnimationControllerComponent.h"
#include "AnimSequenceAdapter.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "AnimationDataAsset.h"
#include "PoseLibraryResourceManager.h"

#include <imgui.h>
#include <imgui_stdlib.h> // std::string用
#include  <iostream >  // C++標準入出力
#include  <fstream >  // C++ファイル入出力
#include <algorithm>
#include <filesystem>
#include <unordered_map>
#include <cmath>

namespace ed = ax::NodeEditor;
static constexpr const char* START_NODE_NAME = "__Start__";
static constexpr const char* ANY_NODE_NAME = "__Any__";
static constexpr const char* ENTRY_NODE_PREFIX = "__Entry__:";
static constexpr const char* EXIT_NODE_PREFIX = "__Exit__:";

static std::string MakeEntryNodeName(const std::string& scopeId) { return std::string(ENTRY_NODE_PREFIX) + scopeId; }
static std::string MakeExitNodeName(const std::string& scopeId) { return std::string(EXIT_NODE_PREFIX) + scopeId; }

static const char* GetGraphNodeDisplayName(const AnimGraphNode& node)
{
    switch (node.type)
    {
    case AnimGraphNodeType::Start: return "Start";
    case AnimGraphNodeType::Any: return "Any";
    case AnimGraphNodeType::SubGraph: return node.stateName.c_str();
    case AnimGraphNodeType::Entry: return "Entry";
    case AnimGraphNodeType::Exit: return "Exit";
    default: return node.stateName.c_str();
    }
}

static bool IsSpecialGraphNode(const AnimGraphNode& node)
{
    return node.type == AnimGraphNodeType::Start || node.type == AnimGraphNodeType::Any ||
        node.type == AnimGraphNodeType::Entry || node.type == AnimGraphNodeType::Exit;
}

static bool IsSpecialGraphNodeName(const std::string& name)
{
    return name == START_NODE_NAME || name == ANY_NODE_NAME;
}

static ImU32 GetGraphNodeFillColor(const AnimGraphNode& node_, bool isActive_)
{
    if (isActive_)
        return IM_COL32(150, 220, 255, 255);

    switch (node_.type)
    {
    case AnimGraphNodeType::Start: return IM_COL32(120, 170, 245, 255);
    case AnimGraphNodeType::Any: return IM_COL32(185, 135, 235, 255);
    case AnimGraphNodeType::Entry: return IM_COL32(120, 205, 145, 255);
    case AnimGraphNodeType::Exit: return IM_COL32(240, 165, 105, 255);
    case AnimGraphNodeType::SubGraph: return IM_COL32(105, 195, 205, 255);
    default: return IM_COL32(215, 215, 215, 255);
    }
}

static ImU32 GetGraphNodeBorderColor(const AnimGraphNode& node_, bool isActive_)
{
    if (isActive_)
        return IM_COL32(45, 170, 255, 255);

    switch (node_.type)
    {
    case AnimGraphNodeType::Start: return IM_COL32(55, 125, 220, 255);
    case AnimGraphNodeType::Any: return IM_COL32(135, 90, 220, 255);
    case AnimGraphNodeType::Entry: return IM_COL32(55, 160, 95, 255);
    case AnimGraphNodeType::Exit: return IM_COL32(215, 120, 55, 255);
    case AnimGraphNodeType::SubGraph: return IM_COL32(45, 145, 155, 255);
    default: return IM_COL32(125, 125, 125, 255);
    }
}

static ImU32 GetGraphNodeTitleColor(const AnimGraphNode& node_)
{
    switch (node_.type)
    {
    case AnimGraphNodeType::Start: return IM_COL32(40, 90, 220, 255);
    case AnimGraphNodeType::Any: return IM_COL32(115, 70, 210, 255);
    case AnimGraphNodeType::Entry: return IM_COL32(35, 135, 75, 255);
    case AnimGraphNodeType::Exit: return IM_COL32(200, 90, 35, 255);
    case AnimGraphNodeType::SubGraph: return IM_COL32(20, 115, 125, 255);
    default: return IM_COL32(0, 0, 0, 255);
    }
}
// ベジェ曲線の中点に三角形の矢印を描画するヘルパー
static void DrawLinkArrow(ImDrawList* drawList, const ImVec2& p0, const ImVec2& p3, ImU32 color)
{
    if (p0.x == 0.0f && p0.y == 0.0f) return;
    if (p3.x == 0.0f && p3.y == 0.0f) return;

    float distance = std::sqrt((p3.x - p0.x) * (p3.x - p0.x) + (p3.y - p0.y) * (p3.y - p0.y));
    if (distance < 45.0f) return;

    float strength = std::fmax(std::abs(p3.x - p0.x) * 0.5f, 50.0f);
    ImVec2 p1 = { p0.x + strength, p0.y };
    ImVec2 p2 = { p3.x - strength, p3.y };

    auto evalBezier = [&](float t) -> ImVec2
        {
            float u = 1.0f - t;
            float uu = u * u;
            float tt = t * t;
            float uuu = uu * u;
            float ttt = tt * t;

            return ImVec2(
                uuu * p0.x + 3.0f * uu * t * p1.x + 3.0f * u * tt * p2.x + ttt * p3.x,
                uuu * p0.y + 3.0f * uu * t * p1.y + 3.0f * u * tt * p2.y + ttt * p3.y
            );
        };

    auto evalTangent = [&](float t) -> ImVec2
        {
            float u = 1.0f - t;

            ImVec2 tan(
                3.0f * u * u * (p1.x - p0.x) +
                6.0f * u * t * (p2.x - p1.x) +
                3.0f * t * t * (p3.x - p2.x),

                3.0f * u * u * (p1.y - p0.y) +
                6.0f * u * t * (p2.y - p1.y) +
                3.0f * t * t * (p3.y - p2.y)
            );

            float len = std::sqrt(tan.x * tan.x + tan.y * tan.y);
            if (len > 0.0001f)
            {
                tan.x /= len;
                tan.y /= len;
            }
            else
            {
                tan = ImVec2(1.0f, 0.0f);
            }

            return tan;
        };

    auto drawArrowAt = [&](float t)
        {
            ImVec2 center = evalBezier(t);
            ImVec2 tan = evalTangent(t);
            ImVec2 perp = ImVec2(-tan.y, tan.x);

            const float size = 14.0f;

            ImVec2 tip = ImVec2(center.x + tan.x * size, center.y + tan.y * size);
            ImVec2 backA = ImVec2(
                center.x - tan.x * size + perp.x * size * 0.75f,
                center.y - tan.y * size + perp.y * size * 0.75f
            );
            ImVec2 backB = ImVec2(
                center.x - tan.x * size - perp.x * size * 0.75f,
                center.y - tan.y * size - perp.y * size * 0.75f
            );

            drawList->AddTriangleFilled(tip, backA, backB, IM_COL32(255, 190, 40, 255));
            drawList->AddTriangle(tip, backA, backB, IM_COL32(70, 45, 0, 255), 2.0f);
        };


    drawArrowAt(0.75f);
    drawArrowAt(0.35f);
}

const std::string ASSET_DIR = "Data/Animation/StateData/";

// ヘルパー関数: パスを構築する
std::string GetFullAssetPath(const std::string& assetName) {
    // 拡張子 .json がなければ付与する
    std::string name = assetName;
    if (name.find(".json") == std::string::npos) {
        name += ".json";
    }

    return ASSET_DIR + assetName + "/" + name;
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

    if (!modelComp) return;

    std::string modelName = modelComp->GetModelName();
    if (!modelName.empty()) {
        m_poseLibrary = PoseLibraryResourceManager::Instance().GetOrCreateResource(modelName);
    }

    const auto& animations = modelComp->GetAnimations();
    std::string objName = MyObject->GetName();

    for (int i = 0; i < (int)animations.size(); ++i)
    {
        const std::string& animName = animations[i].name;

        auto it = states.find(animName);

        if (it == states.end()) {
            auto newState = std::make_unique<AnimState>();
            newState->name = animName;
            newState->playbackSpeed = 1.0f;
            newState->isLoop = true;
            states[animName] = std::move(newState);
            it = states.find(animName);
        }

        auto& state = it->second;
        state->animationIndex = i;

        std::string fullActionName = objName + "_" + animName;
        ActionRegistry::Instance().RegisterAction("Animation", fullActionName, [this, animName]() {
            this->ChangeState(animName);
            });

        registeredFullNames.push_back(fullActionName);
    }

    EnsureSpecialGraphNodes();

    if (currentState) {
        animator.CrossFadeTo(currentState, 0.0f);
    }
    else {
        TryEnterStartState();
        if (!currentState && !states.empty() && !animations.empty()) {
            ChangeState(animations[0].name, 0.0f);
        }
    }

    nodeEditorConfig.SettingsFile = "Data/Editor/AnimationEditor.json";
    nodeEditorConfig.DragButtonIndex = 0;
    nodeEditorConfig.SelectButtonIndex = 0;
    nodeEditorConfig.NavigateButtonIndex = 2;
    nodeEditorConfig.ContextMenuButtonIndex = 1;

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

    if (m_pendingGraphAssetLoad)
    {
        m_pendingGraphAssetLoad = false;

        if (!m_pendingGraphAssetPath.empty())
        {
            LoadGraphAsset(m_pendingGraphAssetPath);
            TryEnterStartState();
        }
    }

    if (m_editorPoseOverride)
    {
        modelComp->UpdateTransform();
        return;
    }


    // 状態遷移の評価
    EvaluateGraphTransitions();

    //ModelAnimator::Updateを呼ぶ際、ポーズライブラリのリソースポインタを第三引数に授ける
    animator.Update(deltaTime, modelComp, curvecomp, m_poseLibrary, GetGameObject());

    const AnimState* currentState = animator.GetCurrentState();
    if (currentState && currentState->rootMotion.enable)
    {
        auto delta = animator.GetRootMotionDelta();
        auto transform = GetGameObject()->transform;

        if (transform)
        {
            using namespace DirectX;
            using namespace DirectX::SimpleMath;

            // localRotation は degree 前提
            float xRad = XMConvertToRadians(transform->localRotation.x);
            float yRad = XMConvertToRadians(transform->localRotation.y);
            float zRad = XMConvertToRadians(transform->localRotation.z);

            // YXZ 運用なら yaw(Y), pitch(X), roll(Z) の順で作る
            Quaternion currentRot = Quaternion::CreateFromYawPitchRoll(yRad, xRad, zRad);
            currentRot.Normalize();

            // 位置 RootMotion
            Vector3 worldDeltaPos = Vector3::Transform(delta.deltaPosition, currentRot);
            transform->localPosition.x += worldDeltaPos.x;
            transform->localPosition.y += worldDeltaPos.y;
            transform->localPosition.z += worldDeltaPos.z;

            // 回転 RootMotion
            Quaternion nextRot = currentRot * delta.deltaRotation;
            nextRot.Normalize();

            XMFLOAT4 q = nextRot;

            float xsqr = q.x * q.x;

            // Y
            float t0 = 2.0f * (q.w * q.y + q.x * q.z);
            float t1 = 1.0f - 2.0f * (q.y * q.y + xsqr);
            float nextY = atan2f(t0, t1);

            // X
            float t2 = 2.0f * (q.w * q.x - q.y * q.z);
            t2 = (std::max)(-1.0f, (std::min)(1.0f, t2));
            float nextX = asinf(t2);

            // Z
            float t3 = 2.0f * (q.w * q.z + q.x * q.y);
            float t4 = 1.0f - 2.0f * (q.z * q.z + xsqr);
            float nextZ = atan2f(t3, t4);

            // ← これが今のコードに足りていない
            transform->localRotation.x = XMConvertToDegrees(nextX);
            transform->localRotation.y = XMConvertToDegrees(nextY);
            transform->localRotation.z = XMConvertToDegrees(nextZ);

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
    ImGui::TextDisabled("Animation editors are opened from the main Window menu.");

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
        const char* typeNames[] = { "Single", "Blend1D", "Custom" };
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
        else if (editingState->type == AnimStateType::Custom) {
            if (m_poseLibrary && !m_poseLibrary->customAnims.empty()) {
                int selectedCustom = editingState->customAnimIndex;
                if (selectedCustom < 0 || selectedCustom >= (int)m_poseLibrary->customAnims.size()) selectedCustom = 0;
                const std::string& previewName = m_poseLibrary->customAnims[selectedCustom].name;
                if (ImGui::BeginCombo("Custom Animation", previewName.c_str())) {
                    for (int i = 0; i < (int)m_poseLibrary->customAnims.size(); ++i) {
                        if (ImGui::Selectable(m_poseLibrary->customAnims[i].name.c_str(), selectedCustom == i)) {
                            editingState->customAnimIndex = i;
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            else {
                ImGui::TextDisabled("Create a custom animation in Pose Animation Creator.");
            }
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
        ImGui::TextDisabled("Use Window > Animation Sequence to edit events in the sequencer.");
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
                auto& globalReg = ActionRegistry::Instance();
                auto owner = GetGameObject();

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

                auto ActionField = [&](const char* label, std::string& targetStr) {
                    ImGui::SetNextItemWidth(250.0f);
                    ImGui::InputText(label, &targetStr);

                    ImGui::SameLine();
                    std::string popupName = std::string("ActionSelect##") + label;
                    if (ImGui::Button((std::string("Select##") + label).c_str())) {
                        ImGui::OpenPopup(popupName.c_str());
                    }

                    if (ImGui::BeginPopup(popupName.c_str())) {
                        if (ImGui::MenuItem("Clear")) {
                            targetStr.clear();
                        }
                        ImGui::Separator();

                        for (const auto& entry : allCategories) {
                            std::string menuLabel = (entry.isLocal ? "[L] " : "[G] ") + entry.name;
                            if (ImGui::BeginMenu(menuLabel.c_str())) {
                                std::vector<std::string> actionNames;
                                if (entry.isLocal && owner) {
                                    actionNames = owner->localRegistry.GetActionNames(entry.name);
                                }
                                else {
                                    actionNames = globalReg.GetActionNames(entry.name);
                                }

                                for (const auto& actionName : actionNames) {
                                    if (ImGui::MenuItem(actionName.c_str())) {
                                        targetStr = actionName;
                                    }
                                }
                                ImGui::EndMenu();
                            }
                        }
                        ImGui::EndPopup();
                    }
                };

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
    j["GraphAssetPath"] = m_graphAssetFilePath;
}

void AnimationControllerComponent::Deserialize(const nlohmann::json& j)
{
    m_assetFilePath = j.value("AssetPath", "");
    m_graphAssetFilePath = j.value("GraphAssetPath", "");

    if (!m_assetFilePath.empty())
    {
        LoadAsset(m_assetFilePath);
    }

    graphNodes.clear();
    graphTransitions.clear();

    if (!m_graphAssetFilePath.empty())
    {
        m_pendingGraphAssetPath = m_graphAssetFilePath;
        m_pendingGraphAssetLoad = true;
    }
    else
    {
        EnsureSpecialGraphNodes();
    }

    TryEnterStartState();
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
    node.type = AnimGraphNodeType::State;

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


void AnimationControllerComponent::EnsureSpecialGraphNodes()
{
    auto ensureNode = [&](AnimGraphNodeType type, const char* internalName, const ImVec2& defaultPos)
    {
        auto it = std::find_if(graphNodes.begin(), graphNodes.end(),
            [&](const AnimGraphNode& node) { return node.type == type || node.stateName == internalName; });

        if (it == graphNodes.end())
        {
            AnimGraphNode node;
            node.id = ed::NodeId(GetNextID());
            node.stateName = internalName;
            node.inputPin = ed::PinId(0);
            node.outputPin = ed::PinId(GetNextID());
            node.pos = defaultPos;
            node.type = type;
            node.scopeId = "Root";
            graphNodes.push_back(node);
            return;
        }

        it->stateName = internalName;
        it->type = type;
        it->scopeId = "Root";
        if ((uintptr_t)it->id.Get() == 0)
            it->id = ed::NodeId(GetNextID());
        it->inputPin = ed::PinId(0);
        if ((uintptr_t)it->outputPin.Get() == 0)
            it->outputPin = ed::PinId(GetNextID());
    };

    ensureNode(AnimGraphNodeType::Start, START_NODE_NAME, ImVec2(-360.0f, -80.0f));
    ensureNode(AnimGraphNodeType::Any, ANY_NODE_NAME, ImVec2(-360.0f, 140.0f));
}

void AnimationControllerComponent::TryEnterStartState()
{
    for (auto& transition : graphTransitions)
    {
        if (transition.fromStateName != START_NODE_NAME)
            continue;

        auto* toNode = FindNodeByName(transition.toStateName);
        if (toNode && toNode->type == AnimGraphNodeType::SubGraph)
        {
            if (TryEnterSubGraph(*toNode, 0.0f))
                return;
        }

        if (states.count(transition.toStateName) == 0)
            continue;

        ChangeState(transition.toStateName, 0.0f);
        return;
    }
}

bool AnimationControllerComponent::TryEnterSubGraph(const AnimGraphNode& subGraphNode, float duration)
{
    if (subGraphNode.type != AnimGraphNodeType::SubGraph || subGraphNode.childScopeId.empty())
        return false;

    const std::string entryName = MakeEntryNodeName(subGraphNode.childScopeId);
    for (auto& transition : graphTransitions)
    {
        if (transition.scopeId != subGraphNode.childScopeId)
            continue;
        if (transition.fromStateName != entryName)
            continue;
        if (states.count(transition.toStateName) == 0)
            continue;

        ChangeState(transition.toStateName, duration);
        return true;
    }

    return false;
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
    const AnimState* activeState = animator.GetCurrentState();
    if (!activeState)
    {
        TryEnterStartState();
        activeState = animator.GetCurrentState();
    }
    if (!activeState) return;

    _runtimeActiveStateName = activeState->name;

    auto areConditionsMet = [&](AnimTransition& transition) -> bool
        {
            bool isMet = transition.conditions.empty();
            if (!transition.conditions.empty())
            {
                isMet = (transition.logic == TransitionLogic::And);
                for (auto& cond : transition.conditions)
                {
                    const bool conditionMet = EvaluateCondition(cond);
                    if (transition.logic == TransitionLogic::And && !conditionMet)
                    {
                        isMet = false;
                        break;
                    }
                    if (transition.logic == TransitionLogic::Or && conditionMet)
                    {
                        isMet = true;
                        break;
                    }
                }
            }
            return isMet;
        };

    auto resetTriggerConditions = [&](AnimTransition& transition)
        {
            for (auto& cond : transition.conditions)
            {
                if (cond.type == ConditionType::Trigger)
                {
                    auto it = blackboard.find(cond.parameterName);
                    if (it != blackboard.end())
                    {
                        it->second = false;
                    }
                }
            }
        };

    auto enterTransitionTarget = [&](AnimTransition& transition) -> bool
        {
            auto* toNode = FindNodeByName(transition.toStateName);
            if (toNode && toNode->type == AnimGraphNodeType::SubGraph)
            {
                if (TryEnterSubGraph(*toNode, transition.duration))
                {
                    return true;
                }
                return false;
            }

            if (states.count(transition.toStateName) == 0)
                return false;

            ChangeState(transition.toStateName, transition.duration);
            return true;
        };

    auto exitSubGraph = [&](AnimTransition& exitTransition) -> bool
        {
            AnimGraphNode* ownerSubGraph = nullptr;
            for (auto& node : graphNodes)
            {
                if (node.type == AnimGraphNodeType::SubGraph && node.childScopeId == exitTransition.scopeId)
                {
                    ownerSubGraph = &node;
                    break;
                }
            }
            if (!ownerSubGraph)
                return false;

            for (auto& parentTransition : graphTransitions)
            {
                if (parentTransition.fromStateName != ownerSubGraph->stateName)
                    continue;
                if (!areConditionsMet(parentTransition))
                    continue;

                resetTriggerConditions(parentTransition);
                return enterTransitionTarget(parentTransition);
            }

            return false;
        };

    for (auto& transition : graphTransitions)
    {
        const bool fromCurrent = (transition.fromStateName == activeState->name);
        const bool fromAny = (transition.fromStateName == ANY_NODE_NAME);

        if (!fromCurrent && !fromAny)
            continue;
        if (transition.toStateName == activeState->name)
            continue;

        auto* toNode = FindNodeByName(transition.toStateName);
        const bool toExit = toNode && toNode->type == AnimGraphNodeType::Exit;
        const bool toSubGraph = toNode && toNode->type == AnimGraphNodeType::SubGraph;
        if (!toExit && !toSubGraph && states.count(transition.toStateName) == 0)
            continue;

        if (!areConditionsMet(transition))
            continue;

        resetTriggerConditions(transition);

        if (toExit)
        {
            if (exitSubGraph(transition))
                break;
            continue;
        }

        if (enterTransitionTarget(transition))
            break;
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

const std::string GRAPH_ASSET_DIR = "Data/Animation/GraphData/";

static std::string GetFullGraphAssetPath(const std::string& assetName)
{
    std::string name = assetName;
    if (name.find(".json") == std::string::npos) {
        name += ".json";
    }
    return GRAPH_ASSET_DIR + name;
}

void AnimationControllerComponent::SaveGraphAsset(const std::string& assetName)
{
    if (assetName.empty()) return;

    if (!std::filesystem::exists(GRAPH_ASSET_DIR)) {
        std::filesystem::create_directories(GRAPH_ASSET_DIR);
    }

    nlohmann::json j;
    j["AssetPath"] = m_assetFilePath;
    j["GraphAssetPath"] = assetName;

    nlohmann::json nodesArray = nlohmann::json::array();
    for (auto& node : graphNodes)
    {
        nlohmann::json n;
        n["stateName"] = node.stateName;
        n["id"] = static_cast<uintptr_t>(node.id.Get());
        n["inputPin"] = static_cast<uintptr_t>(node.inputPin.Get());
        n["outputPin"] = static_cast<uintptr_t>(node.outputPin.Get());
        n["nodeType"] = static_cast<int>(node.type);
        n["scopeId"] = node.scopeId;
        n["childScopeId"] = node.childScopeId;

        ImVec2 pos = node.pos;
        const std::string nodeScope = node.scopeId.empty() ? std::string("Root") : node.scopeId;
        const std::string currentScope = m_currentGraphScopeId.empty() ? std::string("Root") : m_currentGraphScopeId;
        if (nodeEditorContext && nodeScope == currentScope)
        {
            ed::SetCurrentEditor(nodeEditorContext);
            pos = ed::GetNodePosition(node.id);
        }

        n["posX"] = pos.x;
        n["posY"] = pos.y;

        nodesArray.push_back(n);
    }
    j["GraphNodes"] = nodesArray;

    nlohmann::json transArray = nlohmann::json::array();
    for (auto& t : graphTransitions)
    {
        nlohmann::json tr;
        tr["from"] = t.fromStateName;
        tr["to"] = t.toStateName;
        tr["duration"] = t.duration;
        tr["logic"] = static_cast<int>(t.logic);
        tr["id"] = static_cast<uintptr_t>(t.id.Get());
        tr["scopeId"] = t.scopeId;

        nlohmann::json condsArray = nlohmann::json::array();
        for (auto& c : t.conditions)
        {
            nlohmann::json cj;
            cj["type"] = static_cast<int>(c.type);
            cj["param"] = c.parameterName;
            cj["op"] = static_cast<int>(c.op);
            cj["threshold"] = c.threshold;
            condsArray.push_back(cj);
        }

        tr["conditions"] = condsArray;
        transArray.push_back(tr);
    }
    j["GraphTransitions"] = transArray;

    std::ofstream file(GetFullGraphAssetPath(assetName));
    if (file.is_open()) {
        file << j.dump(4);
        m_graphAssetFilePath = assetName;
    }
}
void AnimationControllerComponent::LoadGraphAsset(const std::string& assetName)
{
    if (assetName.empty()) return;

    std::ifstream file(GetFullGraphAssetPath(assetName));
    if (!file.is_open()) return;

    nlohmann::json j;
    file >> j;

    m_graphAssetFilePath = assetName;
    m_currentGraphScopeId = "Root";
    m_graphScopeStack.clear();

    std::string stateAssetPath = j.value("AssetPath", "");
    if (!stateAssetPath.empty())
    {
        m_assetFilePath = stateAssetPath;
        LoadAsset(m_assetFilePath);
    }

    graphNodes.clear();
    graphTransitions.clear();

    uintptr_t maxId = 0;

    auto readId = [&](const nlohmann::json& src, const char* key, bool allowZero = false) -> uintptr_t
        {
            uintptr_t id = src.value(key, static_cast<uintptr_t>(0));
            if (id == 0 && !allowZero)
            {
                id = GetNextID();
            }

            if (id > maxId)
            {
                maxId = id;
            }

            return id;
        };

    if (j.contains("GraphNodes") && j["GraphNodes"].is_array())
    {
        for (auto& n : j["GraphNodes"])
        {
            AnimGraphNode node;
            node.stateName = n.value("stateName", "");
            node.type = static_cast<AnimGraphNodeType>(n.value("nodeType", static_cast<int>(AnimGraphNodeType::State)));
            node.scopeId = n.value("scopeId", "Root");
            node.childScopeId = n.value("childScopeId", "");

            node.id = ed::NodeId(readId(n, "id"));

            if (node.type == AnimGraphNodeType::Start || node.type == AnimGraphNodeType::Any ||
                node.stateName == START_NODE_NAME || node.stateName == ANY_NODE_NAME)
            {
                node.inputPin = ed::PinId(0);
                node.outputPin = ed::PinId(readId(n, "outputPin"));
            }
            else if (node.type == AnimGraphNodeType::Entry)
            {
                node.inputPin = ed::PinId(0);
                node.outputPin = ed::PinId(readId(n, "outputPin"));
            }
            else if (node.type == AnimGraphNodeType::Exit)
            {
                node.inputPin = ed::PinId(readId(n, "inputPin"));
                node.outputPin = ed::PinId(0);
            }
            else
            {
                node.inputPin = ed::PinId(readId(n, "inputPin"));
                node.outputPin = ed::PinId(readId(n, "outputPin"));
            }

            node.pos = ImVec2(n.value("posX", 0.0f), n.value("posY", 0.0f));

            graphNodes.push_back(node);
        }
    }

    if (j.contains("GraphTransitions") && j["GraphTransitions"].is_array())
    {
        for (auto& tr : j["GraphTransitions"])
        {
            AnimTransition transition;
            transition.fromStateName = tr.value("from", "");
            transition.toStateName = tr.value("to", "");
            transition.duration = tr.value("duration", 0.2f);
            transition.logic = static_cast<TransitionLogic>(tr.value("logic", static_cast<int>(TransitionLogic::And)));
            transition.id = ed::LinkId(readId(tr, "id"));
            transition.scopeId = tr.value("scopeId", "Root");

            if (tr.contains("conditions") && tr["conditions"].is_array())
            {
                for (auto& cj : tr["conditions"])
                {
                    AnimCondition cond;
                    cond.type = static_cast<ConditionType>(cj.value("type", static_cast<int>(ConditionType::Variable)));
                    cond.parameterName = cj.value("param", "");
                    cond.op = static_cast<ConditionOp>(cj.value("op", static_cast<int>(ConditionOp::Equal)));
                    cond.threshold = cj.value("threshold", 0.0f);

                    transition.conditions.push_back(cond);
                }
            }

            if (!transition.fromStateName.empty() && !transition.toStateName.empty())
            {
                graphTransitions.push_back(transition);
            }
        }
    }

    SetIDCounterFloor(maxId);
    EnsureSpecialGraphNodes();

    m_needsPositionRestore = true;
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

    EnsureSpecialGraphNodes();

    ed::SetCurrentEditor(nodeEditorContext);

    ImGui::Begin("Animation Graph Editor");

    auto currentScopeName = [&]() -> std::string {
        return m_currentGraphScopeId.empty() ? std::string("Root") : m_currentGraphScopeId;
        };
    auto ensureScopedEntryExitNodes = [&]() {
        const std::string scope = currentScopeName();
        if (scope == "Root") return;

        auto ensureScopedNode = [&](AnimGraphNodeType type, const std::string& internalName, const ImVec2& defaultPos) {
            auto it = std::find_if(graphNodes.begin(), graphNodes.end(),
                [&](const AnimGraphNode& node) { return node.stateName == internalName; });
            if (it == graphNodes.end())
            {
                AnimGraphNode node;
                node.id = ed::NodeId(GetNextID());
                node.stateName = internalName;
                node.inputPin = (type == AnimGraphNodeType::Exit) ? ed::PinId(GetNextID()) : ed::PinId(0);
                node.outputPin = (type == AnimGraphNodeType::Entry) ? ed::PinId(GetNextID()) : ed::PinId(0);
                node.pos = defaultPos;
                node.type = type;
                node.scopeId = scope;
                graphNodes.push_back(node);
                m_needsPositionRestore = true;
                return;
            }

            it->type = type;
            it->scopeId = scope;
            if ((uintptr_t)it->id.Get() == 0) it->id = ed::NodeId(GetNextID());
            if (type == AnimGraphNodeType::Entry)
            {
                it->inputPin = ed::PinId(0);
                if ((uintptr_t)it->outputPin.Get() == 0) it->outputPin = ed::PinId(GetNextID());
            }
            else if (type == AnimGraphNodeType::Exit)
            {
                if ((uintptr_t)it->inputPin.Get() == 0) it->inputPin = ed::PinId(GetNextID());
                it->outputPin = ed::PinId(0);
            }
            };

        ensureScopedNode(AnimGraphNodeType::Entry, MakeEntryNodeName(scope), ImVec2(-360.0f, -80.0f));
        ensureScopedNode(AnimGraphNodeType::Exit, MakeExitNodeName(scope), ImVec2(360.0f, 120.0f));
        };
    ensureScopedEntryExitNodes();

    auto isNodeInCurrentScope = [&](const AnimGraphNode& node) {
        return node.scopeId.empty() ? currentScopeName() == "Root" : node.scopeId == currentScopeName();
        };

    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputText("Graph Asset", &m_graphAssetFilePath);

    ImGui::SameLine();
    if (ImGui::Button("Save Graph")) {
        SaveGraphAsset(m_graphAssetFilePath);
    }

    ImGui::SameLine();
    if (ImGui::Button("Load Graph")) {
        LoadGraphAsset(m_graphAssetFilePath);
    }

    if (ImGui::Button("Focus All"))
        ed::NavigateToContent();

    ImGui::SameLine();
    if (ImGui::Button("Back") && !m_graphScopeStack.empty())
    {
        for (auto& node : graphNodes)
        {
            if (isNodeInCurrentScope(node))
                node.pos = ed::GetNodePosition(node.id);
        }
        m_currentGraphScopeId = m_graphScopeStack.back();
        m_graphScopeStack.pop_back();
        m_needsPositionRestore = true;
    }

    ImGui::SameLine();
    ImGui::Text("Scope: %s", currentScopeName().c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("Double-click SubGraph to enter / Right-click: Add node");

    ed::Begin("Animation Graph", ImVec2(0.0f, 0.0f));

    std::unordered_map<uintptr_t, ImVec2> pinPositions;

    auto drawGraphNode = [&](AnimGraphNode& node)
        {
            const bool isSpecial = IsSpecialGraphNode(node);
            const bool isActive = !_runtimeActiveStateName.empty() &&
                (node.stateName == _runtimeActiveStateName) &&
                !isSpecial;
            const char* displayName = GetGraphNodeDisplayName(node);

            const float nodeW = isSpecial ? 180.0f : 220.0f;
            const float titleH = isSpecial ? 62.0f : 58.0f;
            const float bodyH = isSpecial ? 52.0f : 54.0f;
            const float nodeH = titleH + bodyH;
            const float pinR = 14.0f;
            const ImVec2 pinSize(pinR * 2.0f, pinR * 2.0f);

            auto isPinCircleHovered = [&](const ImVec2& center) {
                const ImVec2 mouse = ImGui::GetMousePos();
                const float dx = mouse.x - center.x;
                const float dy = mouse.y - center.y;
                const float hitRadius = pinR + 3.0f;
                return ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
                    (dx * dx + dy * dy) <= (hitRadius * hitRadius);
                };

            ImU32 fillColor = GetGraphNodeFillColor(node, isActive);
            ImU32 borderColor = GetGraphNodeBorderColor(node, isActive);

            ed::PushStyleVar(ed::StyleVar_NodeRounding, isSpecial ? 18.0f : 10.0f);
            ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(0, 0, 0, 0));
            ed::PushStyleVar(ed::StyleVar_NodeBorderWidth, isActive ? 3.0f : 2.0f);
            ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(0, 0, 0, 0));
            ed::PushStyleColor(ed::StyleColor_NodeBorder, ImVec4(0, 0, 0, 0));

            ed::BeginNode(node.id);
            ImGui::PushID((int)(uintptr_t)node.id.Get());

            ImVec2 nodeMin = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(nodeW, nodeH));
            ImVec2 nodeMax(nodeMin.x + nodeW, nodeMin.y + nodeH);

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImU32 lineColor = IM_COL32(20, 20, 20, 255);
            ImU32 titleColor = GetGraphNodeTitleColor(node);

            drawList->AddRectFilled(nodeMin, nodeMax, fillColor, isSpecial ? 28.0f : 10.0f);
            drawList->AddRect(nodeMin, nodeMax, borderColor, isSpecial ? 28.0f : 10.0f, 0, isActive ? 3.0f : 2.0f);
            drawList->AddLine(ImVec2(nodeMin.x, nodeMin.y + titleH), ImVec2(nodeMax.x, nodeMin.y + titleH), lineColor, 2.0f);

            ImVec2 titleSize = ImGui::CalcTextSize(displayName);
            float titleScale = isSpecial ? 1.45f : 1.15f;
            ImGui::SetWindowFontScale(titleScale);
            titleSize = ImGui::CalcTextSize(displayName);
            ImGui::SetCursorScreenPos(ImVec2(nodeMin.x + (nodeW - titleSize.x) * 0.5f, nodeMin.y + (titleH - titleSize.y) * 0.5f));
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(titleColor), "%s", displayName);
            ImGui::SetWindowFontScale(1.0f);

            if (isSpecial)
            {
                ImVec2 pinCenter(nodeMin.x + nodeW * 0.5f, nodeMin.y + titleH + bodyH * 0.5f);

                if ((uintptr_t)node.outputPin.Get() != 0)
                {
                    ImGui::SetCursorScreenPos(ImVec2(pinCenter.x - pinR, pinCenter.y - pinR));
                    ed::BeginPin(node.outputPin, ed::PinKind::Output);
                    ImGui::Dummy(pinSize);
                    ed::EndPin();
                    pinPositions[(uintptr_t)node.outputPin.Get()] = pinCenter;
                }
                else if ((uintptr_t)node.inputPin.Get() != 0)
                {
                    ImGui::SetCursorScreenPos(ImVec2(pinCenter.x - pinR, pinCenter.y - pinR));
                    ed::BeginPin(node.inputPin, ed::PinKind::Input);
                    ImGui::Dummy(pinSize);
                    ed::EndPin();
                    pinPositions[(uintptr_t)node.inputPin.Get()] = pinCenter;
                }

                ImU32 pinColor = isPinCircleHovered(pinCenter)
                    ? IM_COL32(255, 255, 255, 255)
                    : IM_COL32(235, 220, 0, 255);

                drawList->AddCircle(pinCenter, pinR, pinColor, 24, 3.0f);
            }
            else
            {
                drawList->AddLine(
                    ImVec2(nodeMin.x + nodeW * 0.5f, nodeMin.y + titleH),
                    ImVec2(nodeMin.x + nodeW * 0.5f, nodeMax.y),
                    lineColor, 2.0f
                );

                const char* inText = "In";
                const char* outText = "Out";
                ImVec2 inTextSize = ImGui::CalcTextSize(inText);
                ImVec2 outTextSize = ImGui::CalcTextSize(outText);
                float leftHalfCenter = nodeMin.x + (nodeW * 0.25f);
                float rightHalfCenter = nodeMin.x + (nodeW * 0.75f);
                float textY = nodeMin.y + titleH + (bodyH - inTextSize.y) * 0.5f;

                drawList->AddText(ImVec2(leftHalfCenter - inTextSize.x * 0.5f, textY), IM_COL32(0, 0, 0, 255), inText);
                drawList->AddText(ImVec2(rightHalfCenter - outTextSize.x * 0.5f, textY), IM_COL32(0, 0, 0, 255), outText);

                ImVec2 inCenter(nodeMin.x + 28.0f, nodeMin.y + titleH + bodyH * 0.5f);
                ImGui::SetCursorScreenPos(ImVec2(inCenter.x - pinR, inCenter.y - pinR));
                ed::BeginPin(node.inputPin, ed::PinKind::Input);
                ImGui::Dummy(pinSize);
                ed::EndPin();
                pinPositions[(uintptr_t)node.inputPin.Get()] = inCenter;

                ImU32 inColor = isPinCircleHovered(inCenter)
                    ? IM_COL32(255, 255, 255, 255)
                    : IM_COL32(235, 220, 0, 255);
                drawList->AddCircle(inCenter, pinR, inColor, 24, 3.0f);

                ImVec2 outCenter(nodeMax.x - 28.0f, nodeMin.y + titleH + bodyH * 0.5f);
                ImGui::SetCursorScreenPos(ImVec2(outCenter.x - pinR, outCenter.y - pinR));
                ed::BeginPin(node.outputPin, ed::PinKind::Output);
                ImGui::Dummy(pinSize);
                ed::EndPin();
                pinPositions[(uintptr_t)node.outputPin.Get()] = outCenter;

                ImU32 outColor = isPinCircleHovered(outCenter)
                    ? IM_COL32(255, 255, 255, 255)
                    : IM_COL32(230, 45, 55, 255);
                drawList->AddCircle(outCenter, pinR, outColor, 24, 3.0f);
            }

            ImGui::PopID();
            ed::EndNode();
            ed::PopStyleColor(2);
            ed::PopStyleVar(3);
        };

    for (auto& node : graphNodes)
    {
        if (isNodeInCurrentScope(node))
            drawGraphNode(node);
    }

    if (_nodeToPlaceHash != 0)
    {
        for (auto& n : graphNodes) {
            if ((uintptr_t)n.id.Get() == _nodeToPlaceHash && isNodeInCurrentScope(n)) {
                ed::SetNodePosition(n.id, _newNodeSpawnPos);
                ed::SelectNode(n.id);
                break;
            }
        }
        _nodeToPlaceHash = 0;
    }

    for (auto& link : graphTransitions)
    {
        auto* startNode = FindNodeByName(link.fromStateName);
        auto* endNode = FindNodeByName(link.toStateName);
        if (link.scopeId != currentScopeName()) continue;
        if (startNode && endNode && isNodeInCurrentScope(*startNode) && isNodeInCurrentScope(*endNode) && (uintptr_t)startNode->outputPin.Get() != 0 && (uintptr_t)endNode->inputPin.Get() != 0)
        {
            ImVec4 color = (link.fromStateName == _runtimeActiveStateName || link.fromStateName == ANY_NODE_NAME)
                ? ImVec4(0.25f, 0.65f, 1.0f, 1.0f)
                : ImVec4(0.45f, 0.45f, 0.45f, 1.0f);
            ed::Link(link.id, startNode->outputPin, endNode->inputPin, color, 3.0f);
        }
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    for (auto& link : graphTransitions)
    {
        auto* startNode = FindNodeByName(link.fromStateName);
        auto* endNode = FindNodeByName(link.toStateName);
        if (link.scopeId != currentScopeName()) continue;
        if (!startNode || !endNode || !isNodeInCurrentScope(*startNode) || !isNodeInCurrentScope(*endNode)) continue;

        auto srcKey = (uintptr_t)startNode->outputPin.Get();
        auto dstKey = (uintptr_t)endNode->inputPin.Get();
        if (pinPositions.count(srcKey) && pinPositions.count(dstKey))
        {
            ImU32 color = (link.fromStateName == _runtimeActiveStateName || link.fromStateName == ANY_NODE_NAME)
                ? IM_COL32(80, 165, 255, 255)
                : IM_COL32(120, 120, 120, 255);
            DrawLinkArrow(drawList, pinPositions[srcKey], pinPositions[dstKey], color);
        }
    }

    if (m_needsPositionRestore)
    {
        for (auto& node : graphNodes)
        {
            if (isNodeInCurrentScope(node))
                ed::SetNodePosition(node.id, node.pos);
        }

        ed::NavigateToContent();
        m_needsPositionRestore = false;
    }

    if (ed::BeginCreate())
    {
        auto getPinInfo = [&](uintptr_t pinId, bool& isInput, std::string& nodeName) -> bool {
            for (auto& n : graphNodes) {
                if (!isNodeInCurrentScope(n)) continue;
                if ((uintptr_t)n.inputPin.Get() != 0 && (uintptr_t)n.inputPin.Get() == pinId) { isInput = true;  nodeName = n.stateName; return true; }
                if ((uintptr_t)n.outputPin.Get() != 0 && (uintptr_t)n.outputPin.Get() == pinId) { isInput = false; nodeName = n.stateName; return true; }
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
                    ed::RejectNewItem(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), 2.0f);
                }
                else
                {
                    const std::string finalFrom = isStartInput ? dstName : srcName;
                    const std::string finalTo = isStartInput ? srcName : dstName;
                    auto* finalFromNode = FindNodeByName(finalFrom);
                    auto* finalToNode = FindNodeByName(finalTo);
                    const bool invalidToSpecial = finalToNode &&
                        (finalToNode->type == AnimGraphNodeType::Start ||
                         finalToNode->type == AnimGraphNodeType::Any ||
                         finalToNode->type == AnimGraphNodeType::Entry);
                    const bool invalidFromExit = finalFromNode && finalFromNode->type == AnimGraphNodeType::Exit;
                    const bool invalidFromStartToStart = (finalFrom == START_NODE_NAME && finalTo == START_NODE_NAME);

                    if (invalidToSpecial || invalidFromExit || invalidFromStartToStart)
                    {
                        ed::RejectNewItem(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), 2.0f);
                    }
                    else if (ed::AcceptNewItem(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), 2.0f))
                    {
                        bool exists = false;
                        for (auto& t : graphTransitions)
                            if (t.scopeId == currentScopeName() && t.fromStateName == finalFrom && t.toStateName == finalTo) { exists = true; break; }

                        if (!exists)
                        {
                            if (finalFrom == START_NODE_NAME)
                            {
                                graphTransitions.erase(
                                    std::remove_if(graphTransitions.begin(), graphTransitions.end(),
                                        [&](const auto& t) { return t.scopeId == currentScopeName() && t.fromStateName == START_NODE_NAME; }),
                                    graphTransitions.end());
                            }

                            AnimTransition transition;
                            transition.fromStateName = finalFrom;
                            transition.toStateName = finalTo;
                            transition.duration = 0.2f;
                            transition.id = ed::LinkId(GetNextID());
                            transition.scopeId = currentScopeName();
                            graphTransitions.push_back(transition);
                        }
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
            auto nodeIt = std::find_if(graphNodes.begin(), graphNodes.end(),
                [&](const auto& n) { return n.id == nodeId; });
            if (nodeIt == graphNodes.end())
                continue;

            if (IsSpecialGraphNode(*nodeIt))
            {
                ed::RejectDeletedItem();
                continue;
            }

            if (ed::AcceptDeletedItem())
            {
                std::string deletedName = nodeIt->stateName;

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

            }
        }
    }
    ed::EndDelete();

    ed::NodeId doubleClickedNodeId = ed::GetDoubleClickedNode();
    if ((uintptr_t)doubleClickedNodeId.Get() != 0)
    {
        auto it = std::find_if(graphNodes.begin(), graphNodes.end(),
            [&](const auto& n) { return n.id == doubleClickedNodeId; });
        if (it != graphNodes.end() && it->type == AnimGraphNodeType::SubGraph)
        {
            if (it->childScopeId.empty())
                it->childScopeId = it->stateName;
            for (auto& node : graphNodes)
            {
                if (isNodeInCurrentScope(node))
                    node.pos = ed::GetNodePosition(node.id);
            }
            m_graphScopeStack.push_back(currentScopeName());
            m_currentGraphScopeId = it->childScopeId;
            m_needsPositionRestore = true;
        }
    }

    if (ed::HasSelectionChanged())
    {
        ed::NodeId selectedNodeId;
        if (ed::GetSelectedNodes(&selectedNodeId, 1) > 0)
        {
            auto it = std::find_if(graphNodes.begin(), graphNodes.end(),
                [&](const auto& n) { return n.id == selectedNodeId; });
            if (it != graphNodes.end() && it->type == AnimGraphNodeType::State && states.count(it->stateName))
                editingState = states[it->stateName].get();
        }
    }

    ed::NodeId selectedSubGraphNodeId_;
    if (ed::GetSelectedNodes(&selectedSubGraphNodeId_, 1) > 0)
    {
        auto subGraphIt_ = std::find_if(graphNodes.begin(), graphNodes.end(),
            [&](const auto& node_) { return node_.id == selectedSubGraphNodeId_ && node_.type == AnimGraphNodeType::SubGraph; });

        if (subGraphIt_ != graphNodes.end() && subGraphIt_->scopeId == currentScopeName())
        {
            ed::Suspend();

            ImGui::Begin("SubGraph Properties");

            std::string newName_ = subGraphIt_->stateName;
            if (ImGui::InputText("Name", &newName_))
            {
                const bool isEmpty_ = newName_.empty();
                const bool isSpecialName_ = IsSpecialGraphNodeName(newName_) ||
                    newName_.rfind(ENTRY_NODE_PREFIX, 0) == 0 ||
                    newName_.rfind(EXIT_NODE_PREFIX, 0) == 0;
                const bool exists_ = std::any_of(graphNodes.begin(), graphNodes.end(),
                    [&](const auto& node_) { return node_.id != subGraphIt_->id && node_.stateName == newName_; });

                if (!isEmpty_ && !isSpecialName_ && !exists_ && newName_ != subGraphIt_->stateName)
                {
                    const std::string oldName_ = subGraphIt_->stateName;
                    subGraphIt_->stateName = newName_;

                    for (auto& transition_ : graphTransitions)
                    {
                        if (transition_.fromStateName == oldName_)
                            transition_.fromStateName = newName_;
                        if (transition_.toStateName == oldName_)
                            transition_.toStateName = newName_;
                    }
                }
            }

            ImGui::TextDisabled("Internal Scope: %s", subGraphIt_->childScopeId.c_str());
            ImGui::TextDisabled("Double-click the node to edit its internal graph.");

            ImGui::End();

            ed::Resume();
        }
    }
    static ed::LinkId selectedLinkId;
    if (ed::GetSelectedLinks(&selectedLinkId, 1) > 0)
    {
        auto it = std::find_if(graphTransitions.begin(), graphTransitions.end(),
            [&](const auto& t) { return t.id == selectedLinkId; });

        if (it != graphTransitions.end() && it->scopeId == currentScopeName())
        {
            ed::Suspend();

            ImGui::Begin("Transition Properties");

            auto* fromNode = FindNodeByName(it->fromStateName);
            auto* toNode = FindNodeByName(it->toStateName);
            if (fromNode && toNode)
                ImGui::Text("Transition: %s -> %s", GetGraphNodeDisplayName(*fromNode), GetGraphNodeDisplayName(*toNode));
            else
                ImGui::Text("Transition: (Invalid Nodes)");

            ImGui::Separator();
            ImGui::DragFloat("Fade Duration", &it->duration, 0.01f, 0.0f, 2.0f);

            if (it->fromStateName == START_NODE_NAME)
            {
                ImGui::TextDisabled("Start transitions are used only to choose the first state.");
            }
            else
            {
                ImGui::TextColored(ImVec4(0, 1, 1, 1), "Conditions");
                int logic = static_cast<int>(it->logic);
                const char* logicItems[] = { "AND", "OR" };
                if (ImGui::Combo("Logic", &logic, logicItems, IM_ARRAYSIZE(logicItems)))
                    it->logic = static_cast<TransitionLogic>(logic);

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
                    {
                        it->conditions.erase(it->conditions.begin() + i);
                        --i;
                    }

                    ImGui::Separator();
                    ImGui::PopID();
                }
            }

            ImGui::End();

            ed::Resume();
        }
    }

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
                    newNode.id = ed::NodeId(GetNextID());
                    newNode.stateName = name;
                    newNode.inputPin = ed::PinId(GetNextID());
                    newNode.outputPin = ed::PinId(GetNextID());
                    newNode.type = AnimGraphNodeType::State;
                    newNode.scopeId = currentScopeName();
                    graphNodes.push_back(newNode);
                    _nodeToPlaceHash = (uintptr_t)newNode.id.Get();
                }
            }
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Create Empty State"))
        {
            std::string newName = "NewState_" + std::to_string(graphNodes.size());
            AddState(newName, 0);

            for (auto& n : graphNodes) {
                if (n.stateName == newName) {
                    n.scopeId = currentScopeName();
                    _nodeToPlaceHash = (uintptr_t)n.id.Get();
                    break;
                }
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Create SubGraph"))
        {
            std::string baseName = "SubGraph";
            int index = 1;
            std::string graphName;
            bool exists = false;
            do
            {
                graphName = baseName + "_" + std::to_string(index++);
                exists = std::any_of(graphNodes.begin(), graphNodes.end(),
                    [&](const auto& n) { return n.stateName == graphName || n.childScopeId == graphName; });
            } while (exists);

            AnimGraphNode subGraphNode;
            subGraphNode.id = ed::NodeId(GetNextID());
            subGraphNode.stateName = graphName;
            subGraphNode.childScopeId = graphName;
            subGraphNode.inputPin = ed::PinId(GetNextID());
            subGraphNode.outputPin = ed::PinId(GetNextID());
            subGraphNode.type = AnimGraphNodeType::SubGraph;
            subGraphNode.scopeId = currentScopeName();
            graphNodes.push_back(subGraphNode);
            _nodeToPlaceHash = (uintptr_t)subGraphNode.id.Get();
        }
        ImGui::EndPopup();
    }
    ed::Resume();

    ed::End();
    ed::SetCurrentEditor(nullptr);
    ImGui::End();
}

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

