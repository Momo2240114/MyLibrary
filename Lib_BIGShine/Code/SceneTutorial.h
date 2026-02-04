#pragma once

#include"IScene.h"
#include"ModelRenderSystem.h"
#include"SpriteRenderSystem.h"
#include"ShapeRenderer.h"

#include<memory>

class PlayerComponent;
class ExitComponent;

enum  TutorialState
{
	Tutorial_Start = 0,
	Phase_01_Update,
	Phase_01_End,
	Phase_02_Update,
	Phase_02_End,
	Phase_03_Update,
	Phase_03_End,
	Phase_04_Update,
	Phase_04_End,
	Phase_05_Update,
	Phase_05_End,
};

class SceneTutorial :public Scene
{
public:
	SceneTutorial()
	{
		std::string name = "TutorialScene";
		sceneName = name;
	}

	void Initialize();
	void Finalize();
	// XVˆ—
	void Update(float elapsedTime);
	// •`‰æˆ—
	void Render(float elapsedTime);
	void DrawGUI();

private:
	ModelRenderSystem m_ModelRenderSystem;
	SpriteRenderSystem m_SpriteRenderSystem;
	ShapeRenderSystem m_ShapeRenderSystem;

	
	//int m_Step = 0;
	TutorialState m_Step;
	
	float m_Time = 0.0f;
	//std::weak_ptr<PlayerComponent> m_pPlayerComponent;
	//std::weak_ptr<ExitComponent> m_pExitComponent;
};