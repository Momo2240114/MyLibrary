#include "PlayerStateEditorWindow.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Game/Charcter/Player/PlayerControllerComponent.h"
#include "imgui_node_editor.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>

namespace ed = ax::NodeEditor;

namespace
{
    constexpr std::array<PLAYER_ACTION, 10> PLAYER_ACTIONS =
    {
        PLAYER_ACTION::IDLE,
        PLAYER_ACTION::MOVE,
        PLAYER_ACTION::WEAK_ATTACK,
        PLAYER_ACTION::STRONG_ATTACK,
        PLAYER_ACTION::GUARD,
        PLAYER_ACTION::JUST_GUARD,
        PLAYER_ACTION::DODGE,
        PLAYER_ACTION::SKILL_A,
        PLAYER_ACTION::SKILL_B,
        PLAYER_ACTION::COUNTER_ATTACK,
    };

    constexpr float PI = 3.14159265358979323846f;
    constexpr float SPLITTER_SIZE = 6.0f;

    const char* GetActionGroupName(PLAYER_ACTION_GROUP group_)
    {
        switch (group_)
        {
        case PLAYER_ACTION_GROUP::LOCOMOTION: return "Locomotion";
        case PLAYER_ACTION_GROUP::ATTACK: return "Attack";
        case PLAYER_ACTION_GROUP::GUARD: return "Guard";
        case PLAYER_ACTION_GROUP::DODGE: return "Dodge";
        case PLAYER_ACTION_GROUP::SKILL: return "Skill";
        default: return "None";
        }
    }

    void DrawVerticalSplitter(const char* id_, float height_, float& width_, float direction_)
    {
        ImGui::InvisibleButton(id_, ImVec2(SPLITTER_SIZE, height_));
        const bool hovered_ = ImGui::IsItemHovered();
        const bool active_ = ImGui::IsItemActive();
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetItemRectMin(),
            ImGui::GetItemRectMax(),
            ImGui::GetColorU32(
                hovered_ || active_ ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator));
        if (hovered_ || active_)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        if (active_)
        {
            width_ += ImGui::GetIO().MouseDelta.x * direction_;
        }
    }
}

PlayerStateEditorWindow::PlayerStateEditorWindow(GameObjectManager* manager_)
    : EditorWorkspaceWindow("Player State Graph", manager_)
{
    isVisible = false;
    style.backgroundColor = ImVec4(0.065f, 0.07f, 0.08f, 1.0f);
    style.titleBarColor = ImVec4(0.17f, 0.27f, 0.34f, 1.0f);

    ed::Config config_;
    config_.SettingsFile = nullptr;
    config_.DragButtonIndex = 0;
    config_.SelectButtonIndex = 0;
    config_.NavigateButtonIndex = 2;
    config_.ContextMenuButtonIndex = 1;
    nodeEditorContext = ed::CreateEditor(&config_);
}

PlayerStateEditorWindow::~PlayerStateEditorWindow()
{
    if (nodeEditorContext)
    {
        ed::DestroyEditor(nodeEditorContext);
        nodeEditorContext = nullptr;
    }
}

void PlayerStateEditorWindow::Draw()
{
    PlayerControllerComponent* player_ = ResolveTarget();

    if (!BeginWorkspaceWindow(ImGuiWindowFlags_MenuBar))
    {
        EndEditorWindow();
        return;
    }

    if (ImGui::BeginMenuBar())
    {
        DrawToolbar(player_);
        ImGui::EndMenuBar();
    }

    const ImVec2 available_ = ImGui::GetContentRegionAvail();
    stateListWidth = std::clamp(stateListWidth, 180.0f, (std::max)(180.0f, available_.x * 0.35f));
    inspectorWidth = std::clamp(inspectorWidth, 240.0f, (std::max)(240.0f, available_.x * 0.40f));
    const float graphWidth_ = (std::max)(240.0f,
        available_.x - stateListWidth - inspectorWidth - SPLITTER_SIZE * 2.0f);

    ImGui::BeginChild("PlayerStateList", ImVec2(stateListWidth, available_.y), true);
    DrawStateList(player_);
    ImGui::EndChild();

    ImGui::SameLine(0.0f, 0.0f);
    DrawVerticalSplitter("##PlayerStateListSplitter", available_.y, stateListWidth, 1.0f);
    ImGui::SameLine(0.0f, 0.0f);

    ImGui::BeginChild("PlayerCancelGraph", ImVec2(graphWidth_, available_.y), true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    DrawCancelGraph(player_);
    ImGui::EndChild();

    ImGui::SameLine(0.0f, 0.0f);
    DrawVerticalSplitter("##PlayerStateInspectorSplitter", available_.y, inspectorWidth, -1.0f);
    ImGui::SameLine(0.0f, 0.0f);

    ImGui::BeginChild("PlayerStateInspector", ImVec2(inspectorWidth, available_.y), true);
    DrawInspector(player_);
    ImGui::EndChild();

    EndEditorWindow();
}

PlayerControllerComponent* PlayerStateEditorWindow::ResolveTarget()
{
    if (!gameObjectManager) return nullptr;

    const std::shared_ptr<GameObject> selectedObject_ = gameObjectManager->GetSelectedObject();
    if (selectedObject_)
    {
        const std::shared_ptr<PlayerControllerComponent> selectedPlayer_ =
            selectedObject_->GetComponent<PlayerControllerComponent>();
        if (selectedPlayer_)
        {
            targetObject = selectedObject_;
            return selectedPlayer_.get();
        }
    }

    const std::shared_ptr<GameObject> currentTarget_ = targetObject.lock();
    if (currentTarget_)
    {
        const std::shared_ptr<PlayerControllerComponent> player_ =
            currentTarget_->GetComponent<PlayerControllerComponent>();
        if (player_) return player_.get();
    }

    for (const std::shared_ptr<GameObject>& object_ : gameObjectManager->GetObjects())
    {
        if (!object_) continue;
        const std::shared_ptr<PlayerControllerComponent> player_ =
            object_->GetComponent<PlayerControllerComponent>();
        if (!player_) continue;
        targetObject = object_;
        return player_.get();
    }

    return nullptr;
}

void PlayerStateEditorWindow::DrawToolbar(PlayerControllerComponent* player_)
{
    const std::shared_ptr<GameObject> target_ = targetObject.lock();
    ImGui::TextUnformatted("Target");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::BeginCombo(
        "##PlayerStateTarget",
        target_ ? target_->GetName().c_str() : "No Player Selected"))
    {
        for (const std::shared_ptr<GameObject>& object_ : gameObjectManager->GetObjects())
        {
            if (!object_ || !object_->GetComponent<PlayerControllerComponent>()) continue;
            const bool selected_ = object_ == target_;
            if (ImGui::Selectable(object_->GetName().c_str(), selected_))
            {
                targetObject = object_;
                gameObjectManager->SetSelectedObject(object_);
            }
            if (selected_) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::Separator();
    ImGui::Text("Editing: %s", GetPlayerActionName(editingAction));
    if (player_)
    {
        ImGui::Separator();
        ImGui::Text("Runtime: %s", GetPlayerActionName(player_->GetCurrentAction()));
    }
}

void PlayerStateEditorWindow::DrawStateList(PlayerControllerComponent* player_)
{
    ImGui::SeparatorText("Source State");
    if (!player_)
    {
        ImGui::TextWrapped("Select an object with PlayerControllerComponent.");
        return;
    }

    const PLAYER_ACTION runtimeAction_ = player_->GetCurrentAction();
    for (const PLAYER_ACTION action_ : PLAYER_ACTIONS)
    {
        const bool selected_ = editingAction == action_;
        if (ImGui::Selectable(GetPlayerActionName(action_), selected_, 0, ImVec2(0.0f, 34.0f)))
        {
            SelectEditingAction(action_);
        }
        if (runtimeAction_ == action_)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.25f, 0.78f, 1.0f, 1.0f), "Active");
        }
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Completion");
    ImGui::TextWrapped("ActionEnd completes every timeline-driven state and returns to buffered action or locomotion.");
}

void PlayerStateEditorWindow::DrawCancelGraph(PlayerControllerComponent* player_)
{
    ImGui::Text("%s Cancel Routes", GetPlayerActionName(editingAction));
    ImGui::Separator();
    if (!player_ || !nodeEditorContext)
    {
        ImGui::TextUnformatted("PlayerControllerComponent is not available.");
        return;
    }

    ed::SetCurrentEditor(nodeEditorContext);
    ed::Begin("PlayerCancelNodeEditor");

    for (const PLAYER_ACTION action_ : PLAYER_ACTIONS)
    {
        const bool isSource_ = action_ == editingAction;
        const bool isRuntime_ = action_ == player_->GetCurrentAction();
        constexpr float NODE_WIDTH = 220.0f;
        constexpr float NODE_HEIGHT = 125.0f;
        constexpr float NODE_ROUNDING = 18.0f;
        constexpr float PIN_RADIUS = 13.0f;
        const ImVec2 pinSize_(PIN_RADIUS * 2.0f, PIN_RADIUS * 2.0f);

        ed::PushStyleVar(ed::StyleVar_NodeRounding, NODE_ROUNDING);
        ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ed::PushStyleVar(ed::StyleVar_NodeBorderWidth, 0.0f);
        ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ed::PushStyleColor(ed::StyleColor_NodeBorder, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        ed::BeginNode(GetNodeId(action_));
        ImGui::PushID(static_cast<int>(action_));

        const ImVec2 nodeMinimum_ = ImGui::GetCursorScreenPos();
        const ImVec2 nodeMaximum_(
            nodeMinimum_.x + NODE_WIDTH,
            nodeMinimum_.y + NODE_HEIGHT);
        ImGui::Dummy(ImVec2(NODE_WIDTH, NODE_HEIGHT));

        ImDrawList* drawList_ = ImGui::GetWindowDrawList();
        const ImU32 fillColor_ = ImGui::GetColorU32(
            isSource_
            ? ImVec4(0.12f, 0.29f, 0.39f, 1.0f)
            : ImVec4(0.14f, 0.15f, 0.17f, 1.0f));
        const ImU32 borderColor_ = ImGui::GetColorU32(
            isRuntime_
            ? ImVec4(0.22f, 0.78f, 1.0f, 1.0f)
            : (isSource_
                ? ImVec4(0.30f, 0.68f, 0.82f, 1.0f)
                : ImVec4(0.48f, 0.50f, 0.54f, 1.0f)));
        drawList_->AddRectFilled(
            nodeMinimum_, nodeMaximum_, fillColor_, NODE_ROUNDING);
        drawList_->AddRect(
            nodeMinimum_, nodeMaximum_, borderColor_, NODE_ROUNDING,
            0, isRuntime_ || isSource_ ? 3.0f : 2.0f);

        const char* actionName_ = GetPlayerActionName(action_);
        ImFont* font_ = ImGui::GetFont();
        const float titleFontSize_ = ImGui::GetFontSize() * 1.45f;
        const ImVec2 titleSize_ = font_->CalcTextSizeA(
            titleFontSize_, FLT_MAX, 0.0f, actionName_);
        drawList_->AddText(
            font_,
            titleFontSize_,
            ImVec2(
                nodeMinimum_.x + (NODE_WIDTH - titleSize_.x) * 0.5f,
                nodeMinimum_.y + 22.0f),
            IM_COL32(240, 243, 247, 255),
            actionName_);

        const ImVec2 pinCenter_(
            nodeMinimum_.x + NODE_WIDTH * 0.5f,
            nodeMinimum_.y + NODE_HEIGHT - 30.0f);
        ImGui::SetCursorScreenPos(ImVec2(
            pinCenter_.x - PIN_RADIUS,
            pinCenter_.y - PIN_RADIUS));
        if (isSource_)
        {
            ed::BeginPin(GetOutputPinId(action_), ed::PinKind::Output);
            ImGui::Dummy(pinSize_);
            ed::EndPin();
        }
        else
        {
            ed::BeginPin(GetInputPinId(action_), ed::PinKind::Input);
            ImGui::Dummy(pinSize_);
            ed::EndPin();
        }
        const bool pinHovered_ = ImGui::IsItemHovered();
        const ImU32 pinColor_ = pinHovered_
            ? IM_COL32(255, 255, 255, 255)
            : (isSource_
                ? IM_COL32(232, 55, 64, 255)
                : IM_COL32(235, 216, 45, 255));
        drawList_->AddCircle(pinCenter_, PIN_RADIUS, pinColor_, 28, 4.0f);

        ImGui::SetCursorScreenPos(nodeMaximum_);
        ImGui::PopID();
        ed::EndNode();

        ed::PopStyleColor(2);
        ed::PopStyleVar(3);
    }

    for (const PlayerCancelTransition& transition_ : player_->GetCancelTransitions())
    {
        if (transition_.from != editingAction || transition_.to == editingAction) continue;
        ed::Link(
            GetLinkId(transition_.from, transition_.to),
            GetOutputPinId(transition_.from),
            GetInputPinId(transition_.to),
            ImVec4(0.94f, 0.30f, 0.32f, 1.0f),
            3.0f);
    }

    if (needsNodeLayout)
    {
        ArrangeNodes();
        ed::NavigateToContent(0.0f);
        needsNodeLayout = false;
    }

    if (ed::BeginCreate())
    {
        ed::PinId startPin_;
        ed::PinId endPin_;
        if (ed::QueryNewLink(&startPin_, &endPin_))
        {
            const int sourcePin_ = GetOutputPinId(editingAction);
            const int startPinValue_ = static_cast<int>(startPin_.Get());
            const int endPinValue_ = static_cast<int>(endPin_.Get());
            const int targetPin_ = startPinValue_ == sourcePin_ ? endPinValue_ : startPinValue_;
            PLAYER_ACTION targetAction_ = PLAYER_ACTION::NONE;
            for (const PLAYER_ACTION action_ : PLAYER_ACTIONS)
            {
                if (action_ != editingAction && GetInputPinId(action_) == targetPin_)
                {
                    targetAction_ = action_;
                    break;
                }
            }

            const bool valid_ = targetAction_ != PLAYER_ACTION::NONE &&
                (startPinValue_ == sourcePin_ || endPinValue_ == sourcePin_);
            if (!valid_ || player_->HasCancelTransition(editingAction, targetAction_))
            {
                ed::RejectNewItem(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), 2.0f);
            }
            else if (ed::AcceptNewItem(ImVec4(0.2f, 0.9f, 0.45f, 1.0f), 2.0f))
            {
                player_->AddCancelTransition(editingAction, targetAction_);
            }
        }
    }
    ed::EndCreate();

    if (ed::BeginDelete())
    {
        ed::LinkId linkId_;
        while (ed::QueryDeletedLink(&linkId_))
        {
            PLAYER_ACTION targetAction_ = PLAYER_ACTION::NONE;
            for (const PLAYER_ACTION action_ : PLAYER_ACTIONS)
            {
                if (GetLinkId(editingAction, action_) == static_cast<int>(linkId_.Get()))
                {
                    targetAction_ = action_;
                    break;
                }
            }

            if (targetAction_ == PLAYER_ACTION::NONE)
            {
                ed::RejectDeletedItem();
            }
            else if (ed::AcceptDeletedItem())
            {
                player_->RemoveCancelTransition(editingAction, targetAction_);
            }
        }
    }
    ed::EndDelete();

    ed::End();
    ed::SetCurrentEditor(nullptr);
}

void PlayerStateEditorWindow::DrawInspector(PlayerControllerComponent* player_)
{
    ImGui::SeparatorText("State Details");
    ImGui::Text("Source: %s", GetPlayerActionName(editingAction));
    ImGui::Text("Group: %s", GetActionGroupName(GetPlayerActionGroup(editingAction)));
    ImGui::Text("Timeline Driven: %s", IsPlayerActionTimelineDriven(editingAction) ? "Yes" : "No");

    if (!player_) return;

    ImGui::Spacing();
    ImGui::SeparatorText("Cancel Destinations");
    int routeCount_ = 0;
    for (const PlayerCancelTransition& transition_ : player_->GetCancelTransitions())
    {
        if (transition_.from != editingAction) continue;
        ++routeCount_;
        ImGui::PushID(static_cast<int>(transition_.to));
        ImGui::TextUnformatted(GetPlayerActionName(transition_.to));
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove"))
        {
            player_->RemoveCancelTransition(editingAction, transition_.to);
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    if (routeCount_ == 0)
    {
        ImGui::TextDisabled("No cancel route");
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Runtime Gate");
    ImGui::TextWrapped("Routes are available only while CanCancel is enabled by the animation sequencer.");
}

void PlayerStateEditorWindow::ArrangeNodes()
{
    ed::SetNodePosition(GetNodeId(editingAction), ImVec2(0.0f, 0.0f));

    int targetIndex_ = 0;
    constexpr int TARGET_COUNT = static_cast<int>(PLAYER_ACTIONS.size()) - 1;
    for (const PLAYER_ACTION action_ : PLAYER_ACTIONS)
    {
        if (action_ == editingAction) continue;
        const float angle_ = -PI * 0.5f + PI * 2.0f *
            static_cast<float>(targetIndex_) / static_cast<float>(TARGET_COUNT);
        const ImVec2 position_(
            std::cos(angle_) * 390.0f,
            std::sin(angle_) * 250.0f);
        ed::SetNodePosition(GetNodeId(action_), position_);
        ++targetIndex_;
    }
}

void PlayerStateEditorWindow::SelectEditingAction(PLAYER_ACTION action_)
{
    if (editingAction == action_) return;
    editingAction = action_;
    needsNodeLayout = true;
}

int PlayerStateEditorWindow::GetNodeId(PLAYER_ACTION action_)
{
    return 100 + static_cast<int>(action_);
}

int PlayerStateEditorWindow::GetInputPinId(PLAYER_ACTION action_)
{
    return 1000 + static_cast<int>(action_);
}

int PlayerStateEditorWindow::GetOutputPinId(PLAYER_ACTION action_)
{
    return 2000 + static_cast<int>(action_);
}

int PlayerStateEditorWindow::GetLinkId(PLAYER_ACTION from_, PLAYER_ACTION to_)
{
    return 3000 + static_cast<int>(from_) * 32 + static_cast<int>(to_);
}
