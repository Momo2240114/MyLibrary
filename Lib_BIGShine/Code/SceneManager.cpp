#include "SceneManager.h"
#include "IScene.h"
#include "GameObjectManager.h"

std::unique_ptr<Scene> SceneManager::NowScene = nullptr;
std::unique_ptr<Scene> SceneManager::NextScene = nullptr;
bool SceneManager::callInitOnChange;

void SceneManager::Initialize(std::unique_ptr<Scene> firstScene)
{
    // 所有権を NowScene に渡す
    NowScene = std::move(firstScene);
    GameObjectManager::Instance().ChangeScene(NowScene.get());
    NowScene->Initialize();

}

void SceneManager::Update(float elapsedTime)
{
    // 切り替え処理
    if (NextScene)
    {
        // 1. 旧シーンの終了処理
        Clear();

        // 2. 所有権の移動
        NowScene = std::move(NextScene);
        NextScene = nullptr;

        // 3. マネージャー群に新しいシーンを通知
        if (NowScene)
        {
            GameObjectManager::Instance().ChangeScene(NowScene.get());

            // 必要なら初期化
            if (callInitOnChange)
            {
                NowScene->Initialize();
            }
        }
    }

    // シーン更新
    if (NowScene)
    {
        NowScene->Update(elapsedTime);
    }
}

void SceneManager::Render(float elapsedTime)
{
    if (NowScene)
    {
        NowScene->Render(elapsedTime);
    }
}

void SceneManager::DrawGUI()
{
    if (NowScene)
    {
        NowScene->DrawGUI();
    }
}

void SceneManager::ChangeScene(std::unique_ptr<Scene> scene, bool callInitialize)
{
    // 新しいシーンを予約（所有権を NextScene が持つ）
    NextScene = std::move(scene);
    callInitOnChange = callInitialize;
}

void SceneManager::Clear()
{
    if (NowScene)
    {
        NowScene->Finalize();
    }
}

Scene* SceneManager::GetCurrentScene()
{
    return NowScene.get();
}
