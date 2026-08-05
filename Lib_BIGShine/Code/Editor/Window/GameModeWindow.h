#pragma once

#include "EditorWindow.h"

class BlackboardComponent;
class GameModeManager;

// 現在のGameMode状態とGlobal Blackboardを確認・編集するWindow。
class GameModeWindow : public EditorWindow
{
public:
    explicit GameModeWindow(GameObjectManager* manager_)
        : EditorWindow("Game Mode", manager_)
    {
        defaultWindowSize = ImVec2(420.0f, 520.0f);
    }

    void Draw() override;

private:
    // GameModeManagerが持つ現在状態や遷移情報を表示する。
    void DrawState(GameModeManager& gameModeManager_);
    // Game全体で共有するBlackboard値をEditorから確認・調整する。
    void DrawGlobalBlackboard(BlackboardComponent& blackboard_);
};
