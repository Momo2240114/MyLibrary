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
	// 更新処理
	void Update(float elapsedTime);

	// 描画処理
	void Render(float elapsedTime);

	void DrawGUI();

};