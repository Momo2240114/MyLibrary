// Node Editor GUI
// AnimationControllerComponent の実装を役割ごとに分けたファイルです。
// クラス自体は分離せず、同じクラスのメンバ関数定義だけを別 cpp に置いています。
// そのため既存の参照関係や public/private の扱いはそのままです。

#include "AnimationControllerComponent.h"
#include "Editor/AssetBrowserPayload.h"
#include "AnimSequenceAdapter.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "AnimationDataAsset.h"
#include "PoseLibraryResourceManager.h"
#include "IKMapComponent.h"
#include "Engine/Platform/WindowsDialogHelper.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <cctype>
#include <unordered_map>

namespace ed = ax::NodeEditor;

static constexpr const char* START_NODE_NAME = "__Start__";
static constexpr const char* ANY_NODE_NAME = "__Any__";
static constexpr const char* ENTRY_NODE_PREFIX = "__Entry__:";
static constexpr const char* EXIT_NODE_PREFIX = "__Exit__:";

static std::string MakeEntryNodeName(const std::string& scopeId) { return std::string(ENTRY_NODE_PREFIX) + scopeId; }
static std::string MakeExitNodeName(const std::string& scopeId) { return std::string(EXIT_NODE_PREFIX) + scopeId; }

static bool ContainsCaseInsensitive(const std::string& text_, const std::string& query_)
{
    if (query_.empty()) return true;

    return std::search(
        text_.begin(), text_.end(),
        query_.begin(), query_.end(),
        [](unsigned char left_, unsigned char right_)
        {
            return std::tolower(left_) == std::tolower(right_);
        }) != text_.end();
}

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
    case AnimGraphNodeType::Start: return IM_COL32(25, 65, 145, 255);
    case AnimGraphNodeType::Any: return IM_COL32(80, 40, 145, 255);
    case AnimGraphNodeType::Entry: return IM_COL32(25, 100, 55, 255);
    case AnimGraphNodeType::Exit: return IM_COL32(145, 65, 20, 255);
    case AnimGraphNodeType::SubGraph: return IM_COL32(15, 90, 100, 255);
    default: return IM_COL32(15, 15, 15, 255);
    }
}

// ベジェ曲線の中点に三角形の矢印を描画するヘルパー
static void DrawLinkArrow(ImDrawList* drawList, const ImVec2& p0, const ImVec2& p3, ImU32 color)
{
    if (p0.x == 0.0f && p0.y == 0.0f) return;
    if (p3.x == 0.0f && p3.y == 0.0f) return;

    // 始点と終点の直線距離を測る
    float distance = std::sqrt((p3.x - p0.x) * (p3.x - p0.x) + (p3.y - p0.y) * (p3.y - p0.y));

    // あまりにもノードが近すぎる場合は、めり込みを防ぐため矢印を描画しない
    if (distance < 50.0f) return;

    // imgui_node_editorのデフォルトに近いベジェの制御点を計算
    float strength = std::fmax(std::abs(p3.x - p0.x) * 0.5f, 50.0f);
    ImVec2 p1 = { p0.x + strength, p0.y };
    ImVec2 p2 = { p3.x - strength, p3.y };

    // ベジェ曲線上の点を評価するラムダ式
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

    // --- 【新規】弧長サンプリングによる「真の中央」の探求 ---
    // 曲線の上を等間隔に歩み、実際の長さを計測する
    const int SAMPLE_COUNT = 8;
    float lengths[SAMPLE_COUNT + 1];
    lengths[0] = 0.0f;

    ImVec2 prevPoint = p0;
    float totalLength = 0.0f;

    for (int i = 1; i <= SAMPLE_COUNT; ++i)
    {
        float t = (float)i / (float)SAMPLE_COUNT;
        ImVec2 currPoint = evalBezier(t);
        float dx = currPoint.x - prevPoint.x;
        float dy = currPoint.y - prevPoint.y;
        totalLength += std::sqrt(dx * dx + dy * dy);
        lengths[i] = totalLength;
        prevPoint = currPoint;
    }

    // 目標とするのは、全体の長さのちょうど半分
    float targetLength = totalLength * 0.5f;
    float targetT = 0.5f; // フォールバック用

    // 目標の長さに到達する parameter 't' を線形補間で見つけ出す
    for (int i = 0; i < SAMPLE_COUNT; ++i)
    {
        if (targetLength >= lengths[i] && targetLength <= lengths[i + 1])
        {
            float tMin = (float)i / (float)SAMPLE_COUNT;
            float tMax = (float)i + 1 / (float)SAMPLE_COUNT; // 実際には (float)(i+1)/SAMPLE_COUNT
            tMax = (float)(i + 1) / (float)SAMPLE_COUNT;

            float segmentLength = lengths[i + 1] - lengths[i];
            if (segmentLength > 0.0001f)
            {
                float localRatio = (targetLength - lengths[i]) / segmentLength;
                targetT = tMin + localRatio * (tMax - tMin);
            }
            break;
        }
    }

    // 【安全弁】ノードのピン（半径14.0fなど）に矢印がめり込まないよう、tの範囲を制限する
    // 距離が短いときは、より厳しく中央付近に留まらせる
    float minT = 40.0f / totalLength; // 始点から最低25ドット離す
    float maxT = 1.0f - (40.0f / totalLength); // 終点から最低25ドット離す
    if (minT < maxT)
    {
        targetT = std::fmax(minT, std::fmin(targetT, maxT));
    }
    else
    {
        targetT = 0.5f; // 逆転するほど近いなら中央固定
    }
    // ----------------------------------------------------

    // 接線（向き）を評価するラムダ式
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

            // 矢印の先端と後方の羽を構築
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

    // 導き出された比率で描画
    drawArrowAt(targetT);
}
const std::string ASSET_DIR = "Data/Animation/StateData/";


void AnimationControllerComponent::OnGuiNodeEditor()
{
    if (!nodeEditorContext) return;

    ImGui::Begin("Animation Graph Editor");
    DrawNodeEditorContent();
    ImGui::End();
}

void AnimationControllerComponent::DrawNodeEditorContent()
{
    if (!nodeEditorContext) return;

    EnsureSpecialGraphNodes();

    ed::SetCurrentEditor(nodeEditorContext);


    auto currentScopeName = [&]() -> std::string {
        return currentGraphScopeId.empty() ? std::string("Root") : currentGraphScopeId;
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
                node.id = ed::NodeId(GetNextId());
                node.stateName = internalName;
                node.inputPin = (type == AnimGraphNodeType::Exit) ? ed::PinId(GetNextId()) : ed::PinId(0);
                node.outputPin = (type == AnimGraphNodeType::Entry) ? ed::PinId(GetNextId()) : ed::PinId(0);
                node.pos = defaultPos;
                node.type = type;
                node.scopeId = scope;
                graphNodes.push_back(node);
                needsPositionRestore = true;
                return;
            }

            it->type = type;
            it->scopeId = scope;
            if ((uintptr_t)it->id.Get() == 0) it->id = ed::NodeId(GetNextId());
            if (type == AnimGraphNodeType::Entry)
            {
                it->inputPin = ed::PinId(0);
                if ((uintptr_t)it->outputPin.Get() == 0) it->outputPin = ed::PinId(GetNextId());
            }
            else if (type == AnimGraphNodeType::Exit)
            {
                if ((uintptr_t)it->inputPin.Get() == 0) it->inputPin = ed::PinId(GetNextId());
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
    ImGui::InputText("Graph Asset", &graphAssetFilePath);
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload_ = ImGui::AcceptDragDropPayload(AssetBrowserPayload::JSON))
        {
            const char* path_ = static_cast<const char*>(payload_->Data);
            if (path_ && path_[0] != '\0')
            {
                const bool loaded_ = LoadGraphAsset(path_);
                graphAssetStatus = loaded_
                    ? "Loaded Graph: " + graphAssetFilePath
                    : "Failed to load Animation Graph: " + std::string(path_);
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::SameLine();
    if (ImGui::Button("Browse Graph..."))
    {
        const std::string selectedPath_ = WindowsDialogHelper::OpenJsonDialog(
            L"Open Animation Graph",
            "Data/Animation/GraphData");
        if (!selectedPath_.empty())
        {
            const bool loaded_ = LoadGraphAsset(selectedPath_);
            graphAssetStatus = loaded_
                ? "Loaded Graph: " + graphAssetFilePath
                : "Failed to load Animation Graph: " + selectedPath_;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Save Graph")) {
        SaveGraphAsset(graphAssetFilePath);
    }

    ImGui::SameLine();
    if (ImGui::Button("Load Graph")) {
        const bool loaded_ = LoadGraphAsset(graphAssetFilePath);
        graphAssetStatus = loaded_
            ? "Loaded Graph: " + graphAssetFilePath
            : "Failed to load Animation Graph: " + graphAssetFilePath;
    }

    if (!graphAssetStatus.empty())
    {
        ImGui::TextWrapped(
            "%s | Nodes: %d | Transitions: %d",
            graphAssetStatus.c_str(),
            static_cast<int>(graphNodes.size()),
            static_cast<int>(graphTransitions.size()));
    }

    if (ImGui::Button("Focus All"))
        ed::NavigateToContent();

    ImGui::SameLine();
    if (ImGui::Button("Back") && !graphScopeStack.empty())
    {
        for (auto& node : graphNodes)
        {
            if (isNodeInCurrentScope(node))
                node.pos = ed::GetNodePosition(node.id);
        }
        currentGraphScopeId = graphScopeStack.back();
        graphScopeStack.pop_back();
        needsPositionRestore = true;
    }

    ImGui::SameLine();
    ImGui::Text("Scope: %s", currentScopeName().c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("Double-click SubGraph to enter / Right-click: Add node");

    ed::Begin("Animation Graph", ImVec2(0.0f, 0.0f));

    std::unordered_map<uintptr_t, ImVec2> pinPositions_;

    auto drawGraphNode_ = [&](AnimGraphNode& node_)
        {
            const bool isSpecial_ = IsSpecialGraphNode(node_);
            const bool isActive_ = !runtimeActiveStateName.empty() &&
                (node_.stateName == runtimeActiveStateName) &&
                !isSpecial_;
            const char* displayName_ = GetGraphNodeDisplayName(node_);

            const float nodeWidth_ = isSpecial_ ? 180.0f : 220.0f;
            const float titleHeight_ = isSpecial_ ? 62.0f : 58.0f;
            const float bodyHeight_ = isSpecial_ ? 52.0f : 54.0f;
            const float nodeHeight_ = titleHeight_ + bodyHeight_;
            const float pinRadius_ = 14.0f;
            const ImVec2 pinSize_(pinRadius_ * 2.0f, pinRadius_ * 2.0f);

            const auto isPinCircleHovered_ = [&](const ImVec2& center_)
                {
                    const ImVec2 mouse_ = ImGui::GetMousePos();
                    const float deltaX_ = mouse_.x - center_.x;
                    const float deltaY_ = mouse_.y - center_.y;
                    const float hitRadius_ = pinRadius_ + 3.0f;
                    return ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
                        (deltaX_ * deltaX_ + deltaY_ * deltaY_) <= (hitRadius_ * hitRadius_);
                };

            const ImU32 fillColor_ = GetGraphNodeFillColor(node_, isActive_);
            const ImU32 borderColor_ = GetGraphNodeBorderColor(node_, isActive_);

            ed::PushStyleVar(ed::StyleVar_NodeRounding, isSpecial_ ? 18.0f : 10.0f);
            ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(0, 0, 0, 0));
            ed::PushStyleVar(ed::StyleVar_NodeBorderWidth, isActive_ ? 7.5f : 5.0f);
            ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(0, 0, 0, 0));
            ed::PushStyleColor(ed::StyleColor_NodeBorder, ImVec4(0, 0, 0, 0));

            ed::BeginNode(node_.id);
            ImGui::PushID(static_cast<int>(node_.id.Get()));

            const ImVec2 nodeMinimum_ = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(nodeWidth_, nodeHeight_));
            const ImVec2 nodeMaximum_(
                nodeMinimum_.x + nodeWidth_,
                nodeMinimum_.y + nodeHeight_);

            ImDrawList* drawList_ = ImGui::GetWindowDrawList();
            const ImU32 lineColor_ = IM_COL32(20, 20, 20, 255);
            const ImU32 titleColor_ = GetGraphNodeTitleColor(node_);

            drawList_->AddRectFilled(
                nodeMinimum_, nodeMaximum_, fillColor_, isSpecial_ ? 28.0f : 10.0f);
            drawList_->AddRect(
                nodeMinimum_, nodeMaximum_, borderColor_, isSpecial_ ? 28.0f : 10.0f,
                0, isActive_ ? 3.0f : 2.0f);
            drawList_->AddLine(
                ImVec2(nodeMinimum_.x, nodeMinimum_.y + titleHeight_),
                ImVec2(nodeMaximum_.x, nodeMinimum_.y + titleHeight_),
                lineColor_, 2.0f);

            ImGui::SetWindowFontScale(isSpecial_ ? 1.45f : 1.15f);
            const ImVec2 titleSize_ = ImGui::CalcTextSize(displayName_);
            ImGui::SetCursorScreenPos(ImVec2(
                nodeMinimum_.x + (nodeWidth_ - titleSize_.x) * 0.5f,
                nodeMinimum_.y + (titleHeight_ - titleSize_.y) * 0.5f));
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(titleColor_), "%s", displayName_);
            ImGui::SetWindowFontScale(1.0f);

            if (isSpecial_)
            {
                const ImVec2 pinCenter_(
                    nodeMinimum_.x + nodeWidth_ * 0.5f,
                    nodeMinimum_.y + titleHeight_ + bodyHeight_ * 0.5f);

                if (static_cast<uintptr_t>(node_.outputPin.Get()) != 0)
                {
                    ImGui::SetCursorScreenPos(ImVec2(
                        pinCenter_.x - pinRadius_, pinCenter_.y - pinRadius_));
                    ed::BeginPin(node_.outputPin, ed::PinKind::Output);
                    ImGui::Dummy(pinSize_);
                    ed::EndPin();
                    pinPositions_[static_cast<uintptr_t>(node_.outputPin.Get())] = pinCenter_;
                }
                else if (static_cast<uintptr_t>(node_.inputPin.Get()) != 0)
                {
                    ImGui::SetCursorScreenPos(ImVec2(
                        pinCenter_.x - pinRadius_, pinCenter_.y - pinRadius_));
                    ed::BeginPin(node_.inputPin, ed::PinKind::Input);
                    ImGui::Dummy(pinSize_);
                    ed::EndPin();
                    pinPositions_[static_cast<uintptr_t>(node_.inputPin.Get())] = pinCenter_;
                }

                const ImU32 pinColor_ = isPinCircleHovered_(pinCenter_)
                    ? IM_COL32(255, 255, 255, 255)
                    : IM_COL32(235, 220, 0, 255);
                drawList_->AddCircle(pinCenter_, pinRadius_, pinColor_, 24, 3.0f);
            }
            else
            {
                drawList_->AddLine(
                    ImVec2(nodeMinimum_.x + nodeWidth_ * 0.5f, nodeMinimum_.y + titleHeight_),
                    ImVec2(nodeMinimum_.x + nodeWidth_ * 0.5f, nodeMaximum_.y),
                    lineColor_, 2.0f);

                const char* inputText_ = "In";
                const char* outputText_ = "Out";
                const ImVec2 inputTextSize_ = ImGui::CalcTextSize(inputText_);
                const ImVec2 outputTextSize_ = ImGui::CalcTextSize(outputText_);
                const float leftHalfCenter_ = nodeMinimum_.x + nodeWidth_ * 0.25f;
                const float rightHalfCenter_ = nodeMinimum_.x + nodeWidth_ * 0.75f;
                const float textY_ = nodeMinimum_.y + titleHeight_ +
                    (bodyHeight_ - inputTextSize_.y) * 0.5f;

                drawList_->AddText(
                    ImVec2(leftHalfCenter_ - inputTextSize_.x * 0.5f, textY_),
                    IM_COL32(0, 0, 0, 255), inputText_);
                drawList_->AddText(
                    ImVec2(rightHalfCenter_ - outputTextSize_.x * 0.5f, textY_),
                    IM_COL32(0, 0, 0, 255), outputText_);

                const ImVec2 inputCenter_(
                    nodeMinimum_.x + 28.0f,
                    nodeMinimum_.y + titleHeight_ + bodyHeight_ * 0.5f);
                ImGui::SetCursorScreenPos(ImVec2(
                    inputCenter_.x - pinRadius_, inputCenter_.y - pinRadius_));
                ed::BeginPin(node_.inputPin, ed::PinKind::Input);
                ImGui::Dummy(pinSize_);
                ed::EndPin();
                pinPositions_[static_cast<uintptr_t>(node_.inputPin.Get())] = inputCenter_;

                const ImU32 inputColor_ = isPinCircleHovered_(inputCenter_)
                    ? IM_COL32(255, 255, 255, 255)
                    : IM_COL32(235, 220, 0, 255);
                drawList_->AddCircle(inputCenter_, pinRadius_, inputColor_, 24, 3.0f);

                const ImVec2 outputCenter_(
                    nodeMaximum_.x - 28.0f,
                    nodeMinimum_.y + titleHeight_ + bodyHeight_ * 0.5f);
                ImGui::SetCursorScreenPos(ImVec2(
                    outputCenter_.x - pinRadius_, outputCenter_.y - pinRadius_));
                ed::BeginPin(node_.outputPin, ed::PinKind::Output);
                ImGui::Dummy(pinSize_);
                ed::EndPin();
                pinPositions_[static_cast<uintptr_t>(node_.outputPin.Get())] = outputCenter_;

                const ImU32 outputColor_ = isPinCircleHovered_(outputCenter_)
                    ? IM_COL32(255, 255, 255, 255)
                    : IM_COL32(230, 45, 55, 255);
                drawList_->AddCircle(outputCenter_, pinRadius_, outputColor_, 24, 3.0f);
            }

            // 絶対位置で配置したタイトルとピンの後でも、NodeEditorへ正しい矩形を渡す。
            ImGui::SetCursorScreenPos(nodeMaximum_);
            ImGui::PopID();
            ed::EndNode();
            ed::PopStyleColor(2);
            ed::PopStyleVar(3);
        };

    for (auto& node : graphNodes)
    {
        if (isNodeInCurrentScope(node))
            drawGraphNode_(node);
    }

    if (nodeToPlaceId != 0)
    {
        for (auto& n : graphNodes) {
            if ((uintptr_t)n.id.Get() == nodeToPlaceId && isNodeInCurrentScope(n)) {
                ed::SetNodePosition(n.id, newNodeSpawnPosition);
                ed::SelectNode(n.id);
                break;
            }
        }
        nodeToPlaceId = 0;
    }

    for (auto& link : graphTransitions)
    {
        auto* startNode = FindNodeByName(link.fromStateName);
        auto* endNode = FindNodeByName(link.toStateName);
        if (link.scopeId != currentScopeName()) continue;
        if (startNode && endNode && isNodeInCurrentScope(*startNode) && isNodeInCurrentScope(*endNode) && (uintptr_t)startNode->outputPin.Get() != 0 && (uintptr_t)endNode->inputPin.Get() != 0)
        {
            ImVec4 color = (link.fromStateName == runtimeActiveStateName || link.fromStateName == ANY_NODE_NAME)
                ? ImVec4(0.25f, 0.65f, 1.0f, 1.0f)
                : ImVec4(0.45f, 0.45f, 0.45f, 1.0f);
            ed::Link(link.id, startNode->outputPin, endNode->inputPin, color, 3.0f);
        }
    }

    ImDrawList* arrowDrawList_ = ImGui::GetWindowDrawList();
    for (auto& link_ : graphTransitions)
    {
        if (link_.scopeId != currentScopeName()) continue;

        const AnimGraphNode* startNode_ = FindNodeByName(link_.fromStateName);
        const AnimGraphNode* endNode_ = FindNodeByName(link_.toStateName);
        if (!startNode_ || !endNode_ ||
            !isNodeInCurrentScope(*startNode_) ||
            !isNodeInCurrentScope(*endNode_))
        {
            continue;
        }

        const uintptr_t sourcePinId_ = static_cast<uintptr_t>(startNode_->outputPin.Get());
        const uintptr_t targetPinId_ = static_cast<uintptr_t>(endNode_->inputPin.Get());
        const auto sourcePosition_ = pinPositions_.find(sourcePinId_);
        const auto targetPosition_ = pinPositions_.find(targetPinId_);
        if (sourcePosition_ == pinPositions_.end() ||
            targetPosition_ == pinPositions_.end())
        {
            continue;
        }

        const ImU32 arrowColor_ =
            link_.fromStateName == runtimeActiveStateName ||
            link_.fromStateName == ANY_NODE_NAME
            ? IM_COL32(80, 165, 255, 255)
            : IM_COL32(120, 120, 120, 255);
        DrawLinkArrow(
            arrowDrawList_,
            sourcePosition_->second,
            targetPosition_->second,
            arrowColor_);
    }

    if (needsPositionRestore)
    {
        for (auto& node : graphNodes)
        {
            if (isNodeInCurrentScope(node))
                ed::SetNodePosition(node.id, node.pos);
        }

        ed::NavigateToContent();
        needsPositionRestore = false;
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
                            transition.id = ed::LinkId(GetNextId());
                            transition.scopeId = currentScopeName();
                            graphTransitions.push_back(transition);

                            useTransitionGraph = true;
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

                useTransitionGraph = true;
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

                useTransitionGraph = true;
            }
        }
    }
    ed::EndDelete();

    // ------------------------------------------------------------
    // 追加：選択中のリンクを Delete / Backspace で直接削除
    // BeginDelete 側の判定が拾いにくい場合の保険
    // ------------------------------------------------------------
    ed::LinkId selectedLinkId_;
    if (ed::GetSelectedLinks(&selectedLinkId_, 1) > 0)
    {
        const bool deletePressed_ =
            ImGui::IsKeyPressed(ImGuiKey_Delete) ||
            ImGui::IsKeyPressed(ImGuiKey_Backspace);

        if (deletePressed_)
        {
            const auto oldSize_ = graphTransitions.size();

            graphTransitions.erase(
                std::remove_if(graphTransitions.begin(), graphTransitions.end(),
                    [&](const auto& transition_)
                    {
                        return transition_.id == selectedLinkId_ &&
                            transition_.scopeId == currentScopeName();
                    }),
                graphTransitions.end());

            if (graphTransitions.size() != oldSize_)
            {
                ed::ClearSelection();
                useTransitionGraph = true;
            }
        }
    }


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
            graphScopeStack.push_back(currentScopeName());
            currentGraphScopeId = it->childScopeId;
            needsPositionRestore = true;
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

                    useTransitionGraph = true;
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
                if (ImGui::Combo("Logic", &logic, logicItems, IM_ARRAYSIZE(logicItems))) {
                    it->logic = static_cast<TransitionLogic>(logic);
                    useTransitionGraph = true;
                }
                if (ImGui::Button("+ Add Condition"))
                    it->conditions.push_back(AnimCondition());

                for (int i = 0; i < (int)it->conditions.size(); i++)
                {
                    ImGui::PushID(i);
                    auto& cond = it->conditions[i];

                    const char* condTypes[] = { "Variable", "AnimationEnd", "Trigger" };
                    int typeIdx = (int)cond.type;
                    if (ImGui::Combo("Type", &typeIdx, condTypes, IM_ARRAYSIZE(condTypes))) {
                        cond.type = (ConditionType)typeIdx;
                        useTransitionGraph = true;
                    }

                    if (cond.type == ConditionType::Variable || cond.type == ConditionType::Trigger)
                    {
                        if (!GetEditableBlackboard().empty())
                        {
                            std::vector<const char*> keys;
                            for (auto& [k, v] : GetEditableBlackboard()) keys.push_back(k.c_str());
                            int curIdx = 0;
                            for (int k = 0; k < (int)keys.size(); k++)
                                if (cond.parameterName == keys[k]) { curIdx = k; break; }
                            ImGui::SetNextItemWidth(160.0f);
                            if (ImGui::Combo("Parameter", &curIdx, keys.data(), (int)keys.size())) {
                                cond.parameterName = keys[curIdx];
                                useTransitionGraph = true;
                            }
                        }
                        else
                        {
                            if (ImGui::InputText("Parameter", &cond.parameterName)) {
                                useTransitionGraph = true;
                            }
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
                        {
                            cond.op = (ConditionOp)opIdx;
                            useTransitionGraph = true;
                        }
                        ImGui::SameLine();
                        if(ImGui::DragFloat("Threshold", &cond.threshold, 0.1f))
                        {
                            useTransitionGraph = true;
                        }
                    }

                    if (ImGui::Button("Remove Condition"))
                    {
                        it->conditions.erase(it->conditions.begin() + i);
                        --i;
                        useTransitionGraph = true;
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
        newNodeSpawnPosition = ed::ScreenToCanvas(ImGui::GetMousePos());
    }
    ed::Resume();

    ed::Suspend();
    if (ImGui::BeginPopup("Create New Node"))
    {
        ImGui::Text("Create State Node");
        ImGui::Separator();
        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputTextWithHint("##GraphStateSearch", "Search states...", &graphStateSearchText);
        ImGui::Separator();

        for (auto& [name, statePtr] : states)
        {
            if (!ContainsCaseInsensitive(name, graphStateSearchText)) continue;

            auto it = std::find_if(graphNodes.begin(), graphNodes.end(),
                [&](const auto& n) { return n.stateName == name; });

            if (it == graphNodes.end())
            {
                if (ImGui::MenuItem(name.c_str()))
                {
                    AnimGraphNode newNode;
                    newNode.id = ed::NodeId(GetNextId());
                    newNode.stateName = name;
                    newNode.inputPin = ed::PinId(GetNextId());
                    newNode.outputPin = ed::PinId(GetNextId());
                    newNode.type = AnimGraphNodeType::State;
                    newNode.scopeId = currentScopeName();
                    graphNodes.push_back(newNode);
                    nodeToPlaceId = (uintptr_t)newNode.id.Get();

                    useTransitionGraph = true;
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
                    nodeToPlaceId = (uintptr_t)n.id.Get();
                    break;
                }
            }

            useTransitionGraph = true;
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
            subGraphNode.id = ed::NodeId(GetNextId());
            subGraphNode.stateName = graphName;
            subGraphNode.childScopeId = graphName;
            subGraphNode.inputPin = ed::PinId(GetNextId());
            subGraphNode.outputPin = ed::PinId(GetNextId());
            subGraphNode.type = AnimGraphNodeType::SubGraph;
            subGraphNode.scopeId = currentScopeName();
            graphNodes.push_back(subGraphNode);
            nodeToPlaceId = (uintptr_t)subGraphNode.id.Get();

            useTransitionGraph = true;
        }
        ImGui::EndPopup();
    }
    ed::Resume();

    ed::End();
    ed::SetCurrentEditor(nullptr);
}
