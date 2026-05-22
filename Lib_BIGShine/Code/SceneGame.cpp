#include"SceneGame.h"
#include"Engine/GamePlay/Core/Object/GameObjectManager.h"
#include"Engine/Graphics/Core/GraphicsDeviceManager.h"
#include"Engine/GamePlay/Camera/CameraComponent.h"
#include"InputComponent.h"
#include"Engine/Physics/BoxCollider.h"
#include"Engine/Physics/CapsuleCollider.h"
#include"Engine/GamePlay/Serialize/ActionRegistry.h"
#include"Engine/GamePlay/Input/MouseInput.h"
#include "Engine/Audio/AudioManager.h"

void SceneGame::Finalize()
{
	SceneBase::Finalize();
}

void SceneGame::Initialize()
{
	SceneBase::Initialize();


	_GameObjectManager->ResolveHierarchy();
}

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
