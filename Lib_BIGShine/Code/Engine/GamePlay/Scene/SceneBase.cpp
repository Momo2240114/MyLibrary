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
#include "Editor/Window/ShadowWindow.h"
#include "Editor/NodeEditor/AnimNodeEditorWindow.h"

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

	editorWindowManager = std::make_unique<EditorWindowManager>(_GameObjectManager.get());

	editorWindowManager->AddWindow(std::make_unique<HierarchyWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<InspectorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<ShaderWindow>("",_GameObjectManager.get(),m_ModelRenderSystem));
	editorWindowManager->AddWindow(std::make_unique<ShadowWindow>("",_GameObjectManager.get(),m_ModelRenderSystem));
	editorWindowManager->AddWindow(std::make_unique<AnimationEditorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<AnimNodeEditorWindow>(_GameObjectManager.get()));

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
	_GameObjectManager->Update(elapsedTime);
}

void SceneBase::Render(float elapsedTime)
{
	_GameObjectManager->RenderAll(m_SpriteRenderSystem, m_ModelRenderSystem);

}

void SceneBase::DrawGUI()
{
	if(isEditorVisible)
	{
		editorWindowManager->DrawAll();
		GraphicsDeviceManager::Instance().ShowPostEffectGui(); // ここで呼び出す
	}
}



