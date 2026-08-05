#pragma once

#include"SceneBase.h"

#include<memory>

class PlayerComponent;
class ExitComponent;

enum  TutorialState
{
	Tutorial_Start = 0, // Tutorial開始時の初期化フェーズ。
	Phase_01_Update,   // Phase01の説明・入力待ち更新。
	Phase_01_End,      // Phase01完了時の切り替え処理。
	Phase_02_Update,   // Phase02の説明・入力待ち更新。
	Phase_02_End,      // Phase02完了時の切り替え処理。
	Phase_03_Update,   // Phase03の説明・入力待ち更新。
	Phase_03_End,      // Phase03完了時の切り替え処理。
	Phase_04_Update,   // Phase04の説明・入力待ち更新。
	Phase_04_End,      // Phase04完了時の切り替え処理。
	Phase_05_Update,   // Phase05の説明・入力待ち更新。
	Phase_05_End,      // Tutorial終了へ向かう最後の切り替え処理。
};

// チュートリアル専用Scene。段階ごとの状態をm_Stepで進め、操作説明とゲーム進行を分けて管理する。
class SceneTutorial :public SceneBase
{
public:
	SceneTutorial()
	{
		std::string name = "TutorialScene";
		sceneName = name;
	}

	void Initialize();
	void Finalize();
	// 更新処理
	void Update(float elapsedTime);
	// 描画処理
	void Render(float elapsedTime);
	void DrawGUI();

private:
	// Tutorial Scene内で使う描画System。Scene側で生成したObjectを用途別に描画する。
	ModelRenderSystem m_ModelRenderSystem;
	SpriteRenderSystem m_SpriteRenderSystem;
	ShapeRenderSystem m_ShapeRenderSystem;

	
	//int m_Step = 0;
	// 現在のTutorial進行段階。Update内の状態遷移で次Phaseへ進める。
	TutorialState m_Step;
	
	// Phase内での待ち時間や演出時間を計測するためのタイマー。
	float m_Time = 0.0f;
	//std::weak_ptr<PlayerComponent> m_pPlayerComponent;
	//std::weak_ptr<ExitComponent> m_pExitComponent;
};
