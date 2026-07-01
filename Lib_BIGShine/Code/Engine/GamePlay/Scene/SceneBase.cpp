#include"SceneBase.h"

#include"Engine/Graphics/Core/GraphicsDeviceManager.h"
#include"Engine/GamePlay/Camera/CameraComponent.h"

#include"Engine/Physics/BoxCollider.h"
#include"Engine/Physics/CapsuleCollider.h"
#include"Engine/GamePlay/Serialize/ActionRegistry.h"
#include"Engine/GamePlay/Input/KeyInput.h"
#include "Engine/Audio/AudioManager.h"
#include "Editor/Window/HierarchyWindow.h"
#include "Editor/Window/InspectorWindow.h"
#include "Editor/Window/ShaderWindow.h"
#include "Editor/Window/AnimationEditorWindow.h"
#include "Editor/Window/AssetBrowserWindow.h"
#include "Editor/Window/CurveAssetEditorWindow.h"
#include "Editor/Window/EasingPresetEditorWindow.h"
#include "Editor/Window/ActionSequenceEditorWindow.h"
#include "Editor/Window/EffectAssetListWindow.h"
#include "Editor/Window/ShadowWindow.h"
#include "Editor/Window/PostEffectWindow.h"
#include "Editor/Window/InputEditorWindow.h"
#include "Editor/Window/TimeManagerWindow.h"
#include "Editor/NodeEditor/AnimNodeEditorWindow.h"
#include "Editor/Workspace/ParticleEditorWindow.h"
#include "Editor/Workspace/PlayerStateEditorWindow.h"
#include "Editor/Workspace/AudioWorkspaceWindow.h"
#include "Editor/Workspace/AIBehaviorEditorWindow.h"

std::shared_ptr<GameObjectManager> SceneBase::GetGameObjectManager()
{ 
	return _GameObjectManager;
}

void SceneBase::Finalize()
{
	m_SpriteRenderSystem.Finalize();
	actionRegistry.reset();


	if (_GameObjectManager) {
		_GameObjectManager->Clear();
		_GameObjectManager.reset(); // 世界の崩壊（メモリ解放）
	}
}

void SceneBase::Initialize()
{
	// シーン独自のマネージャーを生成する
	_GameObjectManager = std::make_shared<GameObjectManager>();
	_GameObjectManager->Initialize(); // マネージャーの初期化
	_GameObjectManager->SetModelRenderSystem(&m_ModelRenderSystem);

	editorWindowManager = std::make_unique<EditorWindowManager>(_GameObjectManager.get());

	editorWindowManager->AddWindow(std::make_unique<HierarchyWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<InspectorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<ShaderWindow>("",_GameObjectManager.get(),m_ModelRenderSystem));
	editorWindowManager->AddWindow(std::make_unique<ShadowWindow>("",_GameObjectManager.get(),m_ModelRenderSystem));
	editorWindowManager->AddWindow(std::make_unique<PostEffectWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<InputEditorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<TimeManagerWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<ActionSequenceEditorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<EffectAssetListWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<AnimationEditorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<AnimNodeEditorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<ParticleEditorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<PlayerStateEditorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<AudioWorkspaceWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<AIBehaviorEditorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<AssetBrowserWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<CurveAssetEditorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<EasingPresetEditorWindow>(_GameObjectManager.get()));

	actionRegistry = std::make_shared<ActionRegistry>();
	ActionRegistry::SetInstance(actionRegistry);

	//モデルのレンダラーを作成
	m_ModelRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice());
	m_SpriteRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice(), GraphicsDeviceManager::Instance().GetContext());
	m_ShapeRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice());
}

void SceneBase::Update(float elapsedTime)
{
	if (KeyInput::KeyPressedIn(KeyInput::Key::K_F1))
	{
		isEditorVisible = !isEditorVisible;
	}

	if (editorWindowManager)
	{
		editorWindowManager->Update(elapsedTime);
	}

	if (isEditorVisible && editorWindowManager && editorWindowManager->ShouldPauseScene())
	{
		return;
	}

	_GameObjectManager->Update(elapsedTime);
}

void SceneBase::Render(float elapsedTime)
{
	if (isEditorVisible &&
		editorWindowManager &&
		editorWindowManager->ShouldPauseScene() &&
		!editorWindowManager->ShouldRenderSceneWhenPaused())
	{
		GraphicsDeviceManager::Instance().SetBackBufferRenderTarget();
		return;
	}

	_GameObjectManager->RenderAll(m_SpriteRenderSystem, m_ModelRenderSystem);

}

void SceneBase::DrawGUI()
{
	if(isEditorVisible)
	{
		editorWindowManager->DrawAll();
	}
}
