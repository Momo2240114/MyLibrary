#pragma once

#include "EditorWindow.h"

class ActionSequenceEditorWindow : public EditorWindow
{
public:
    explicit ActionSequenceEditorWindow(GameObjectManager* manager_);
    void Draw() override;
};
