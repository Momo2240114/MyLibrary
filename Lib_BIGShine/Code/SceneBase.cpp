#include"SceneBase.h"

#include"Engine/Graphics/Core/GraphicsDeviceManager.h"
#include"Engine/GamePlay/Camera/CameraComponent.h"
#include"InputComponent.h"
#include"Engine/Physics/BoxCollider.h"
#include"Engine/Physics/CapsuleCollider.h"
#include"Engine/GamePlay/Serialize/ActionRegistry.h"
#include"Engine/GamePlay/Input/MouseInput.h"
#include "Engine/Audio/AudioManager.h"

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

	actionRegistry = std::make_shared<ActionRegistry>();
	ActionRegistry::SetInstance(actionRegistry);

	//モデルのレンダラーを作成
	m_ModelRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice());
	m_SpriteRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice(), GraphicsDeviceManager::Instance().GetContext());
	m_ShapeRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice());
}

void SceneBase::Update(float elapsedTime)
{
	_GameObjectManager->Update(elapsedTime);
}

void SceneBase::Render(float elapsedTime)
{
	_GameObjectManager->RenderAll(m_SpriteRenderSystem, m_ModelRenderSystem);
}

void SceneBase::DrawGUI()
{
	_GameObjectManager->Gui();
}
