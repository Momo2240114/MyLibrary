#include "SceneTutorial.h"
#include"GameObjectManager.h"
#include"GraphicsDeviceManager.h"
#include"CameraComponent.h"
#include"InputComponent.h"
#include"PlayerComponent.h"
#include"BoxCollider.h"
#include"CapsuleCollider.h"

#include"MouseInput.h"
#include"KeyInput.h"

#include "SceneGame.h"
#include"TransformDirector.h"
#include"CameraDirectorComponent.h"
#include"ActionSequence.h"

#include"ColorFilterDirector.h"
#include"ExitComponent.h"
#include "AudioManager.h"

void SceneTutorial::Finalize()
{
	m_pNaviMeshRenderer.reset();
	m_SpriteRenderSystem.Finalize();
	GameObjectManager::Instance().Clear();
	actionRegistry.reset();
}

void SceneTutorial::DrawGUI()
{
	GameObjectManager::Instance().Gui();
}


void SceneTutorial::Initialize()
{
	m_Time = 0.0f;

	//GameObjectManager::Instance().Initialize();

	auto navi = GameObjectManager::Instance().GetNaviMeshManager();

	actionRegistry = std::make_shared<ActionRegistry>();
	ActionRegistry::SetInstance(actionRegistry);
	MouseInput::Instance().SetDebugFlag(true);

	GameObjectManager::Instance().LoadScene("TutorialScene");


	//ÉÇÉfÉãÇÃÉåÉìÉ_ÉâÅ[ÇçÏê¨
	m_ModelRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice());
	m_SpriteRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice(), GraphicsDeviceManager::Instance().GetContext());
	m_ShapeRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice());



	//m_pNaviMeshRenderer = std::make_unique<NavMeshRenderer>(GraphicsDeviceManager::Instance().GetDevice());

	std::weak_ptr<GameObject>PlayerGameObject = GameObjectManager::Instance().Find("MainCamera");
	AudioManager::Instance().PlayBGM(BGM_TUTORIAL);
}

void SceneTutorial::Update(float elapsedTime)
{
	GameObjectManager::Instance().Update(elapsedTime);
}

void SceneTutorial::Render(float elapsedTime)
{
	GameObjectManager::Instance().RenderAll(m_SpriteRenderSystem, m_ModelRenderSystem);
}


