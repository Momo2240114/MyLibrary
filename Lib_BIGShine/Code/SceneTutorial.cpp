#include "SceneTutorial.h"
#include"Engine/GamePlay/Core/Object/GameObjectManager.h"
#include"Engine/Graphics/Core/GraphicsDeviceManager.h"
#include"Engine/GamePlay/Camera/CameraComponent.h"
#include"InputComponent.h"
#include"Engine/Physics/BoxCollider.h"
#include"Engine/Physics/CapsuleCollider.h"

#include"Engine/GamePlay/Input/MouseInput.h"
#include"Engine/GamePlay/Input/KeyInput.h"

#include "SceneGame.h"
#include"TransformDirector.h"
#include"CameraDirectorComponent.h"
#include"Engine/GamePlay/IventSystem/ActionSequence.h"

#include"ColorFilterDirector.h"
#include"ExitComponent.h"
#include "Engine/Audio/AudioManager.h"

void SceneTutorial::Finalize()
{
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

	actionRegistry = std::make_shared<ActionRegistry>();
	ActionRegistry::SetInstance(actionRegistry);
	MouseInput::Instance().SetDebugFlag(true);

	GameObjectManager::Instance().LoadScene("TutorialScene");


	//ÉÇÉfÉãÇÃÉåÉìÉ_ÉâÅ[ÇçÏê¨
	m_ModelRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice());
	m_SpriteRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice(), GraphicsDeviceManager::Instance().GetContext());
	m_ShapeRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice());

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


