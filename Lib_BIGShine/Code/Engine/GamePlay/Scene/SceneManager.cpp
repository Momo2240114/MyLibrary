#include "SceneManager.h"
#include "SceneBase.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"

std::unique_ptr<SceneBase> SceneManager::NowScene = nullptr;
std::unique_ptr<SceneBase> SceneManager::NextScene = nullptr;
bool SceneManager::callInitOnChange;

void SceneManager::Initialize(std::unique_ptr<SceneBase> firstScene)
{
    // 所有権を NowScene に渡す
    NowScene = std::move(firstScene);

    // もし GameObjectManager::ChangeScene が「現在のシーン名」を記録するだけのものなら以下のように。
    if (NowScene)
    {
        // 先に初期化を行い、固有の GameObjectManager を産み落とす
        NowScene->Initialize();

        // 誕生したマネージャーに対し、現在のシーンを認識させる
        if (NowScene->GetGameObjectManager())
        {
            NowScene->GetGameObjectManager()->ChangeScene(NowScene.get());
        }
    }

}

void SceneManager::Update(float elapsedTime)
{
    // シーン切り替え予約（NextScene）がある場合の跳躍儀式
    if (NextScene)
    {
        std::unique_ptr<SceneBase> nextScene_ = std::move(NextScene);

        // 1. 旧世界の終焉（Finalize と GameObjectManager の破棄）
        Clear();

        // 2. 新世界への主権移譲
        NowScene = std::move(nextScene_);

        if (NowScene)
        {
            // 必要なら初期化（通常はこちらが true になるはずです）
            if (callInitOnChange)
            {
                NowScene->Initialize();
            }

            // キャストなしで、新世界のマネージャーに直接シーンを通知できる美しさ！
            if (NowScene->GetGameObjectManager())
            {
                NowScene->GetGameObjectManager()->ChangeScene(NowScene.get());
            }
        }
    }

    // 現在の世界の時間を進める
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

void SceneManager::ChangeScene(std::unique_ptr<SceneBase> scene, bool callInitialize)
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

    // PhysXなどの基盤が終了する前に、Sceneが所有するComponentを破棄する。
    NowScene.reset();
    NextScene.reset();
}

SceneBase* SceneManager::GetCurrentScene()
{
    return NowScene.get();
}
