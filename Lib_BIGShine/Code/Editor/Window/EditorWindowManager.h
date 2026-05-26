// EditorWindowManager.h
#include <vector>
#include <memory>
#include "EditorWindow.h"

class EditorWindowManager {
public:
    void AddWindow(std::unique_ptr<EditorWindow> window) {
        windows.push_back(std::move(window));
    }

    void DrawAll() {
        for (auto& window : windows) {
            window->Draw();
        }
    }

private:
    std::vector<std::unique_ptr<EditorWindow>> windows;
};