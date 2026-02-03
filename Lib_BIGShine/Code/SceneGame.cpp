#include "SceneGame.h"
#include"GameObjectManager.h"
#include"GraphicsDeviceManager.h"
#include"CameraComponent.h"
#include"InputComponent.h"
#include"PlayerComponent.h"
#include"BoxCollider.h"
#include"CapsuleCollider.h"
#include"EnemyComponent.h"
#include"AreaComponent.h"
#include"ActionRegistry.h"
#include"MouseInput.h"
#include"AudioManager.h"

void SceneGame::Finalize()
{
	m_pNaviMeshRenderer.reset();
	m_SpriteRenderSystem.Finalize();
	GameObjectManager::Instance().Clear();
	actionRegistry.reset();
}

void SceneGame::Initialize()
{
	auto navi = GameObjectManager::Instance().GetNaviMeshManager();

	actionRegistry = std::make_shared<ActionRegistry>();
	ActionRegistry::SetInstance(actionRegistry);

	//GameObjectManager::Instance().LoadScene("ObjectScene");

	//ÉÇÉfÉãÇÃÉåÉìÉ_ÉâÅ[ÇçÏê¨
	m_ModelRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice());
	m_SpriteRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice(), GraphicsDeviceManager::Instance().GetContext());
	m_ShapeRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice());
	
	m_pNaviMeshRenderer = std::make_unique<NavMeshRenderer>(GraphicsDeviceManager::Instance().GetDevice());

	GameObjectManager::Instance().ResolveHierarchy();
	//GameObjectManager::Instance().InitializeAllComponents();

	//MouseInput::Instance().SetDebugFlag(false);
	//AudioManager::Instance().PlayBGM(BGM_GAME);
}

void SceneGame::Update(float elapsedTime)
{
	GameObjectManager::Instance().Update(elapsedTime);
}

void SceneGame::Render(float elapsedTime)
{
	GameObjectManager::Instance().RenderAll(m_SpriteRenderSystem,m_ModelRenderSystem, m_pNaviMeshRenderer.get());
}

void SceneGame::DrawGUI()
{
	GameObjectManager::Instance().Gui();
}
