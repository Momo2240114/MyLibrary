// EditorWindow.h
#pragma once
#include <string>
#include <memory>
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"

class EditorWindow {
public:
    EditorWindow(const std::string& name, GameObjectManager* manager)
        : windowName(name), gameObjectManager(manager) {
    }
    virtual ~EditorWindow() = default;

    virtual void Draw() = 0; // 各ウィンドウ固有の描画
    const std::string& GetName() const { return windowName; }

protected:
    std::string windowName;
    GameObjectManager* gameObjectManager; // データを扱うための窓口
};