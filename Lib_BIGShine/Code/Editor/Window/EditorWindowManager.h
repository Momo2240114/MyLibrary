// EditorWindowManager.h
#include <vector>
#include <memory>
#include "EditorWindow.h" 

class GameObjectManager;

class EditorWindowManager
{
public:
    // コンストラクタでObjectManagerを入れる
    EditorWindowManager(GameObjectManager* manager) : gameObjectManager(manager) {}

    void AddWindow(std::unique_ptr<EditorWindow> window) {
        windows.push_back(std::move(window));
    }

    void DrawAll();
private:
    void DrawMenuBar();
    void ApplyEngineLayout(ImVec2& scenePos, ImVec2& sceneSize);
    void DrawSceneView(const ImVec2& pos, const ImVec2& size);
    void DrawLayoutSplitters();

    ImVec2 bottomDockPos = ImVec2(0.0f, 0.0f);
    ImVec2 bottomDockSize = ImVec2(0.0f, 0.0f);
    float leftPanelWidth = 300.0f;
    float rightPanelWidth = 350.0f;
    float bottomPanelHeight = 300.0f;
    int selectedBottomTool = 0;
    GameObjectManager* gameObjectManager; // ここが繋がる！
    std::vector<std::unique_ptr<EditorWindow>> windows;
};