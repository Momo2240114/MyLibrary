#include "PrecompiledHeader.h"

#include "ActionRegistry.h"
#include "Engine/GamePlay/Scene/SceneManager.h"
#include "Engine/GamePlay/Scene/SceneGame.h"
#include "Engine/GamePlay/Scene/SceneTitle.h"
#include "Engine/GamePlay/Scene/SceneLoading.h"
#include "Engine/GamePlay/Scene/SceneTutorial.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/Common/DebugLog.h"
#include "Engine/Core/GameRuntimeSettings.h"

#include "Engine/GamePlay/Input/ControllerInput.h"
#include "Engine/GamePlay/Scene/SceneBase.h"
#include "Game/AI/Dragon/DragonAttackControllerComponent.h"
#include "Engine/Debug/TracyProfiler.h"

#include <string_view>

std::shared_ptr<ActionRegistry> ActionRegistry::instance = nullptr;

void RegisterAllActions()
{
    auto& reg = ActionRegistry::Instance();
    auto& audio = AudioManager::Instance();
    // Scene共通Actionの再構築時に、前Sceneの一時的な演出状態を持ち越さない。
    audio.ResetPresentationAudio();
    ControllerInput::Instance().StopRumble();
    // Sequenceや被弾時の追加Actionからも、共通の手応えを呼び出せるようにする。
    reg.RegisterAction("Controller", "Controller.RumbleAttackStart", []() { ControllerInput::Instance().PlayRumble(0.15f, 0.08f, 0.08f, 1); });
    reg.RegisterAction("Controller", "Controller.RumbleAttack", []() { ControllerInput::Instance().PlayRumble(0.3f, 0.2f, 0.12f, 2); });
    reg.RegisterAction("Controller", "Controller.RumbleReceiveHit", []() { ControllerInput::Instance().PlayRumble(0.65f, 0.4f, 0.18f, 3); });
    // 成立側と攻撃を受け流された側を分け、ActionSequenceからも同じ反応を再利用する。
    reg.RegisterAction("Controller", "Controller.RumbleGuard", []() { ControllerInput::Instance().PlayRumble(0.3f, 0.45f, 0.10f, 2); });
    reg.RegisterAction("Controller", "Controller.RumbleGuardBreak", []() { ControllerInput::Instance().PlayRumble(0.85f, 0.5f, 0.24f, 4); });
    reg.RegisterAction("Controller", "Controller.RumbleAttackGuarded", []() { ControllerInput::Instance().PlayRumble(0.3f, 0.5f, 0.12f, 2); });
    reg.RegisterAction("Controller", "Controller.RumbleJustGuard", []() { ControllerInput::Instance().PlayRumble(0.5f, 0.8f, 0.16f, 4); });
    reg.RegisterAction("Controller", "Controller.RumbleJustGuarded", []() { ControllerInput::Instance().PlayRumble(0.7f, 0.45f, 0.20f, 3); });
    reg.RegisterAction("Controller", "Controller.RumbleJustDodge", []() { ControllerInput::Instance().PlayRumble(0.18f, 0.5f, 0.14f, 3); });
    reg.RegisterAction("Controller", "Controller.StopRumble", []() { ControllerInput::Instance().StopRumble(); });

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
    // 登場演出の既存通知を使い、現在のドラゴン位置から咆哮を一度だけ鳴らす。
    reg.RegisterAction("Audio", "BossIntro.RoarSound", []()
    {
        BS_PROFILE_SCOPE("Audio::BossIntroRoarSound");
        const auto scene_ = dynamic_cast<SceneBase*>(SceneManager::GetCurrentScene());
        const auto manager_ = scene_ ? scene_->GetGameObjectManager() : nullptr;
        if (!manager_) return;
        for (const auto& dragon_ : manager_->FindComponents<DragonAttackControllerComponent>())
        {
            const auto owner_ = dragon_ ? dragon_->GetGameObject() : nullptr;
            // 通常攻撃が演出用に一時停止されていても、保存上有効なドラゴンの咆哮は再生する。
            if (!dragon_ || !dragon_->IsConfiguredEnabled() || !owner_ || owner_->IsPendingDestroy() || !owner_->transform) continue;
            AudioManager::Instance().PlaySpatialAttackSound("Data/Audio/SE/Sonniss/Dragon/RoarTrex.wav",
                owner_->transform->GetWorldPosition(), 0.65f, 12.0f, 160.0f);
            break;
        }
    });

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

    // Sequenceからも通常戦闘・暴走・大技の各曲を明示的に選択できるようにする。
    reg.RegisterAction("Audio", "PlayBGM_Game", []() { AudioManager::Instance().SwitchBGM(BGM_GAME); });
    reg.RegisterAction("Audio", "PlayBGM_HumanBoss", []() { AudioManager::Instance().SwitchBGM(BGM_HUMAN_BOSS); });
    reg.RegisterAction("Audio", "PlayBGM_Berserk", []() { AudioManager::Instance().SwitchBGM(BGM_BERSERK); });
    reg.RegisterAction("Audio", "PlayBGM_SpecialAttack", []() { AudioManager::Instance().SwitchBGM(BGM_SPECIAL_ATTACK); });

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

    // Scene固有の終了処理を持たず、JSONのメニューから共通Actionを呼び出す。
    const auto canExit_ = []()
    {
        const auto scene_ = dynamic_cast<SceneBase*>(SceneManager::GetCurrentScene());
        const auto manager_ = scene_ ? scene_->GetGameObjectManager() : nullptr;
        return !manager_ || !manager_->IsCinematicSequencePreview();
    };
    reg.RegisterAction("Application", "Application.Exit", [canExit_]() { if (canExit_()) PostQuitMessage(0); });
    // 移行前のJSONやSequenceから呼ばれる終了Actionも維持する。
    reg.RegisterAction("Title", "Title.Exit", [canExit_]() { if (canExit_()) PostQuitMessage(0); });

    reg.RegisterAction("Scene", "GoSceneGame", []()
        {
            // Scene Actionからの本編開始は、EditorのPlay待ちを挟まない。
            auto next_ = std::make_unique<SceneGame>(true);
            SceneManager::ChangeScene(std::make_unique<SceneLoading>(std::move(next_)));
        });

    reg.RegisterAction("Scene", "GoSceneTitle", []()
        {
            auto next_ = std::make_unique<SceneTitle>();
            SceneManager::ChangeScene(std::make_unique<SceneLoading>(std::move(next_)));
        });

    // タイトルを経由する起動でも、従来のScene編集へ戻れる入口を残す。
    reg.RegisterAction("Scene", "GoSceneEditor", []()
        {
            if (GameRuntimeSettings::IsGameOnly()) return;
            SceneManager::ChangeScene(std::make_unique<SceneLoading>(std::make_unique<SceneGame>()));
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
    // 呼び出し中のObject破棄で登録が解除されても、実行中のCallbackを保持する。
    const ActionFunc callback_ = iterator_->second;
    callback_();
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
