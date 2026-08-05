#pragma once

#include "EditorWindow.h"

// ActionSequence Assetの作成・確認・調整を行うEditor Window。
class ActionSequenceEditorWindow : public EditorWindow
{
public:
    explicit ActionSequenceEditorWindow(GameObjectManager* manager_);
    void Draw() override;
};
