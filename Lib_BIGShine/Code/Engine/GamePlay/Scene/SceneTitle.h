#pragma once

#include"SceneBase.h"

class SceneTitle :public SceneBase
{
public:
	SceneTitle()
	{
		std::string name = "TitleScene";
		sceneName = name;
	}

	void Initialize();
	void Finalize();
	void DrawGUI()override;
	// XVˆ—
	void Update(float elapsedTime);

	// •`‰æˆ—
	void Render(float elapsedTime);
};