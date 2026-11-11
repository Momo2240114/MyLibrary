#pragma once


#include"SceneBase.h"


class SceneGame :public SceneBase
{
public:
	// 直接生成は従来の編集起動、Scene Action経由は即時プレイとして扱う。
	SceneGame(bool startImmediately_ = false) : startImmediately(startImmediately_)
	{
		std::string name = "GameScene";
		sceneName = name;
        isEditorSuppressed = startImmediately_ && !SceneManager::IsEditorPlaySessionActive();
	}

	void Initialize();
	void Finalize();
	// 更新処理
	void Update(float elapsedTime);

	// 描画処理
	void Render(float elapsedTime);

	void DrawGUI();
    bool IsEditorDestination() const override { return !startImmediately && !SceneManager::IsEditorPlaySessionActive(); }
    bool RequiresFreeCursor() const override;

private:
    bool startImmediately = false;

};
