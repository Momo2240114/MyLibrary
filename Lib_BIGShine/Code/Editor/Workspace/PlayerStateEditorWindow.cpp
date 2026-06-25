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
#include "ParticleEditorWindow.h"

#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Core/RenderContext.h"
#include "ImSequencer.h"
#include "imgui_internal.h"

#include <algorithm>
#include <DirectXMath.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace
{
    constexpr float TIMELINE_FPS = 60.0f;
}

ParticleEditorWindow::ParticleEditorWindow(GameObjectManager* manager_)
    : EditorWorkspaceWindow("VFX Editor", manager_),
    assetBrowser(manager_)
{
    isVisible = false;
    style.backgroundColor = ImVec4(0.08f, 0.09f, 0.10f, 1.0f);
    style.titleBarColor = ImVec4(0.10f, 0.34f, 0.18f, 1.0f);
}

void ParticleEditorWindow::Draw()
{
    EnsurePreviewObjects();

    if (!BeginWorkspaceWindow(ImGuiWindowFlags_MenuBar))
    {
        EndEditorWindow();
        return;
    }

    if (ImGui::BeginMenuBar())
    {
        DrawToolbar();
        ImGui::EndMenuBar();
    }

    const float splitterSize_ = 6.0f;
    const float minimumLeftWidth_ = 280.0f;
    const float minimumInspectorWidth_ = 240.0f;
    const float minimumPreviewHeight_ = 160.0f;
    const float minimumTimelineHeight_ = 120.0f;
    const float verticalSpacing_ = ImGui::GetStyle().ItemSpacing.y * 2.0f;
    ImVec2 available_ = ImGui::GetContentRegionAvail();
    inspectorWidth = std::clamp(
        inspectorWidth,
        minimumInspectorWidth_,
        (std::max)(minimumInspectorWidth_, available_.x - minimumLeftWidth_ - splitterSize_));
    timelineHeight = std::clamp(
        timelineHeight,
        minimumTimelineHeight_,
        (std::max)(
            minimumTimelineHeight_,
            available_.y - minimumPreviewHeight_ - splitterSize_ - verticalSpacing_));

    const float leftWidth_ = (std::max)(minimumLeftWidth_, available_.x - inspectorWidth - splitterSize_);
    const float previewHeight_ =
        (std::max)(
            minimumPreviewHeight_,
            available_.y - timelineHeight - splitterSize_ - verticalSpacing_);

    ImGui::BeginGroup();
    ImGui::BeginChild("ParticlePreviewPanel", ImVec2(leftWidth_, previewHeight_), true);
    DrawPreviewPanel();
    ImGui::EndChild();

    ImGui::InvisibleButton("##VFXHorizontalSplitter", ImVec2(leftWidth_, splitterSize_));
    const bool isHorizontalSplitterHovered_ = ImGui::IsItemHovered();
    const bool isHorizontalSplitterActive_ = ImGui::IsItemActive();
    const ImU32 horizontalSplitterColor_ = ImGui::GetColorU32(
        isHorizontalSplitterHovered_ || isHorizontalSplitterActive_
        ? ImGuiCol_SeparatorHovered
        : ImGuiCol_Separator);
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImGui::GetItemRectMin(),
        ImGui::GetItemRectMax(),
        horizontalSplitterColor_);
    if (isHorizontalSplitterHovered_ || isHorizontalSplitterActive_)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    if (isHorizontalSplitterActive_)
    {
        timelineHeight -= ImGui::GetIO().MouseDelta.y;
    }

    ImGui::BeginChild("VFXTimelinePanel", ImVec2(leftWidth_, timelineHeight), true);
    DrawTimelinePanel();
    ImGui::EndChild();
    ImGui::EndGroup();

    ImGui::SameLine(0.0f, 0.0f);

    ImGui::InvisibleButton("##VFXVerticalSplitter", ImVec2(splitterSize_, available_.y));
    const bool isVerticalSplitterHovered_ = ImGui::IsItemHovered();
    const bool isVerticalSplitterActive_ = ImGui::IsItemActive();
    const ImU32 verticalSplitterColor_ = ImGui::GetColorU32(
        isVerticalSplitterHovered_ || isVerticalSplitterActive_
        ? ImGuiCol_SeparatorHovered
        : ImGuiCol_Separator);
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImGui::GetItemRectMin(),
        ImGui::GetItemRectMax(),
        verticalSplitterColor_);
    if (isVerticalSplitterHovered_ || isVerticalSplitterActive_)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    if (isVerticalSplitterActive_)
    {
        inspectorWidth -= ImGui::GetIO().MouseDelta.x;
    }

    ImGui::SameLine(0.0f, 0.0f);

    ImGui::BeginChild("ParticleInspectorPanel", ImVec2(inspectorWidth, available_.y), true);
    DrawInspectorPanel();
    ImGui::EndChild();

    EndEditorWindow();
}

void ParticleEditorWindow::EnsurePreviewObjects()
{
    if (isInitialized) return;

    auto& graphics_ = GraphicsDeviceManager::Instance();
    modelRenderSystem.Initialize(graphics_.GetDevice());
    particleRenderSystem.Initialize(graphics_.GetDevice());
    meshVFXRenderSystem.Initialize(graphics_.GetDevice());

    previewObject = std::make_shared<GameObject>("VFXPreviewObject");
    previewObject->transform->gameObject = previewObject;
    previewModel = previewObject->AddComponent<GltfModelCommponennt>();
    previewParticle = previewObject->AddComponent<ParticleComponent>();
    previewMeshVFX = previewObject->AddComponent<MeshVFXComponent>();
    if (previewParticle)
    {
        previewParticle->RefreshTextureList();
    }
    if (previewMeshVFX)
    {
        previewMeshVFX->Stop();
    }

    previewCamera.SetLookAt({ 0.0f, 1.5f, -6.0f }, { 0.0f, 0.4f, 0.0f }, { 0.0f, 1.0f, 0.0f });
    previewCamera.SetPerspectiveFov(45.0f, 16.0f / 9.0f, 0.01f, 100.0f);

    RefreshParticleAssets();
    RefreshSequenceAssets();
    isInitialized = true;
}

void ParticleEditorWindow::DrawToolbar()
{
    if (ImGui::MenuItem("Refresh Assets"))
    {
        RefreshParticleAssets();
        RefreshSequenceAssets();
    }

    ImGui::Separator();

    if (ImGui::MenuItem(sequencePlaying ? "Pause" : "Play"))
    {
        sequencePlaying = !sequencePlaying;
    }
    if (ImGui::MenuItem("Stop"))
    {
        sequencePlaying = false;
        ResetSequence();
    }
    if (ImGui::MenuItem("Reset"))
    {
        ResetSequence();
    }
    ImGui::MenuItem("Loop", nullptr, &sequenceLoop);
    ImGui::MenuItem("Grid", nullptr, &showGrid);
    if (ImGui::MenuItem("Reset Layout"))
    {
        inspectorWidth = 360.0f;
        timelineHeight = 190.0f;
    }
}

void ParticleEditorWindow::DrawPreviewPanel()
{
    ImGui::TextUnformatted("Preview");
    ImGui::Separator();

    ImVec2 previewSize_ = ImGui::GetContentRegionAvail();
    previewSize_.x = (std::max)(1.0f, previewSize_.x);
    previewSize_.y = (std::max)(1.0f, previewSize_.y);

    RenderPreview(previewSize_);

    if (previewTarget.GetShaderResourceView())
    {
        ImGui::Image(reinterpret_cast<void*>(previewTarget.GetShaderResourceView()), previewSize_);
    }
    else
    {
        ImGui::Dummy(previewSize_);
    }
}

void ParticleEditorWindow::DrawAssetBrowserPanel()
{
    ImGui::TextUnformatted("VFX Sequences");
    ImGui::Separator();

    ImGui::InputText("Sequence Name", sequenceNameBuffer, IM_ARRAYSIZE(sequenceNameBuffer));
    if (ImGui::Button("Save Sequence"))
    {
        SaveSequence("Data/VFXSequence/" + std::string(sequenceNameBuffer) + ".json");
        RefreshSequenceAssets();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Sequence") &&
        selectedSequenceIndex >= 0 &&
        selectedSequenceIndex < static_cast<int>(sequenceAssetPaths.size()))
    {
        LoadSequence(sequenceAssetPaths[selectedSequenceIndex]);
    }

    for (int sequenceIndex_ = 0;
        sequenceIndex_ < static_cast<int>(sequenceAssetPaths.size());
        ++sequenceIndex_)
    {
        const std::filesystem::path path_(sequenceAssetPaths[sequenceIndex_]);
        if (ImGui::Selectable(path_.filename().string().c_str(), selectedSequenceIndex == sequenceIndex_))
        {
            selectedSequenceIndex = sequenceIndex_;
        }
    }

    ImGui::SeparatorText("Particle Assets");
    if (ImGui::Button("Refresh Assets"))
    {
        RefreshParticleAssets();
        RefreshSequenceAssets();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Particle") &&
        previewParticle &&
        selectedAssetIndex >= 0 &&
        selectedAssetIndex < static_cast<int>(particleAssetPaths.size()))
    {
        previewParticle->LoadAsset(particleAssetPaths[selectedAssetIndex]);
        previewParticle->Play();
    }

    if (particleAssetPaths.empty())
    {
        ImGui::TextDisabled("No particle json assets found.");
        return;
    }

    for (int i_ = 0; i_ < static_cast<int>(particleAssetPaths.size()); ++i_)
    {
        const std::filesystem::path path_(particleAssetPaths[i_]);
        if (ImGui::Selectable(path_.filename().string().c_str(), selectedAssetIndex == i_))
        {
            selectedAssetIndex = i_;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && previewParticle)
        {
            previewParticle->LoadAsset(particleAssetPaths[i_]);
            previewParticle->Play();
        }
    }
}

void ParticleEditorWindow::DrawTimelinePanel()
{
    struct TimelineSequenceAdapter : public ImSequencer::SequenceInterface
    {
        std::vector<TimelineTrack>* tracks = nullptr;
        ParticleComponent* particle = nullptr;
        float duration = 0.0f;

        int ToFrame(float time_) const { return static_cast<int>(time_ * TIMELINE_FPS); }
        float ToTime(int frame_) const { return static_cast<float>(frame_) / TIMELINE_FPS; }

        int GetFrameMin() const override { return 0; }
        int GetFrameMax() const override { return (std::max)(1, ToFrame(duration)); }
        int GetItemCount() const override { return tracks ? static_cast<int>(tracks->size()) : 0; }
        int GetItemTypeCount() const override { return 2; }
        const char* GetItemTypeName(int typeIndex_) const override
        {
            return typeIndex_ == 0 ? "Particle Emitter" : "Mesh VFX";
        }

        void BeginEdit(int index_) override {}

        void EndEdit() override
        {
            if (!tracks) return;

            for (TimelineTrack& track_ : *tracks)
            {
                track_.frameStart = std::clamp(track_.frameStart, 0, GetFrameMax() - 1);
                track_.frameEnd = std::clamp(track_.frameEnd, track_.frameStart + 1, GetFrameMax());
                track_.startTime = ToTime(track_.frameStart);
                track_.duration = ToTime(track_.frameEnd - track_.frameStart);
            }
        }

        void Get(int index_, int** start_, int** end_, int* type_, unsigned int* color_) override
        {
            if (!tracks || index_ < 0 || index_ >= static_cast<int>(tracks->size())) return;

            TimelineTrack& track_ = (*tracks)[index_];
            track_.frameStart = ToFrame(track_.startTime);
            track_.frameEnd = ToFrame(track_.startTime + track_.duration);
            if (start_) *start_ = &track_.frameStart;
            if (end_) *end_ = &track_.frameEnd;
            if (type_) *type_ = static_cast<int>(track_.type);
            if (color_)
            {
                *color_ = track_.type == TimelineTrackType::PARTICLE_EMITTER
                    ? 0xFF4FA3FF
                    : 0xFFFF8A4F;
            }
        }

        const char* GetItemLabel(int index_) const override
        {
            if (!tracks || index_ < 0 || index_ >= static_cast<int>(tracks->size())) return "";
            const TimelineTrack& track_ = (*tracks)[index_];
            return track_.targetName.empty()
                ? (track_.type == TimelineTrackType::PARTICLE_EMITTER ? "Particle Emitter" : "Mesh VFX")
                : track_.targetName.c_str();
        }

        void CustomDraw(
            int index_,
            ImDrawList* drawList_,
            const ImRect& rect_,
            const ImRect& legendRect_,
            const ImRect& clippingRect_,
            const ImRect& legendClippingRect_) override
        {
            drawList_->PushClipRect(rect_.Min, rect_.Max, true);
            drawList_->AddText(
                ImVec2(rect_.Min.x + 3.0f, rect_.Min.y + 2.0f),
                0xFFFFFFFF,
                GetItemLabel(index_));
            drawList_->PopClipRect();
        }

        void Add(int type_) override
        {
            if (!tracks) return;

            TimelineTrack track_{};
            track_.type = type_ == 0
                ? TimelineTrackType::PARTICLE_EMITTER
                : TimelineTrackType::MESH_VFX;
            if (track_.type == TimelineTrackType::PARTICLE_EMITTER)
            {
                if (particle && !particle->GetTracks().empty())
                {
                    track_.targetName = particle->GetTracks().front().name;
                }
            }
            else
            {
                track_.targetName = "Mesh VFX";
            }

            track_.startTime = 0.0f;
            track_.duration = (std::min)(1.0f, duration);
            tracks->push_back(track_);
        }

        void Del(int index_) override
        {
            if (!tracks || index_ < 0 || index_ >= static_cast<int>(tracks->size())) return;
            tracks->erase(tracks->begin() + index_);
        }
    };

    ImGui::Text("VFX Timeline  %.2f / %.2f", sequenceTime, sequenceDuration);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    if (ImGui::DragFloat("Duration", &sequenceDuration, 0.05f, 0.1f, 600.0f))
    {
        sequenceDuration = (std::max)(0.1f, sequenceDuration);
        const float maximumStartTime_ = (std::max)(0.0f, sequenceDuration - (1.0f / TIMELINE_FPS));
        for (TimelineTrack& track_ : timelineTracks)
        {
            track_.startTime = std::clamp(track_.startTime, 0.0f, maximumStartTime_);
            track_.duration = std::clamp(
                track_.duration,
                1.0f / TIMELINE_FPS,
                sequenceDuration - track_.startTime);
        }
    }

    TimelineSequenceAdapter adapter_;
    adapter_.tracks = &timelineTracks;
    adapter_.particle = previewParticle.get();
    adapter_.duration = sequenceDuration;

    int currentFrame_ = adapter_.ToFrame(sequenceTime);
    const int previousFrame_ = currentFrame_;
    ImGui::PushItemWidth(-1.0f);
    ImSequencer::Sequencer(
        &adapter_,
        &currentFrame_,
        &timelineExpanded,
        &selectedTimelineTrack,
        &firstTimelineFrame,
        ImSequencer::SEQUENCER_EDIT_ALL |
        ImSequencer::SEQUENCER_ADD |
        ImSequencer::SEQUENCER_DEL |
        ImSequencer::SEQUENCER_CHANGE_FRAME);
    ImGui::PopItemWidth();

    if (currentFrame_ != previousFrame_)
    {
        sequencePlaying = false;
        ResetSequence();
        sequenceTime = std::clamp(adapter_.ToTime(currentFrame_), 0.0f, sequenceDuration);
    }

    if (selectedTimelineTrack < 0 ||
        selectedTimelineTrack >= static_cast<int>(timelineTracks.size()))
    {
        ImGui::TextDisabled("Add or select a Particle / Mesh VFX bar to edit it.");
        return;
    }

    TimelineTrack& selectedTrack_ = timelineTracks[selectedTimelineTrack];
    ImGui::SeparatorText("Selected Track");
    const char* trackTypes_[] = { "Particle Emitter", "Mesh VFX" };
    int selectedType_ = static_cast<int>(selectedTrack_.type);
    if (ImGui::Combo("Type", &selectedType_, trackTypes_, IM_ARRAYSIZE(trackTypes_)))
    {
        selectedTrack_.type = static_cast<TimelineTrackType>(selectedType_);
        selectedTrack_.targetName = selectedTrack_.type == TimelineTrackType::MESH_VFX
            ? "Mesh VFX"
            : "";
    }

    if (selectedTrack_.type == TimelineTrackType::PARTICLE_EMITTER && previewParticle)
    {
        if (ImGui::BeginCombo("Emitter", selectedTrack_.targetName.c_str()))
        {
            for (const auto& emitterTrack_ : previewParticle->GetTracks())
            {
                if (ImGui::Selectable(
                    emitterTrack_.name.c_str(),
                    selectedTrack_.targetName == emitterTrack_.name))
                {
                    selectedTrack_.targetName = emitterTrack_.name;
                }
            }
            ImGui::EndCombo();
        }
    }

    float endTime_ = selectedTrack_.startTime + selectedTrack_.duration;
    bool timeChanged_ = ImGui::DragFloat("Start Time", &selectedTrack_.startTime, 0.01f, 0.0f, sequenceDuration);
    timeChanged_ |= ImGui::DragFloat("End Time", &endTime_, 0.01f, 0.0f, sequenceDuration);
    if (timeChanged_)
    {
        const float maximumStartTime_ = (std::max)(0.0f, sequenceDuration - (1.0f / TIMELINE_FPS));
        selectedTrack_.startTime = std::clamp(selectedTrack_.startTime, 0.0f, maximumStartTime_);
        endTime_ = std::clamp(endTime_, selectedTrack_.startTime + (1.0f / TIMELINE_FPS), sequenceDuration);
        selectedTrack_.duration = endTime_ - selectedTrack_.startTime;
    }
}

void ParticleEditorWindow::DrawInspectorPanel()
{
    ImGui::TextUnformatted("VFX Assets / Inspector");
    ImGui::Separator();

    if (!previewParticle || !previewMeshVFX)
    {
        ImGui::TextDisabled("Preview particle is not ready.");
        return;
    }

    const float splitterSize_ = 6.0f;
    const float availableHeight_ = ImGui::GetContentRegionAvail().y;
    assetBrowserHeight = std::clamp(assetBrowserHeight, 140.0f, (std::max)(140.0f, availableHeight_ - 180.0f));
    const float inspectorHeight_ = (std::max)(180.0f, availableHeight_ - assetBrowserHeight - splitterSize_);

    ImGui::BeginChild("VFXInspectorContent", ImVec2(0.0f, inspectorHeight_), false);
    if (ImGui::BeginTabBar("VFXInspectorTabs"))
    {
        if (ImGui::BeginTabItem("Assets"))
        {
            DrawAssetBrowserPanel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Particle"))
        {
            previewParticle->Gui();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Mesh VFX"))
        {
            previewMeshVFX->Gui();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Source Model"))
        {
            if (previewObject->transform) previewObject->transform->Gui();
            ImGui::Separator();
            if (previewModel) previewModel->Gui();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    ImGui::InvisibleButton("##VFXAssetBrowserSplitter", ImVec2(-1.0f, splitterSize_));
    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    if (ImGui::IsItemActive())
    {
        assetBrowserHeight -= ImGui::GetIO().MouseDelta.y;
    }

    ImGui::BeginChild("VFXEmbeddedAssetBrowser", ImVec2(0.0f, assetBrowserHeight), true);
    assetBrowser.DrawContent();
    ImGui::EndChild();
}

void ParticleEditorWindow::RenderPreview(const ImVec2& previewSize_)
{
    auto& graphics_ = GraphicsDeviceManager::Instance();
    ID3D11Device* device_ = graphics_.GetDevice();
    ID3D11DeviceContext* context_ = graphics_.GetContext();
    if (!device_ || !context_ || !previewParticle || !previewMeshVFX) return;

    const int width_ = (std::max)(1, static_cast<int>(previewSize_.x));
    const int height_ = (std::max)(1, static_cast<int>(previewSize_.y));
    if (!previewTarget.Resize(device_, width_, height_)) return;

    previewCamera.SetPerspectiveFov(45.0f, static_cast<float>(width_) / static_cast<float>(height_), 0.01f, 100.0f);

    const float clearColor_[4] = { 0.025f, 0.028f, 0.032f,0.0f };
    previewTarget.Begin(context_, clearColor_);

    const float deltaTime_ = ImGui::GetIO().DeltaTime;

    if (previewObject && previewObject->transform)
    {
        // Keep the preview world matrix synchronized with inspector edits.
        previewObject->transform->UpdateWorldMatrix();
    }

    if (previewModel)
    {
        previewModel->Update(deltaTime_);
    }
    UpdateSequence(deltaTime_);
    previewParticle->Update(deltaTime_);
    previewMeshVFX->Update(deltaTime_);

    RenderContext renderContext_{};
    renderContext_.device = device_;
    renderContext_.deviceContext = context_;
    renderContext_.renderState = graphics_.GetRenderState();
    renderContext_.camera = &previewCamera;
    renderContext_.backBufferRTV = nullptr;
    renderContext_.depthSRV = previewTarget.GetDepthShaderResourceView();
    renderContext_.baseColorSRV = previewTarget.GetShaderResourceView();
    renderContext_.screenWidth = static_cast<float>(width_);
    renderContext_.screenHeight = static_cast<float>(height_);

    if (previewModel && !previewModel->GetMeshes().empty())
    {
        modelRenderSystem.PushSubmit(previewModel->GetShaderID(), previewModel);
        modelRenderSystem.RenderAll(renderContext_);
        previewTarget.ResetRenderTarget(context_);
    }
    else
    {
        particleRenderSystem.ApplySceneConstants(renderContext_);
        graphics_.RenderSkyBox(renderContext_);
    }

    particleRenderSystem.PushSubmit(previewParticle.get());
    particleRenderSystem.RenderAll(renderContext_, true);
    previewTarget.ResetRenderTarget(context_);
    meshVFXRenderSystem.PushSubmit(previewMeshVFX.get());
    meshVFXRenderSystem.RenderAll(
        renderContext_,
        true,
        previewTarget.GetRenderTargetView(),
        previewTarget.GetDepthStencilView());

    previewTarget.End(context_);
}

void ParticleEditorWindow::UpdateSequence(float deltaTime_)
{
    if (!sequencePlaying) return;

    sequenceTime += deltaTime_;
    for (TimelineTrack& track_ : timelineTracks)
    {
        const bool shouldBeActive_ =
            sequenceTime >= track_.startTime &&
            sequenceTime < track_.startTime + track_.duration;

        if (shouldBeActive_ && !track_.active)
        {
            track_.active = true;
            if (track_.type == TimelineTrackType::PARTICLE_EMITTER && previewParticle)
            {
                previewParticle->TriggerEmitter(track_.targetName);
            }
            else if (track_.type == TimelineTrackType::MESH_VFX && previewMeshVFX)
            {
                previewMeshVFX->Reset();
                previewMeshVFX->Play();
            }
        }
        else if (!shouldBeActive_ && track_.active)
        {
            track_.active = false;
            if (track_.type == TimelineTrackType::PARTICLE_EMITTER && previewParticle)
            {
                previewParticle->StopEmitter(track_.targetName);
            }
            else if (track_.type == TimelineTrackType::MESH_VFX && previewMeshVFX)
            {
                previewMeshVFX->Stop();
            }
        }
    }

    if (sequenceTime >= sequenceDuration)
    {
        const bool shouldContinue_ = sequenceLoop;
        ResetSequence();
        sequencePlaying = shouldContinue_;
    }
}

void ParticleEditorWindow::ResetSequence()
{
    sequenceTime = 0.0f;
    if (previewParticle) previewParticle->Stop();
    if (previewMeshVFX)
    {
        previewMeshVFX->Stop();
        previewMeshVFX->Reset();
    }
    for (TimelineTrack& track_ : timelineTracks)
    {
        track_.active = false;
    }
}

bool ParticleEditorWindow::SaveSequence(const std::string& path_)
{
    if (path_.empty()) return false;

    nlohmann::json sequenceJson_;
    sequenceJson_["AssetType"] = "VFXSequence";
    sequenceJson_["AssetVersion"] = 1;
    sequenceJson_["Duration"] = sequenceDuration;
    sequenceJson_["Loop"] = sequenceLoop;
    sequenceJson_["Tracks"] = nlohmann::json::array();

    for (const TimelineTrack& track_ : timelineTracks)
    {
        sequenceJson_["Tracks"].push_back(
            {
                { "Type", static_cast<int>(track_.type) },
                { "Target", track_.targetName },
                { "StartTime", track_.startTime },
                { "Duration", track_.duration },
            });
    }

    if (previewParticle)
    {
        previewParticle->Serialize(sequenceJson_["Particle"]);
        sequenceJson_["Particle"]["IsPlaying"] = false;
    }
    if (previewMeshVFX)
    {
        previewMeshVFX->Serialize(sequenceJson_["MeshVFX"]);
    }

    const std::filesystem::path sequencePath_(path_);
    std::filesystem::create_directories(sequencePath_.parent_path());
    std::ofstream file_(sequencePath_, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) return false;

    file_ << sequenceJson_.dump(4);
    return true;
}

bool ParticleEditorWindow::LoadSequence(const std::string& path_)
{
    if (path_.empty() || !std::filesystem::exists(path_)) return false;

    std::ifstream file_(path_);
    if (!file_.is_open()) return false;

    nlohmann::json sequenceJson_;
    file_ >> sequenceJson_;
    if (sequenceJson_.value("AssetType", "") != "VFXSequence") return false;

    sequencePlaying = false;
    ResetSequence();
    sequenceDuration = (std::max)(0.1f, sequenceJson_.value("Duration", 5.0f));
    sequenceLoop = sequenceJson_.value("Loop", true);
    timelineTracks.clear();

    if (sequenceJson_.contains("Tracks") && sequenceJson_["Tracks"].is_array())
    {
        for (const auto& trackJson_ : sequenceJson_["Tracks"])
        {
            TimelineTrack track_{};
            track_.type = static_cast<TimelineTrackType>(
                std::clamp(trackJson_.value("Type", 0), 0, 1));
            track_.targetName = trackJson_.value("Target", "");
            track_.startTime = (std::max)(0.0f, trackJson_.value("StartTime", 0.0f));
            track_.duration = (std::max)(0.01f, trackJson_.value("Duration", 1.0f));
            timelineTracks.push_back(track_);
        }
    }

    if (previewParticle && sequenceJson_.contains("Particle"))
    {
        previewParticle->Deserialize(sequenceJson_["Particle"]);
    }
    if (previewMeshVFX && sequenceJson_.contains("MeshVFX"))
    {
        previewMeshVFX->Deserialize(sequenceJson_["MeshVFX"]);
        previewMeshVFX->Stop();
    }

    const std::string sequenceName_ = std::filesystem::path(path_).stem().string();
    strncpy_s(sequenceNameBuffer, sequenceName_.c_str(), sizeof(sequenceNameBuffer) - 1);
    sequenceNameBuffer[sizeof(sequenceNameBuffer) - 1] = '\0';
    selectedTimelineTrack = -1;
    return true;
}

void ParticleEditorWindow::RefreshParticleAssets()
{
    particleAssetPaths.clear();
    const std::filesystem::path particleDir_("Data/Particle");
    if (!std::filesystem::exists(particleDir_)) return;

    for (const auto& entry_ : std::filesystem::directory_iterator(particleDir_))
    {
        if (!entry_.is_regular_file()) continue;
        if (entry_.path().extension() == ".json")
        {
            particleAssetPaths.push_back(entry_.path().generic_string());
        }
    }
}

void ParticleEditorWindow::RefreshSequenceAssets()
{
    sequenceAssetPaths.clear();
    const std::filesystem::path sequenceDirectory_("Data/VFXSequence");
    if (!std::filesystem::exists(sequenceDirectory_)) return;

    for (const auto& entry_ : std::filesystem::directory_iterator(sequenceDirectory_))
    {
        if (!entry_.is_regular_file() || entry_.path().extension() != ".json") continue;
        sequenceAssetPaths.push_back(entry_.path().generic_string());
    }
}
