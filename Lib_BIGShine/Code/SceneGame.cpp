#include"SceneGame.h"
#include"GameObjectManager.h"
#include"GraphicsDeviceManager.h"
#include"CameraComponent.h"
#include"InputComponent.h"
#include"BoxCollider.h"
#include"CapsuleCollider.h"
#include"ActionRegistry.h"
#include"MouseInput.h"
#include "Engine/Audio/AudioManager.h"

void SceneGame::Finalize()
{
	m_SpriteRenderSystem.Finalize();
	GameObjectManager::Instance().Clear();
	actionRegistry.reset();
}

void SceneGame::Initialize()
{
	actionRegistry = std::make_shared<ActionRegistry>();
	ActionRegistry::SetInstance(actionRegistry);


	//ÉÇÉfÉãÇÃÉåÉìÉ_ÉâÅ[ÇçÏê¨
	m_ModelRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice());
	m_SpriteRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice(), GraphicsDeviceManager::Instance().GetContext());
	m_ShapeRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice());

	GameObjectManager::Instance().ResolveHierarchy();
}

void SceneGame::Update(float elapsedTime)
{
	GameObjectManager::Instance().Update(elapsedTime);
}

void SceneGame::Render(float elapsedTime)
{
	GameObjectManager::Instance().RenderAll(m_SpriteRenderSystem,m_ModelRenderSystem);
}

void SceneGame::DrawGUI()
{
	GameObjectManager::Instance().Gui();
}
