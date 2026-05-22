#include "SceneTitle.h"
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
#include "Engine/Audio/AudioManager.h"


void SceneTitle::Finalize()
{
	SceneBase::Finalize();
}

void SceneTitle::DrawGUI()
{
	SceneBase::DrawGUI();


}


void SceneTitle::Initialize()
{
	SceneBase::Initialize();


	_GameObjectManager->LoadScene("MainScene");

	_GameObjectManager->ResolveHierarchy();
}

void SceneTitle::Update(float elapsedTime)
{
	SceneBase::Update(elapsedTime);
}

void SceneTitle::Render(float elapsedTime)
{
	SceneBase::Render(elapsedTime);
}

