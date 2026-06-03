// EditorWindow.h
#pragma once
#include <string>
#include <memory>
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"

struct WindowStyle {
    ImVec4 backgroundColor = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
    ImVec4 titleBarColor = ImVec4(0.2f, 0.3f, 0.4f, 1.0f);
};

class EditorWindow {
public:
    EditorWindow(const std::string& name, GameObjectManager* manager)
        : windowName(name), gameObjectManager(manager), isVisible(true) 
    {
    }
    virtual ~EditorWindow() = default;

    virtual void Draw() = 0; // 各ウィンドウ固有の描画
    virtual void DrawMenuBar() {}

    // この窓が表示されているかを確認し、設定する術
    bool IsVisible() const { return isVisible; }
    void SetVisible(bool visible) { isVisible = visible; }

    const std::string& GetName() const { return windowName; }
    WindowStyle& GetStyle() { return style; }
protected:
    WindowStyle style;
    // 共通のスタイル編集UIを描画する関数
    void DrawStylePopup() {
        if (ImGui::BeginPopupContextItem("WindowSettings")) {
            ImGui::Text("Style Settings");
            ImGui::Separator();
            ImGui::ColorEdit4("Background", (float*)&style.backgroundColor);
            ImGui::ColorEdit4("Title Bar", (float*)&style.titleBarColor);

            if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

    std::string windowName;
    GameObjectManager* gameObjectManager; // データを扱うための窓口
    bool isVisible; // 顕現のフラグ
};