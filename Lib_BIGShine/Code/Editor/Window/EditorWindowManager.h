// EditorWindowManager.h
#include <vector>
#include <memory>
#include <string>
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

    void Update(float deltaTime_);
    void DrawAll();
    bool IsWorkspaceMode() const { return activeWorkspaceIndex >= 0 && activeWorkspaceIndex < static_cast<int>(windows.size()) && windows[activeWorkspaceIndex]->IsVisible(); }
    bool ShouldPauseScene() const { return IsWorkspaceMode() && windows[activeWorkspaceIndex]->ShouldPauseScene(); }
    bool ShouldRenderSceneWhenPaused() const { return IsWorkspaceMode() && windows[activeWorkspaceIndex]->ShouldRenderSceneWhenPaused(); }
private:
    void DrawMenuBar();
    const char* GetWindowMenuCategory(const EditorWindow& window_) const;
    void DrawWorkspaceSelectorBar();
    void SetActiveWorkspace(int workspaceIndex_);
    std::string GetActiveWorkspaceName() const;
    float GetWorkspaceSelectorHeight() const;
    void ApplyEngineLayout(ImVec2& scenePos, ImVec2& sceneSize);
    void DrawSceneView(const ImVec2& pos, const ImVec2& size);
    void DrawLayoutSplitters();

    ImVec2 bottomDockPos = ImVec2(0.0f, 0.0f);
    ImVec2 bottomDockSize = ImVec2(0.0f, 0.0f);
    float leftPanelWidth = 300.0f;
    float rightPanelWidth = 350.0f;
    float bottomPanelHeight = 300.0f;
    int selectedBottomTool = 0;
    int activeWorkspaceIndex = -1;
    GameObjectManager* gameObjectManager; // ここが繋がる！
    std::vector<std::unique_ptr<EditorWindow>> windows;
};

