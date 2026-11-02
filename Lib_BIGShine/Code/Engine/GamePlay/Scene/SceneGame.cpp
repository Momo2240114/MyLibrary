#include "PrecompiledHeader.h"

#include"SceneGame.h"
#include "Engine/UI/GameMenuComponent.h"

#include "Game/GameMode/GameModeManager.h"
#include "Engine/Core/GameRuntimeSettings.h"
#include "Engine/GamePlay/Input/MouseInput.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Input/KeyInput.h"
#include "Engine/GamePlay/Input/ControllerInput.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include <imgui.h>

void SceneGame::Finalize()
{
    SceneBase::Finalize();
}

void SceneGame::Initialize()
{
    BS_PROFILE_SCOPE("SceneGame::Initialize");
	SceneBase::Initialize();
    if (startImmediately) MouseInput::Instance().SetDebugFlag(false);
    if (startImmediately && isEditorSuppressed) MouseInput::Instance().SetCursorLockEnabled(true);
	if (!GameRuntimeSettings::IsGameOnly() && !startImmediately) _GameObjectManager->EnableEditorPlayMode();
	if (SceneManager::IsRestoringEditorScene())
	{
		_GameObjectManager->RestoreEditorSnapshot(SceneManager::GetEditorPlaySnapshot());
	}
	else
	{
#if defined(BS_BUILD_RELEASE) && !defined(BS_APP_GAME) && !defined(BS_APP_ENGINE)
        // 通常のReleaseはEditorを有効にした配布版でも、書き出し時の開始Sceneを読み込む。
        const auto name_ = GameRuntimeSettings::GetStartupScene();
#else
        const auto name_ = GameRuntimeSettings::IsGameOnly() ? GameRuntimeSettings::GetStartupScene() : std::string("MainScene");
#endif
        if (isLoadingPreparation)
            _GameObjectManager->LoadSceneAsync(name_, startImmediately || GameRuntimeSettings::IsGameOnly() || SceneManager::IsEditorPlaySessionActive());
        else _GameObjectManager->LoadScene(name_);
	}
	if (!isLoadingPreparation && (startImmediately || GameRuntimeSettings::IsGameOnly() || SceneManager::IsEditorPlaySessionActive()))
	{
		// ゲーム専用起動とテスト中のScene遷移では、Play待ちを挟まずゲーム進行を開始する。
		if (auto gameModeManager_ = _GameObjectManager->GetGameModeManager()) gameModeManager_->BeginGameSceneFlow();
	}

	// SceneBase側で作られたGameObjectManagerの階層関係を確定させる。
	_GameObjectManager->ResolveHierarchy();
}

// ゲーム内UIとポーズも、配置済みComponentをManagerの通常Updateから更新する。
void SceneGame::Update(float elapsedTime)
{
    SceneBase::Update(elapsedTime);
}

void SceneGame::Render(float elapsedTime)
{
	SceneBase::Render(elapsedTime);
}

void SceneGame::DrawGUI()
{
	SceneBase::DrawGUI();
}

// 物理の再開待ちフレームとは分け、実際に開いているUIだけでカーソルを判断する。
bool SceneGame::RequiresFreeCursor() const
{
    if (_GameObjectManager)
        for (const auto& menu_ : _GameObjectManager->FindComponents<GameMenuComponent>())
            if (menu_ && menu_->IsEnabled() && menu_->IsOpen()) return true;
    return SceneBase::RequiresFreeCursor();
}
