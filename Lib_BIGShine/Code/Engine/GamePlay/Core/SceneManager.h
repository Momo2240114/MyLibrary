#pragma once

#include <memory>
#include <functional>

class Scene;

class SceneManager
{
public:
    // 初期化
    static void Initialize(std::unique_ptr<Scene> firstScene);

    // 更新・描画
    static void Update(float elapsedTime);
    static void Render(float elapsedTime);
    static void DrawGUI();

    // シーン切り替え予約
    static void ChangeScene(std::unique_ptr<Scene> scene, bool callInitialize = true);

    // 現在のシーン取得
    static Scene* GetCurrentScene();

    // 現在のシーンクリア
    static void Clear();

private:
    static std::unique_ptr<Scene> NowScene;
    static std::unique_ptr<Scene> NextScene;

    static bool  callInitOnChange;

};