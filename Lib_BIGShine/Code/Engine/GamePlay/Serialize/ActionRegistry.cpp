#include "ActionRegistry.h"
#include "Engine/GamePlay/Scene/SceneManager.h"
#include "Engine/GamePlay/Scene/SceneGame.h"
#include "Engine/GamePlay/Scene/SceneTitle.h"
#include "Engine/GamePlay/Scene/SceneLoading.h"
#include "Engine/GamePlay/Scene/SceneTutorial.h"
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
    audio.RegisterAudioWorkspaceActions();

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

std::vector<std::string> ActionRegistry::GetCategories()
{
    std::vector<std::string> categories;
    for (auto const& [cat, _] : actions) categories.push_back(cat);
    return categories;
}

ActionRegistry::ActionFunc ActionRegistry::GetAction(const std::string& name)
{
    for (auto const& [cat, actionMap] : actions) {
        auto it = actionMap.find(name);
        if (it != actionMap.end()) {
            return it->second;
        }
    }
    return nullptr; // 見つからない場合
}

bool ActionRegistry::RemoveAction(const std::string& category, const std::string& actionName)
{
    auto catIt = actions.find(category);
    if (catIt != actions.end()) {
        // アクション名を探して削除
        if (catIt->second.erase(actionName) > 0) {
            // カテゴリが空になった場合、カテゴリ自体を削除するかは設計次第
            // if (catIt->second.empty()) actions.erase(catIt); 
            return true;
        }
    }
    return false;
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

bool ActionRegistry::HasCategory(const std::string& category) const
{
    return actions.find(category) != actions.end();
}

bool ActionRegistry::HasActionInCategory(const std::string& category, const std::string& actionName) const
{
    auto catIt = actions.find(category);
    if (catIt != actions.end()) {
        return catIt->second.find(actionName) != catIt->second.end();
    }
    return false;
}

