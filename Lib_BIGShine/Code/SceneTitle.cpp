#include "SceneTitle.h"
#include"GameObjectManager.h"
#include"GraphicsDeviceManager.h"
#include"CameraComponent.h"
#include"InputComponent.h"
#include"BoxCollider.h"
#include"CapsuleCollider.h"

#include"MouseInput.h"
#include"KeyInput.h"

#include "SceneGame.h"

#include"TransformDirector.h"
#include"CameraDirectorComponent.h"
#include"ActionSequence.h"

#include"ColorFilterDirector.h"
#include "AudioManager.h"

void SceneTitle::Finalize()
{
	m_SpriteRenderSystem.Finalize();
	GameObjectManager::Instance().Clear();
	actionRegistry.reset();
}

void SceneTitle::DrawGUI()
{
	GameObjectManager::Instance().Gui();
}


void SceneTitle::Initialize()
{
	actionRegistry = std::make_shared<ActionRegistry>();
	ActionRegistry::SetInstance(actionRegistry);
	RegisterAllActions();

	MouseInput::Instance().SetDebugFlag(true);

	GameObjectManager::Instance().LoadScene("MainScene");

	//ÉÇÉfÉãÇÃÉåÉìÉ_ÉâÅ[ÇçÏê¨
	m_ModelRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice());
	m_SpriteRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice(), GraphicsDeviceManager::Instance().GetContext());

	GameObjectManager::Instance().ResolveHierarchy();
}

void SceneTitle::Update(float elapsedTime)
{
	GameObjectManager::Instance().Update(elapsedTime);
}

void SceneTitle::Render(float elapsedTime)
{
	GameObjectManager::Instance().RenderAll(m_SpriteRenderSystem, m_ModelRenderSystem);
}

