#pragma once

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
#define APPLICATION_NAME L"BigShine"

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
	void update(float elapsedTime/*Elapsed seconds from last frame*/);
	void render(float elapsedTime/*Elapsed seconds from last frame*/);
	bool uninitialize();
	void GuiDraw();
private:
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
			outs << "FPS : " << fps << " / " << "Frame Time : " << mspf << " (ms)";
			SetWindowTextW(hwnd, outs.str().c_str());

				// Reset for next average.

			frames_per_second = 0;
			count_by_seconds += 1.0f;
		}
	}

private:
	SpriteRenderSystem spriteRenderSystem;

	std::unique_ptr<SceneGame> m_pSceneGame;
	std::unique_ptr<SceneStartUp> m_pSceneStartUp;
};

