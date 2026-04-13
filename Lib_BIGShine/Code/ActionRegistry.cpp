#include "ActionRegistry.h"
#include "SceneManager.h"
#include "SceneGame.h"
#include "SceneTitle.h"
#include "SceneLoading.h"
#include "SceneTutorial.h"
#include "Engine/Audio/AudioManager.h"


std::shared_ptr<ActionRegistry> ActionRegistry::instance = nullptr;

void RegisterAllActions()
{
    auto& reg = ActionRegistry::Instance();
    auto& audio = AudioManager::Instance();
    //分類の登録
    reg.RegisterAction("Transform", "---", nullptr);
    reg.RegisterAction("FOV", "---", nullptr);
    reg.RegisterAction("PostProcess", "---", nullptr);
    reg.RegisterAction("Camera", "---", nullptr);
    reg.RegisterAction("Sequence", "---", nullptr);
    reg.RegisterAction("Audio", "---", nullptr);
    reg.RegisterAction("Scene", "---", nullptr);

    //// タイトルBGM
    reg.RegisterAction("Audio", "PlayBGM_Title", [&]() {
        audio.StopBGM();
        audio.PlayBGM(BGM_TITLE);
        });

    // チュートリアルBGM
    reg.RegisterAction("Audio", "PlayBGM_Tutorial", [&]() {
        audio.StopBGM();
        audio.PlayBGM(BGM_TUTORIAL);
        });

    // BGM全停止
    reg.RegisterAction("Audio", "StopAllBGM", [&]() {
        audio.StopBGM();
        });

    // --- SE再生アクション ---
    // タイトルでのタップ音
    reg.RegisterAction("Audio", "PlaySE_TapStart", [&]() {
        audio.PlaySE(SE_TITLESELECT);
        });
    // タイトルでのタップ音
    reg.RegisterAction("Audio", "PlaySE_TapStartMOVE", [&]() {
        audio.PlaySE(SE_TTILEMOVE);
        });
    // タイトルでのタップ音
    reg.RegisterAction("Audio", "PlaySE_GameOver", [&]() {
        audio.PlaySE(SE_GAMEOVER);
        });
    reg.RegisterAction("Audio", "PlaySE_GameClear", [&]() {
        audio.PlaySE(SE_GAMECLEAR);
        });



    reg.RegisterAction("Scene", "GoSceneGame", []() 
        {
        // SceneGame を作って、そのまま SceneLoading に所有権を渡す
        auto next = std::make_unique<SceneGame>();
        SceneManager::ChangeScene(std::make_unique<SceneLoading>(std::move(next)));
        });

    reg.RegisterAction("Scene", "GoSceneTitle", []() 
        {
        auto next = std::make_unique<SceneTitle>();
        SceneManager::ChangeScene(std::make_unique<SceneLoading>(std::move(next)));
        });
    reg.RegisterAction("Scene", "GoSceneTutorial", []() 
        {
        auto next = std::make_unique<SceneTutorial>();
        SceneManager::ChangeScene(std::make_unique<SceneLoading>(std::move(next)));
        });
}

void ActionRegistry::RegisterAction(const std::string& category, const std::string& name, ActionFunc func)
{
    // カテゴリが存在しなければ作る
    auto& categoryMap = actions[category];

    // 既に name が存在するなら追加しない
    if (categoryMap.find(name) != categoryMap.end())
        return;

    categoryMap.emplace(name, func);
}

void ActionRegistry::Call(const std::string& name)
{

    for (auto& [cat, actionMap] : actions)
    {
        auto it = actionMap.find(name); // count と [] で2回検索するより find が速い
        if (it != actionMap.end()) {
            if (it->second) it->second(); // 関数の中身が空（nullptr）でないかチェック
            return;
        }
    }
}


bool ActionRegistry::HasAction(const std::string& name)
{
    for (auto const& [cat, actionMap] : actions) {
        if (actionMap.count(name)) return true;
    }
    return false;
}