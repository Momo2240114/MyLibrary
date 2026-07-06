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
	// 更新処理
	void Update(float elapsedTime);

	// 描画処理
	void Render(float elapsedTime);
};