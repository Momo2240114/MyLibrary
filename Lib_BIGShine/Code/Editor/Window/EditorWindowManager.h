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
    GameObjectManager* gameObjectManager; // ここが繋がる！
    std::vector<std::unique_ptr<EditorWindow>> windows;
};