#include "PrecompiledHeader.h"

#include "SceneManager.h"
#include "IScene.h"
#include "SceneBase.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"

std::unique_ptr<Scene> SceneManager::NowScene = nullptr;
std::unique_ptr<Scene> SceneManager::NextScene = nullptr;
bool SceneManager::callInitOnChange;

void SceneManager::Initialize(std::unique_ptr<Scene> firstScene)
{
    // 所有権を NowScene に渡す
    NowScene = std::move(firstScene);

    // もし GameObjectManager::ChangeScene が「現在のシーン名」を記録するだけのものなら以下のように。
    if (NowScene)
    {
        // 先に初期化を行い、固有の GameObjectManager を産み落とす
        NowScene->Initialize();

        // 誕生したマネージャーに対し、現在のシーンを認識させる
        SceneBase* sceneBase_ = dynamic_cast<SceneBase*>(NowScene.get());
        if (sceneBase_ && sceneBase_->GetGameObjectManager())
        {
            sceneBase_->GetGameObjectManager()->ChangeScene(sceneBase_);
        }
    }

}

void SceneManager::Update(float elapsedTime)
{
    // シーン切り替え予約（NextScene）がある場合
    if (NextScene)
    {
        std::unique_ptr<Scene> nextScene_ = std::move(NextScene);

        // 1. Finalize と GameObjectManager の破棄
        Clear();

        // 2. 主権移譲
        NowScene = std::move(nextScene_);

        if (NowScene)
        {
            // 必要なら初期化（通常はこちらが true になるはずです）
            if (callInitOnChange)
            {
                NowScene->Initialize();
            }

            // マネージャーに直接シーンを通知
            SceneBase* sceneBase_ = dynamic_cast<SceneBase*>(NowScene.get());
            if (sceneBase_ && sceneBase_->GetGameObjectManager())
            {
                sceneBase_->GetGameObjectManager()->ChangeScene(sceneBase_);
            }
        }
    }

    // 現在のsceneを進める
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

    // PhysXなどの基盤が終了する前に、Sceneが所有するComponentを破棄する。
    NowScene.reset();
    NextScene.reset();
}

Scene* SceneManager::GetCurrentScene()
{
    return NowScene.get();
}
