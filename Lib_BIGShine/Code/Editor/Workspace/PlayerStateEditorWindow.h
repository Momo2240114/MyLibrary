#pragma once

#include "EditorWorkspaceWindow.h"
#include "Game/Charcter/Player/PlayerState.h"

#include <memory>

class GameObject;
class PlayerControllerComponent;

namespace ax::NodeEditor
{
    struct EditorContext;
    struct Config;
}
//わたしのプロジェクトに合わせたステートグラフです
//行動のキャンセルを設定するためのWindowです
class PlayerStateEditorWindow : public EditorWorkspaceWindow
{
public:
    explicit PlayerStateEditorWindow(GameObjectManager* manager_);
    ~PlayerStateEditorWindow() override;

    void Draw() override;

private:
    PlayerControllerComponent* ResolveTarget();
    void DrawToolbar(PlayerControllerComponent* player_);
    void DrawStateList(PlayerControllerComponent* player_);
    void DrawCancelGraph(PlayerControllerComponent* player_);
    void DrawInspector(PlayerControllerComponent* player_);
    void ArrangeNodes();
    void SelectEditingAction(PLAYER_ACTION action_);

    static int GetNodeId(PLAYER_ACTION action_);
    static int GetInputPinId(PLAYER_ACTION action_);
    static int GetOutputPinId(PLAYER_ACTION action_);
    static int GetLinkId(PLAYER_ACTION from_, PLAYER_ACTION to_);

    std::weak_ptr<GameObject> targetObject;
    ax::NodeEditor::EditorContext* nodeEditorContext = nullptr;
    PLAYER_ACTION editingAction = PLAYER_ACTION::WEAK_ATTACK;
    float stateListWidth = 230.0f;
    float inspectorWidth = 320.0f;
    bool needsNodeLayout = true;
    bool editDodgeCancelRoutes = true;
};
