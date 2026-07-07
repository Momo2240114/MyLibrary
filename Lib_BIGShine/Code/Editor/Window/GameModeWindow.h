#pragma once

#include "EditorWindow.h"

class BlackboardComponent;
class GameModeManager;

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
    void DrawState(GameModeManager& gameModeManager_);
    void DrawGlobalBlackboard(BlackboardComponent& blackboard_);
};
