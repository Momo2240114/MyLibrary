#include "PrecompiledHeader.h"

#include "ActionRegistry.h"
#include "Engine/GamePlay/Scene/SceneManager.h"
#include "Engine/GamePlay/Scene/SceneGame.h"
#include "Engine/GamePlay/Scene/SceneTitle.h"
#include "Engine/GamePlay/Scene/SceneLoading.h"
#include "Engine/GamePlay/Scene/SceneTutorial.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/Common/DebugLog.h"

#include <string_view>

std::shared_ptr<ActionRegistry> ActionRegistry::instance = nullptr;

void RegisterAllActions()
{
    auto& reg = ActionRegistry::Instance();
    auto& audio = AudioManager::Instance();

    reg.RegisterAction("Transform", "---", nullptr);
    reg.RegisterAction("FOV", "---", nullptr);
    reg.RegisterAction("PostProcess", "---", nullptr);
    reg.RegisterAction("Camera", "---", nullptr);
    reg.RegisterAction("Sequence", "---", nullptr);
    reg.RegisterAction("Audio", "---", nullptr);
    reg.RegisterAction("Scene", "---", nullptr);
    audio.RegisterAudioWorkspaceActions();
    // 音声素材の準備後、Sequence Editorで実際のAudio Actionへ差し替えるための空Action。
    reg.RegisterAction("Audio", "BossIntro.DrawWeaponSound", []() {});
    reg.RegisterAction("Audio", "BossIntro.RoarSound", []() {});

    reg.RegisterAction("Audio", "PlayBGM_Title", [&]() {
        audio.StopBGM();
        audio.PlayBGM(BGM_TITLE);
        });

    reg.RegisterAction("Audio", "PlayBGM_Tutorial", [&]() {
        audio.StopBGM();
        audio.PlayBGM(BGM_TUTORIAL);
        });

    reg.RegisterAction("Audio", "StopAllBGM", [&]() {
        audio.StopBGM();
        });

    reg.RegisterAction("Audio", "PlaySE_TapStart", [&]() {
        audio.PlaySE(SE_TITLESELECT);
        });

    reg.RegisterAction("Audio", "PlaySE_TapStartMOVE", [&]() {
        audio.PlaySE(SE_TTILEMOVE);
        });

    reg.RegisterAction("Audio", "PlaySE_GameOver", [&]() {
        audio.PlaySE(SE_GAMEOVER);
        });

    reg.RegisterAction("Audio", "PlaySE_GameClear", [&]() {
        audio.PlaySE(SE_GAMECLEAR);
        });

    reg.RegisterAction("Scene", "GoSceneGame", []()
        {
            auto next_ = std::make_unique<SceneGame>();
            SceneManager::ChangeScene(std::make_unique<SceneLoading>(std::move(next_)));
        });

    reg.RegisterAction("Scene", "GoSceneTitle", []()
        {
            auto next_ = std::make_unique<SceneTitle>();
            SceneManager::ChangeScene(std::make_unique<SceneLoading>(std::move(next_)));
        });

    reg.RegisterAction("Scene", "GoSceneTutorial", []()
        {
            auto next_ = std::make_unique<SceneTutorial>();
            SceneManager::ChangeScene(std::make_unique<SceneLoading>(std::move(next_)));
        });
}

StringId ActionRegistry::MakeActionId(std::string_view name_)
{
    return StringIdUtility::Hash(name_);
}

void ActionRegistry::RegisterAction(
    const std::string& category,
    const std::string& name,
    ActionFunc func)
{
    auto& categoryMap_ = actions[category];
    if (categoryMap_.find(name) != categoryMap_.end()) return;

    categoryMap_.emplace(name, func);

    // Editor表示用の文字列Mapを残しつつ、実行時呼び出し用のID索引も更新する。
    const StringId categoryId_ = MakeActionId(category);
    const StringId actionId_ = MakeActionId(name);
    categoryIndex[categoryId_].emplace(actionId_, func);
    actionIndex.emplace(actionId_, func);
}

std::vector<std::string> ActionRegistry::GetCategories()
{
    std::vector<std::string> categories_;
    for (auto const& [category_, actionMap_] : actions)
    {
        (void)actionMap_;
        categories_.push_back(category_);
    }
    return categories_;
}

ActionRegistry::ActionFunc ActionRegistry::GetAction(const std::string& name)
{
    const auto iterator_ = actionIndex.find(MakeActionId(name));
    return iterator_ != actionIndex.end() ? iterator_->second : nullptr;
}

bool ActionRegistry::RemoveAction(
    const std::string& category,
    const std::string& actionName)
{
    auto categoryIterator_ = actions.find(category);
    if (categoryIterator_ == actions.end()) return false;
    if (categoryIterator_->second.erase(actionName) == 0) return false;

    const StringId categoryId_ = MakeActionId(category);
    const StringId actionId_ = MakeActionId(actionName);
    auto categoryIndexIterator_ = categoryIndex.find(categoryId_);
    if (categoryIndexIterator_ != categoryIndex.end())
    {
        categoryIndexIterator_->second.erase(actionId_);
    }

    RebuildActionIndexForName(actionName);
    return true;
}

void ActionRegistry::Call(const std::string& name)
{
    const auto iterator_ = actionIndex.find(MakeActionId(name));
    if (iterator_ == actionIndex.end()) return;
    if (!iterator_->second) return;

    BS_LOG_INFO(LOG_CATEGORY::GAME, "Action fired: " + name);
    iterator_->second();
}

bool ActionRegistry::HasAction(const std::string& name)
{
    return actionIndex.find(MakeActionId(name)) != actionIndex.end();
}

bool ActionRegistry::HasCategory(const std::string& category) const
{
    return categoryIndex.find(StringIdUtility::Hash(category)) != categoryIndex.end();
}

bool ActionRegistry::HasActionInCategory(
    const std::string& category,
    const std::string& actionName) const
{
    const auto categoryIterator_ = categoryIndex.find(StringIdUtility::Hash(category));
    if (categoryIterator_ == categoryIndex.end()) return false;

    return categoryIterator_->second.find(StringIdUtility::Hash(actionName)) !=
        categoryIterator_->second.end();
}

void ActionRegistry::RebuildActionIndexForName(const std::string& actionName_)
{
    // 同名Actionが別カテゴリに残っている場合、全体呼び出し用Indexだけ登録し直す。
    const StringId actionId_ = MakeActionId(actionName_);
    actionIndex.erase(actionId_);

    for (const auto& [category_, actionMap_] : actions)
    {
        (void)category_;
        const auto actionIterator_ = actionMap_.find(actionName_);
        if (actionIterator_ != actionMap_.end())
        {
            actionIndex.emplace(actionId_, actionIterator_->second);
            return;
        }
    }
}
