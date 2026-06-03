#pragma once


#include"SceneBase.h"


class SceneGame :public SceneBase
{
public:
	SceneGame()
	{
		std::string name = "GameScene";
		sceneName = name;
	}

	void Initialize();
	void Finalize();
	// XVˆ—
	void Update(float elapsedTime);

	// •`‰æˆ—
	void Render(float elapsedTime);

	void DrawGUI();

};