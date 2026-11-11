#pragma once

#include "Engine/Core/BuildConfig.h"
#include <windows.h>
#include <tchar.h>
#include <sstream>
#include <d3d11.h> 
#include <WRL.h> 

//#include "../PhisXLib/Pch.h"


#include "Engine/Common/misc.h"
#include "high_resolution_timer.h"

#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/RenderSystem/SpriteRenderSystem.h"

#include"Engine/GamePlay/Scene/SceneGame.h"
#include"Engine/GamePlay/Scene/SceneStartUp.h"


#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080
#define FULLSCREEN FALSE
// ビルド対象の名称を、ウィンドウクラスとタイトルで共通使用する。
#if defined(BS_APP_GAME) || defined(BS_BUILD_SHIPPING)
#define APPLICATION_NAME L"ANATHEMA"
#else
#define APPLICATION_NAME L"BigShine Engine"
#endif

// Win32ウィンドウ、メインループ、Scene、基本描画Systemを束ねるアプリケーション本体。
class framework
{
public:
	CONST HWND hwnd;


	framework(HWND hwnd);
	~framework();

	framework(const framework&) = delete;
	framework& operator=(const framework&) = delete;
	framework(framework&&) noexcept = delete;
	framework& operator=(framework&&) noexcept = delete;

	int run();
	LRESULT CALLBACK handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private:
	bool initialize();
	// elapsedTimeは前フレームからの経過秒。Scene更新やEditor更新の基準時間として扱う。
	void update(float elapsedTime/*Elapsed seconds from last frame*/);
	void render(float elapsedTime/*Elapsed seconds from last frame*/);
	bool uninitialize();
	void GuiDraw();
private:
	// FPS計測は累積時間を使い、開発用構成だけでWindowタイトルへ反映する。
	high_resolution_timer tictoc;
	uint32_t frames_per_second{ 0 };
	float count_by_seconds{ 0.0f };
	void calculate_frame_stats()
	{
		if (++frames_per_second, (tictoc.time_stamp() - count_by_seconds) >= 1.0f)
		{
			float fps = static_cast<float>(frames_per_second);
			float mspf = 1000.0f / fps;
			std::wostringstream outs;
			outs.precision(6);
			//outs << L"Silent Escape";
			//SetWindowTextW(hwnd, outs.str().c_str());
			outs << APPLICATION_NAME << L" | FPS : " << fps << " / " << "Frame Time : " << mspf << " (ms)";
#if !defined(BS_APP_GAME) && !defined(BS_BUILD_SHIPPING)
			SetWindowTextW(hwnd, outs.str().c_str());
#endif

			// 次の1秒平均を取り直すため、フレーム数だけリセットして時間基準を進める。

			frames_per_second = 0;
			count_by_seconds += 1.0f;
		}
	}

private:
	// 2D描画やUI向けSprite描画をまとめるRenderSystem。
	SpriteRenderSystem spriteRenderSystem;

	// 起動中に切り替える主要Scene。frameworkは生成/破棄とメインループからの呼び出しだけを担当する。
	std::unique_ptr<SceneGame> m_pSceneGame;
	std::unique_ptr<SceneStartUp> m_pSceneStartUp;
};

