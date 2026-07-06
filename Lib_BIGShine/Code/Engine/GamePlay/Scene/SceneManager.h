#pragma once

#include <memory>
#include <functional>

class SceneBase;

class SceneManager
{
public:
    // 初期化
    static void Initialize(std::unique_ptr<SceneBase> firstScene);

    // 更新・描画
    static void Update(float elapsedTime);
    static void Render(float elapsedTime);
    static void DrawGUI();

    // シーン切り替え予約
    static void ChangeScene(std::unique_ptr<SceneBase> scene, bool callInitialize = true);

    // 現在のシーン取得
    static SceneBase* GetCurrentScene();

    // 現在のシーンクリア
    static void Clear();

private:
    static std::unique_ptr<SceneBase> NowScene;
    static std::unique_ptr<SceneBase> NextScene;

    static bool  callInitOnChange;

};