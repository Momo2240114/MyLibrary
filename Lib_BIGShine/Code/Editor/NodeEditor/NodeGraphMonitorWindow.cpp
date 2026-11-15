#include "PrecompiledHeader.h"

#include "NodeGraphMonitorWindow.h"

#include "Engine/GamePlay/AI/AIControllerComponent.h"
#include "Engine/GamePlay/AI/AIBehaviorGraph.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/Graphics/Resource/Animation/AnimationState.h"
#include "Game/AI/Dragon/DragonAIComponent.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    const ImVec2 NODE_SIZE(190.0f, 68.0f);

    // UIへ表示する列挙値名をここへ集約し、描画側では文字列変換の分岐を持たないようにしている。
    const char* GetGraphTypeName(NODE_GRAPH_MONITOR_TYPE type_)
    {
        switch (type_)
        {
        case NODE_GRAPH_MONITOR_TYPE::AUTO: return "Auto";
        case NODE_GRAPH_MONITOR_TYPE::AI_BEHAVIOR: return "AI Behavior";
        case NODE_GRAPH_MONITOR_TYPE::DRAGON_AI: return "Dragon AI";
        case NODE_GRAPH_MONITOR_TYPE::ANIMATION: return "Animation";
        default: return "Auto";
        }
    }

    const char* GetFocusModeName(NODE_GRAPH_MONITOR_FOCUS_MODE mode_)
    {
        switch (mode_)
        {
        case NODE_GRAPH_MONITOR_FOCUS_MODE::GRAPH_ORIGIN: return "Graph Origin";
        case NODE_GRAPH_MONITOR_FOCUS_MODE::SCOPE_CENTER: return "Scope Center";
        case NODE_GRAPH_MONITOR_FOCUS_MODE::ACTIVE_NODE: return "Active Node";
        default: return "Active Node";
        }
    }

    const char* GetAIBehaviorStatusName(AI_BEHAVIOR_STATUS status_)
    {
        switch (status_)
        {
        case AI_BEHAVIOR_STATUS::SUCCESS: return "Success";
        case AI_BEHAVIOR_STATUS::FAILURE: return "Failure";
        case AI_BEHAVIOR_STATUS::RUNNING: return "Running";
        default: return "Unknown";
        }
    }

    const char* GetAIBehaviorNodeTypeName(AI_BEHAVIOR_NODE_TYPE type_)
    {
        switch (type_)
        {
        case AI_BEHAVIOR_NODE_TYPE::ROOT: return "Root";
        case AI_BEHAVIOR_NODE_TYPE::SELECTOR: return "Selector";
        case AI_BEHAVIOR_NODE_TYPE::SEQUENCE: return "Sequence";
        case AI_BEHAVIOR_NODE_TYPE::RANDOM_SELECTOR: return "Random Selector";
        case AI_BEHAVIOR_NODE_TYPE::BLACKBOARD_CONDITION: return "Blackboard Condition";
        case AI_BEHAVIOR_NODE_TYPE::SET_BLACKBOARD: return "Set Blackboard";
        case AI_BEHAVIOR_NODE_TYPE::PLAY_ACTION: return "Play Action";
        case AI_BEHAVIOR_NODE_TYPE::WAIT: return "Wait";
        case AI_BEHAVIOR_NODE_TYPE::WAIT_BLACKBOARD_BOOL: return "Wait Blackboard Bool";
        case AI_BEHAVIOR_NODE_TYPE::SUB_GRAPH: return "Sub Graph";
        default: return "Unknown";
        }
    }

    const char* GetAnimationNodeTypeName(AnimGraphNodeType type_)
    {
        switch (type_)
        {
        case AnimGraphNodeType::State: return "State";
        case AnimGraphNodeType::Start: return "Start";
        case AnimGraphNodeType::Any: return "Any";
        case AnimGraphNodeType::SubGraph: return "Sub Graph";
        case AnimGraphNodeType::Entry: return "Entry";
        case AnimGraphNodeType::Exit: return "Exit";
        default: return "Unknown";
        }
    }

    bool HasAnyGraph(const std::shared_ptr<GameObject>& object_)
    {
        if (!object_) return false;
        return object_->GetComponent<AIControllerComponent>() != nullptr ||
            object_->GetComponent<DragonAIComponent>() != nullptr ||
            object_->GetComponent<AnimationControllerComponent>() != nullptr;
    }

    ImVec2 BuildCanvasPosition(const ImVec2& origin_, const ImVec2& graphPosition_, const ImVec2& offset_, float zoom_)
    {
        // グラフ上の座標を、現在の表示オフセットとズームを通したキャンバス座標へ変換する。
        return ImVec2(
            origin_.x + (graphPosition_.x - offset_.x) * zoom_,
            origin_.y + (graphPosition_.y - offset_.y) * zoom_);
    }

    ImVec2 BuildOriginOffset(const ImVec2& minimumPosition_, float zoom_)
    {
        // グラフの左上が少し余白を持って見えるよう、最小座標から表示開始位置を決める。
        return ImVec2(
            minimumPosition_.x - 28.0f / zoom_,
            minimumPosition_.y - 28.0f / zoom_);
    }

    ImVec2 BuildFocusedOffset(const ImVec2& canvasSize_, const ImVec2& focusPosition_, float zoom_)
    {
        return ImVec2(
            focusPosition_.x - (canvasSize_.x * 0.5f) / zoom_,
            focusPosition_.y - (canvasSize_.y * 0.5f) / zoom_);
    }

    bool IsScopeMatch(const std::string& nodeScopeId_, const std::string& scopeId_)
    {
        // 空のスコープはルート扱いにそろえ、AIとアニメーションで同じ比較規則にする。
        const std::string normalizedNodeScopeId_ = nodeScopeId_.empty() ? std::string("Root") : nodeScopeId_;
        const std::string normalizedScopeId_ = scopeId_.empty() ? std::string("Root") : scopeId_;
        return normalizedNodeScopeId_ == normalizedScopeId_;
    }

    void AddUniqueScope(std::vector<std::string>& scopes_, const std::string& scopeId_)
    {
        // コンボボックスへ出すスコープ一覧は重複を許さず、空文字は必ずRootとして見せる。
        const std::string normalizedScopeId_ = scopeId_.empty() ? std::string("Root") : scopeId_;
        if (std::find(scopes_.begin(), scopes_.end(), normalizedScopeId_) == scopes_.end())
        {
            scopes_.push_back(normalizedScopeId_);
        }
    }

    std::string GetAnimationNodeDisplayName(const AnimGraphNode& node_)
    {
        switch (node_.type)
        {
        case AnimGraphNodeType::Start: return "Start";
        case AnimGraphNodeType::Any: return "Any";
        case AnimGraphNodeType::Entry: return "Entry";
        case AnimGraphNodeType::Exit: return "Exit";
        default: return node_.stateName;
        }
    }

    bool HasScope(const std::vector<std::string>& scopes_, const std::string& scopeId_)
    {
        return std::find(scopes_.begin(), scopes_.end(), scopeId_) != scopes_.end();
    }

    void DrawScopeCombo(const char* label_, std::string& scopeId_, const std::vector<std::string>& scopes_, bool& followRuntimeScope_)
    {
        // ユーザーが手動でスコープを選んだ場合は、ランタイム追従を切って表示先を固定する。
        ImGui::SetNextItemWidth(170.0f);
        if (ImGui::BeginCombo(label_, scopeId_.c_str()))
        {
            for (const std::string& scope_ : scopes_)
            {
                const bool selected_ = scopeId_ == scope_;
                if (ImGui::Selectable(scope_.c_str(), selected_))
                {
                    scopeId_ = scope_;
                    followRuntimeScope_ = false;
                }
                if (selected_) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    void DrawMonitorNode(
        ImDrawList* drawList_,
        const ImVec2& minimum_,
        const char* title_,
        const char* subtitle_,
        bool active_)
    {
        // 実行中ノードだけ色と枠を強め、現在通っている処理経路が一目で分かるように描画する。
        const ImVec2 maximum_(minimum_.x + NODE_SIZE.x, minimum_.y + NODE_SIZE.y);
        const ImU32 fillColor_ = active_
            ? IM_COL32(32, 96, 62, 255)
            : IM_COL32(50, 55, 64, 255);
        const ImU32 borderColor_ = active_
            ? IM_COL32(80, 255, 150, 255)
            : IM_COL32(120, 126, 138, 255);
        drawList_->AddRectFilled(minimum_, maximum_, fillColor_, 7.0f);
        drawList_->AddRect(minimum_, maximum_, borderColor_, 7.0f, 0, active_ ? 3.0f : 1.5f);
        drawList_->PushClipRect(ImVec2(minimum_.x + 8.0f, minimum_.y + 6.0f), ImVec2(maximum_.x - 8.0f, maximum_.y - 6.0f), true);
        drawList_->AddText(ImVec2(minimum_.x + 10.0f, minimum_.y + 10.0f), IM_COL32(245, 247, 250, 255), title_);
        drawList_->AddText(ImVec2(minimum_.x + 10.0f, minimum_.y + 40.0f), IM_COL32(185, 192, 202, 255), subtitle_);
        drawList_->PopClipRect();
    }

    void DrawArrowLine(ImDrawList* drawList_, const ImVec2& from_, const ImVec2& to_, ImU32 color_, float thickness_)
    {
        // リンク中央に矢印を置き、接続先の向きがズーム状態でも読み取れるようにする。
        drawList_->AddLine(from_, to_, color_, thickness_);

        ImVec2 direction_(to_.x - from_.x, to_.y - from_.y);
        const float length_ = std::sqrt(direction_.x * direction_.x + direction_.y * direction_.y);
        if (length_ <= 0.001f) return;

        direction_.x /= length_;
        direction_.y /= length_;

        const ImVec2 center_((from_.x + to_.x) * 0.5f, (from_.y + to_.y) * 0.5f);
        const ImVec2 side_(-direction_.y, direction_.x);
        const ImVec2 tip_(center_.x + direction_.x * 8.0f, center_.y + direction_.y * 8.0f);
        const ImVec2 back_(center_.x - direction_.x * 8.0f, center_.y - direction_.y * 8.0f);
        drawList_->AddTriangleFilled(
            tip_,
            ImVec2(back_.x + side_.x * 5.0f, back_.y + side_.y * 5.0f),
            ImVec2(back_.x - side_.x * 5.0f, back_.y - side_.y * 5.0f),
            color_);
    }
}

NodeGraphMonitorWindow::NodeGraphMonitorWindow(GameObjectManager* manager_)
    : EditorWindow("Node Graph Monitor", manager_)
{
    isVisible = false;
    defaultWindowSize = ImVec2(520.0f, 460.0f);
    style.backgroundColor = ImVec4(0.07f, 0.075f, 0.085f, 0.95f);
    style.titleBarColor = ImVec4(0.16f, 0.24f, 0.28f, 1.0f);
}

void NodeGraphMonitorWindow::Draw()
{
    if (!BeginEditorWindow(nullptr, ImGuiWindowFlags_MenuBar))
    {
        EndEditorWindow();
        return;
    }

    // 非表示タブでは対象検索を省き、表示中だけシーン内Objectから監視対象を解決する。
    std::shared_ptr<GameObject> targetObject_ = ResolveTargetObject();

    if (ImGui::BeginMenuBar())
    {
        DrawTargetControls(targetObject_);
        ImGui::EndMenuBar();
    }

    DrawRuntimeSummary(targetObject_);
    ImGui::Separator();
    DrawGraphView(targetObject_);
    DrawStylePopup();

    EndEditorWindow();
}

std::shared_ptr<GameObject> NodeGraphMonitorWindow::ResolveTargetObject()
{
    if (!gameObjectManager) return nullptr;

    // Follow Selected中は、選択中オブジェクトが監視可能なグラフを持つ場合だけ対象を差し替える。
    if (followSelectedObject)
    {
        const std::shared_ptr<GameObject> selectedObject_ = gameObjectManager->GetSelectedObject();
        if (HasAnyGraph(selectedObject_))
        {
            targetObject = selectedObject_;
            return selectedObject_;
        }
    }

    std::shared_ptr<GameObject> currentTarget_ = targetObject.lock();
    if (HasAnyGraph(currentTarget_)) return currentTarget_;

    // 選択対象が無効になった時は、シーン内で最初に見つかった監視可能オブジェクトへフォールバックする。
    for (const std::shared_ptr<GameObject>& object_ : gameObjectManager->GetObjects())
    {
        if (!HasAnyGraph(object_)) continue;
        targetObject = object_;
        return object_;
    }

    return nullptr;
}

void NodeGraphMonitorWindow::DrawTargetControls(std::shared_ptr<GameObject>& targetObject_)
{
    if (!gameObjectManager) return;

    // 追従・グラフ種別・対象オブジェクトをメニューバーにまとめ、実行中でも素早く切り替えられるようにする。
    ImGui::Checkbox("Follow Selected", &followSelectedObject);
    ImGui::SameLine();

    int graphTypeIndex_ = static_cast<int>(graphType);
    ImGui::SetNextItemWidth(130.0f);
    if (ImGui::BeginCombo("Graph", GetGraphTypeName(graphType)))
    {
        for (int index_ = 0; index_ <= static_cast<int>(NODE_GRAPH_MONITOR_TYPE::ANIMATION); ++index_)
        {
            const NODE_GRAPH_MONITOR_TYPE type_ = static_cast<NODE_GRAPH_MONITOR_TYPE>(index_);
            const bool selected_ = graphTypeIndex_ == index_;
            if (ImGui::Selectable(GetGraphTypeName(type_), selected_))
            {
                graphType = type_;
                graphTypeIndex_ = index_;
            }
            if (selected_) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(followSelectedObject);
    ImGui::SetNextItemWidth(180.0f);
    const char* targetName_ = targetObject_ ? targetObject_->GetName().c_str() : "None";
    if (ImGui::BeginCombo("Target", targetName_))
    {
        for (const std::shared_ptr<GameObject>& object_ : gameObjectManager->GetObjects())
        {
            if (!HasAnyGraph(object_)) continue;
            const bool selected_ = object_ == targetObject_;
            if (ImGui::Selectable(object_->GetName().c_str(), selected_))
            {
                targetObject = object_;
                targetObject_ = object_;
            }
            if (selected_) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
}

void NodeGraphMonitorWindow::DrawRuntimeSummary(const std::shared_ptr<GameObject>& targetObject_)
{
    if (!targetObject_)
    {
        ImGui::TextDisabled("No graph target.");
        return;
    }

    ImGui::Text("Target: %s", targetObject_->GetName().c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("Graph: %s", GetGraphTypeName(ResolveGraphType(targetObject_)));

    // 対象が持つ各コンポーネントの現在状態を並べて、グラフ表示とランタイム状態を照合しやすくする。
    if (const std::shared_ptr<AnimationControllerComponent> animation_ =
        targetObject_->GetComponent<AnimationControllerComponent>())
    {
        const AnimState* state_ = animation_->GetCurrentState();
        ImGui::Text("Animation Runtime: %s", state_ ? state_->name.c_str() : "None");
    }

    if (const std::shared_ptr<DragonAIComponent> dragonAI_ =
        targetObject_->GetComponent<DragonAIComponent>())
    {
        ImGui::Text("Dragon Runtime: %s", GetDragonActionName(dragonAI_->GetCurrentAction()));
    }

    if (const std::shared_ptr<AIControllerComponent> aiController_ =
        targetObject_->GetComponent<AIControllerComponent>())
    {
        ImGui::Text("AI Graph Status: %s", GetAIBehaviorStatusName(aiController_->GetGraph().GetLastTickStatus()));
    }
}

void NodeGraphMonitorWindow::DrawGraphView(const std::shared_ptr<GameObject>& targetObject_)
{
    if (!targetObject_)
    {
        ImGui::TextDisabled("Select a GameObject with AnimationControllerComponent, AIControllerComponent, or DragonAIComponent.");
        return;
    }

    ImGui::SetNextItemWidth(180.0f);
    ImGui::SliderFloat("Zoom", &zoom, 0.25f, 1.2f);

    // Focusはグラフ全体・スコープ中心・アクティブノード追従のどこへ視点を置くかを決める。
    ImGui::SameLine();
    int focusModeIndex_ = static_cast<int>(focusMode);
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::BeginCombo("Focus", GetFocusModeName(focusMode)))
    {
        for (int index_ = 0; index_ <= static_cast<int>(NODE_GRAPH_MONITOR_FOCUS_MODE::ACTIVE_NODE); ++index_)
        {
            const NODE_GRAPH_MONITOR_FOCUS_MODE mode_ = static_cast<NODE_GRAPH_MONITOR_FOCUS_MODE>(index_);
            const bool selected_ = focusModeIndex_ == index_;
            if (ImGui::Selectable(GetFocusModeName(mode_), selected_))
            {
                focusMode = mode_;
                focusModeIndex_ = index_;
            }
            if (selected_) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::Checkbox("Follow Runtime Scope", &followRuntimeScope);

    const NODE_GRAPH_MONITOR_TYPE resolvedType_ = ResolveGraphType(targetObject_);
    // Auto解決後の種類に応じて、AI/Dragon AI/Animationの描画関数へ振り分ける。
    if (resolvedType_ == NODE_GRAPH_MONITOR_TYPE::AI_BEHAVIOR)
    {
        if (const std::shared_ptr<AIControllerComponent> aiController_ =
            targetObject_->GetComponent<AIControllerComponent>())
        {
            DrawAIBehaviorGraph(aiController_->GetGraph());
            return;
        }
    }
    if (resolvedType_ == NODE_GRAPH_MONITOR_TYPE::DRAGON_AI)
    {
        if (const std::shared_ptr<DragonAIComponent> dragonAI_ =
            targetObject_->GetComponent<DragonAIComponent>())
        {
            DrawAIBehaviorGraph(dragonAI_->GetBehaviorGraph());
            return;
        }
    }
    if (resolvedType_ == NODE_GRAPH_MONITOR_TYPE::ANIMATION)
    {
        DrawAnimationGraph(targetObject_);
        return;
    }

    ImGui::TextDisabled("Selected graph type is not available on this object.");
}

void NodeGraphMonitorWindow::DrawAIBehaviorGraph(AIBehaviorGraph& graph_)
{
    // ノードとサブグラフ名から表示可能スコープを再構築し、現在の選択が消えていたらRootへ戻す。
    std::vector<std::string> scopes_;
    AddUniqueScope(scopes_, "Root");
    for (const AIBehaviorNode& node_ : graph_.GetNodes())
    {
        AddUniqueScope(scopes_, node_.scopeId);
        if (node_.type == AI_BEHAVIOR_NODE_TYPE::SUB_GRAPH && !node_.subGraphName.empty())
        {
            AddUniqueScope(scopes_, node_.subGraphName);
        }
    }

    if (!HasScope(scopes_, aiScopeId)) aiScopeId = "Root";

    ImVec2 activeNodePosition_(0.0f, 0.0f);
    std::string activeNodeScopeId_ = "Root";
    bool hasActiveNode_ = false;
    // 実行中ノードを探し、Follow Runtime Scope中はそのノードが属するスコープへ自動で移動する。
    for (const AIBehaviorNode& node_ : graph_.GetNodes())
    {
        if (!graph_.IsNodeRuntimeActive(node_.id)) continue;
        activeNodePosition_ = ImVec2(
            node_.position.x + NODE_SIZE.x * 0.5f / zoom,
            node_.position.y + NODE_SIZE.y * 0.5f / zoom);
        activeNodeScopeId_ = node_.scopeId.empty() ? "Root" : node_.scopeId;
        hasActiveNode_ = true;
        if (followRuntimeScope)
        {
            aiScopeId = activeNodeScopeId_;
        }
    }

    if (!HasScope(scopes_, aiScopeId)) aiScopeId = "Root";
    DrawScopeCombo("Scope", aiScopeId, scopes_, followRuntimeScope);
    if (!IsScopeMatch(activeNodeScopeId_, aiScopeId)) hasActiveNode_ = false;

    const ImVec2 canvasSize_ = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("AIBehaviorGraphMonitorCanvas", canvasSize_, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 canvasOrigin_ = ImGui::GetCursorScreenPos();
    ImDrawList* drawList_ = ImGui::GetWindowDrawList();
    drawList_->AddRectFilled(canvasOrigin_, ImVec2(canvasOrigin_.x + canvasSize_.x, canvasOrigin_.y + canvasSize_.y), IM_COL32(25, 27, 32, 255));

    // 表示中スコープに含まれるノード範囲を求め、フォーカス用の基準座標に使う。
    ImVec2 minimumPosition_(FLT_MAX, FLT_MAX);
    ImVec2 maximumPosition_(-FLT_MAX, -FLT_MAX);
    for (const AIBehaviorNode& node_ : graph_.GetNodes())
    {
        if (!IsScopeMatch(node_.scopeId, aiScopeId)) continue;
        minimumPosition_.x = (std::min)(minimumPosition_.x, node_.position.x);
        minimumPosition_.y = (std::min)(minimumPosition_.y, node_.position.y);
        maximumPosition_.x = (std::max)(maximumPosition_.x, node_.position.x);
        maximumPosition_.y = (std::max)(maximumPosition_.y, node_.position.y);
    }
    if (minimumPosition_.x == FLT_MAX) minimumPosition_ = ImVec2(0.0f, 0.0f);
    if (maximumPosition_.x == -FLT_MAX) maximumPosition_ = minimumPosition_;

    ImVec2 offset_ = BuildOriginOffset(minimumPosition_, zoom);
    if (focusMode == NODE_GRAPH_MONITOR_FOCUS_MODE::SCOPE_CENTER)
    {
        const ImVec2 scopeCenter_(
            (minimumPosition_.x + maximumPosition_.x) * 0.5f + NODE_SIZE.x * 0.5f / zoom,
            (minimumPosition_.y + maximumPosition_.y) * 0.5f + NODE_SIZE.y * 0.5f / zoom);
        offset_ = BuildFocusedOffset(canvasSize_, scopeCenter_, zoom);
    }
    else if (focusMode == NODE_GRAPH_MONITOR_FOCUS_MODE::ACTIVE_NODE && hasActiveNode_)
    {
        offset_ = BuildFocusedOffset(canvasSize_, activeNodePosition_, zoom);
    }

    std::unordered_map<uintptr_t, ImVec2> nodeCenters_;
    // 先にノード中心座標を作っておくことで、リンク描画はID同士の接続だけを見ればよくなる。
    for (const AIBehaviorNode& node_ : graph_.GetNodes())
    {
        if (!IsScopeMatch(node_.scopeId, aiScopeId)) continue;
        const ImVec2 nodeMinimum_ = BuildCanvasPosition(
            canvasOrigin_,
            ImVec2(node_.position.x, node_.position.y),
            offset_,
            zoom);
        nodeCenters_[node_.id] = ImVec2(nodeMinimum_.x + NODE_SIZE.x * 0.5f, nodeMinimum_.y + NODE_SIZE.y * 0.5f);
    }

    for (const AIBehaviorLink& link_ : graph_.GetLinks())
    {
        const auto fromIt_ = nodeCenters_.find(link_.fromNodeId);
        const auto toIt_ = nodeCenters_.find(link_.toNodeId);
        if (fromIt_ == nodeCenters_.end() || toIt_ == nodeCenters_.end()) continue;
        const bool active_ = graph_.IsNodeRuntimeActive(link_.fromNodeId) && graph_.IsNodeRuntimeActive(link_.toNodeId);
        DrawArrowLine(drawList_, fromIt_->second, toIt_->second, active_ ? IM_COL32(80, 255, 150, 255) : IM_COL32(130, 136, 148, 255), active_ ? 3.0f : 1.5f);
    }

    // ノードはリンクの上に描画し、実行中ノードの強調が線に隠れないようにする。
    for (const AIBehaviorNode& node_ : graph_.GetNodes())
    {
        if (!IsScopeMatch(node_.scopeId, aiScopeId)) continue;
        const ImVec2 nodeMinimum_ = BuildCanvasPosition(
            canvasOrigin_,
            ImVec2(node_.position.x, node_.position.y),
            offset_,
            zoom);
        DrawMonitorNode(drawList_, nodeMinimum_, node_.name.c_str(), GetAIBehaviorNodeTypeName(node_.type), graph_.IsNodeRuntimeActive(node_.id));
    }

    ImGui::Dummy(canvasSize_);
    ImGui::EndChild();
}

void NodeGraphMonitorWindow::DrawAnimationGraph(const std::shared_ptr<GameObject>& targetObject_)
{
    // アニメーション側はControllerComponentの編集用ノード情報と現在再生中ステートを重ねて表示する。
    const std::shared_ptr<AnimationControllerComponent> animation_ =
        targetObject_ ? targetObject_->GetComponent<AnimationControllerComponent>() : nullptr;
    if (!animation_)
    {
        ImGui::TextDisabled("AnimationControllerComponent is not available.");
        return;
    }

    animation_->PrepareGraphEditor();
    std::vector<AnimGraphNode>& nodes_ = animation_->GetGraphNodes();
    std::vector<AnimTransition>& transitions_ = animation_->GetTransitions();
    const AnimState* activeState_ = animation_->GetCurrentState();
    const std::string activeStateName_ = activeState_ ? activeState_->name : std::string{};

    std::vector<std::string> scopes_;
    // ノード・サブグラフ・トランジションの全てからスコープを拾い、リンクだけのスコープも選べるようにする。
    AddUniqueScope(scopes_, "Root");
    for (const AnimGraphNode& node_ : nodes_)
    {
        AddUniqueScope(scopes_, node_.scopeId);
        if (node_.type == AnimGraphNodeType::SubGraph && !node_.childScopeId.empty())
        {
            AddUniqueScope(scopes_, node_.childScopeId);
        }
    }
    for (const AnimTransition& transition_ : transitions_)
    {
        AddUniqueScope(scopes_, transition_.scopeId);
    }

    if (!HasScope(scopes_, animationScopeId)) animationScopeId = "Root";

    ImVec2 activeNodePosition_(0.0f, 0.0f);
    std::string activeNodeScopeId_ = "Root";
    bool hasActiveNode_ = false;
    // 現在ステート名と一致するノードをランタイム上のアクティブノードとして扱う。
    for (const AnimGraphNode& node_ : nodes_)
    {
        if (activeStateName_.empty() || node_.stateName != activeStateName_) continue;
        activeNodePosition_ = ImVec2(
            node_.pos.x + NODE_SIZE.x * 0.5f / zoom,
            node_.pos.y + NODE_SIZE.y * 0.5f / zoom);
        activeNodeScopeId_ = node_.scopeId.empty() ? "Root" : node_.scopeId;
        hasActiveNode_ = true;
        if (followRuntimeScope)
        {
            animationScopeId = activeNodeScopeId_;
        }
    }

    if (!HasScope(scopes_, animationScopeId)) animationScopeId = "Root";
    DrawScopeCombo("Scope", animationScopeId, scopes_, followRuntimeScope);
    if (!IsScopeMatch(activeNodeScopeId_, animationScopeId)) hasActiveNode_ = false;

    const ImVec2 canvasSize_ = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("AnimationGraphMonitorCanvas", canvasSize_, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 canvasOrigin_ = ImGui::GetCursorScreenPos();
    ImDrawList* drawList_ = ImGui::GetWindowDrawList();
    drawList_->AddRectFilled(canvasOrigin_, ImVec2(canvasOrigin_.x + canvasSize_.x, canvasOrigin_.y + canvasSize_.y), IM_COL32(25, 27, 32, 255));

    // 選択スコープ内のノード矩形をもとに、表示開始位置やスコープ中心を計算する。
    ImVec2 minimumPosition_(FLT_MAX, FLT_MAX);
    ImVec2 maximumPosition_(-FLT_MAX, -FLT_MAX);
    for (const AnimGraphNode& node_ : nodes_)
    {
        if (!IsScopeMatch(node_.scopeId, animationScopeId)) continue;
        minimumPosition_.x = (std::min)(minimumPosition_.x, node_.pos.x);
        minimumPosition_.y = (std::min)(minimumPosition_.y, node_.pos.y);
        maximumPosition_.x = (std::max)(maximumPosition_.x, node_.pos.x);
        maximumPosition_.y = (std::max)(maximumPosition_.y, node_.pos.y);
    }
    if (minimumPosition_.x == FLT_MAX) minimumPosition_ = ImVec2(0.0f, 0.0f);
    if (maximumPosition_.x == -FLT_MAX) maximumPosition_ = minimumPosition_;

    ImVec2 offset_ = BuildOriginOffset(minimumPosition_, zoom);
    if (focusMode == NODE_GRAPH_MONITOR_FOCUS_MODE::SCOPE_CENTER)
    {
        const ImVec2 scopeCenter_(
            (minimumPosition_.x + maximumPosition_.x) * 0.5f + NODE_SIZE.x * 0.5f / zoom,
            (minimumPosition_.y + maximumPosition_.y) * 0.5f + NODE_SIZE.y * 0.5f / zoom);
        offset_ = BuildFocusedOffset(canvasSize_, scopeCenter_, zoom);
    }
    else if (focusMode == NODE_GRAPH_MONITOR_FOCUS_MODE::ACTIVE_NODE && hasActiveNode_)
    {
        offset_ = BuildFocusedOffset(canvasSize_, activeNodePosition_, zoom);
    }

    std::unordered_map<std::string, ImVec2> nodeCenters_;
    // アニメーショングラフのリンクはステート名で結ばれるため、ステート名から中心座標を引ける形にする。
    for (const AnimGraphNode& node_ : nodes_)
    {
        if (!IsScopeMatch(node_.scopeId, animationScopeId)) continue;
        const ImVec2 nodeMinimum_ = BuildCanvasPosition(canvasOrigin_, node_.pos, offset_, zoom);
        nodeCenters_[node_.stateName] = ImVec2(nodeMinimum_.x + NODE_SIZE.x * 0.5f, nodeMinimum_.y + NODE_SIZE.y * 0.5f);
    }

    for (const AnimTransition& transition_ : transitions_)
    {
        if (!IsScopeMatch(transition_.scopeId, animationScopeId)) continue;
        const auto fromIt_ = nodeCenters_.find(transition_.fromStateName);
        const auto toIt_ = nodeCenters_.find(transition_.toStateName);
        if (fromIt_ == nodeCenters_.end() || toIt_ == nodeCenters_.end()) continue;
        const bool active_ = !activeStateName_.empty() &&
            (transition_.fromStateName == activeStateName_ || transition_.toStateName == activeStateName_);
        DrawArrowLine(drawList_, fromIt_->second, toIt_->second, active_ ? IM_COL32(90, 195, 255, 255) : IM_COL32(130, 136, 148, 255), active_ ? 3.0f : 1.5f);
    }

    // 最後にノード本体を描画して、現在ステートと通常ノードを色で見分ける。
    for (const AnimGraphNode& node_ : nodes_)
    {
        if (!IsScopeMatch(node_.scopeId, animationScopeId)) continue;
        const bool active_ = !activeStateName_.empty() && node_.stateName == activeStateName_;
        const ImVec2 nodeMinimum_ = BuildCanvasPosition(canvasOrigin_, node_.pos, offset_, zoom);
        const std::string displayName_ = GetAnimationNodeDisplayName(node_);
        DrawMonitorNode(drawList_, nodeMinimum_, displayName_.c_str(), GetAnimationNodeTypeName(node_.type), active_);
    }

    ImGui::Dummy(canvasSize_);
    ImGui::EndChild();
}

NODE_GRAPH_MONITOR_TYPE NodeGraphMonitorWindow::ResolveGraphType(const std::shared_ptr<GameObject>& targetObject_) const
{
    if (!targetObject_) return NODE_GRAPH_MONITOR_TYPE::AUTO;
    if (graphType != NODE_GRAPH_MONITOR_TYPE::AUTO) return graphType;

    // Auto時は専用AI、汎用AI、アニメーションの順に優先し、ゲーム中の意図に近いグラフを初期表示する。
    if (targetObject_->GetComponent<DragonAIComponent>()) return NODE_GRAPH_MONITOR_TYPE::DRAGON_AI;
    if (targetObject_->GetComponent<AIControllerComponent>()) return NODE_GRAPH_MONITOR_TYPE::AI_BEHAVIOR;
    if (targetObject_->GetComponent<AnimationControllerComponent>()) return NODE_GRAPH_MONITOR_TYPE::ANIMATION;
    return NODE_GRAPH_MONITOR_TYPE::AUTO;
}
