#pragma once

#include <windows.h>
#include <tchar.h>
#include <sstream>
#include <d3d11.h> 
#include <WRL.h> 

//#include "../PhisXLib/Pch.h"


#include "misc.h"
#include "high_resolution_timer.h"

#include "GraphicsDeviceManager.h"
#include "SpriteRenderSystem.h"
#include "GameObject.h"


#include"SceneGame.h"
#include"SceneTitle.h"


#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080
#define FULLSCREEN TRUE
#define APPLICATION_NAME L"Silent Escape"

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
		//if (++frames_per_second, (tictoc.time_stamp() - count_by_seconds) >= 1.0f)
		//{
		//	float fps = static_cast<float>(frames_per_second);
		//	std::wostringstream outs;
		//	outs.precision(6);
		//	outs << L"Silent Escape";
		//	SetWindowTextW(hwnd, outs.str().c_str());

		//	frames_per_second = 0;
		//	count_by_seconds += 1.0f;
		//}
	}

private:
	//GraphicsDeviceManager graphicsDeviceManager;
	SpriteRenderSystem spriteRenderSystem;
	std::shared_ptr<SpriteRenderer> sprite[3]; // 描画したいスプライト
	std::vector<std::shared_ptr<GameObject>> objects;

	//テスト用
	GltfModelCommponennt::Node* selectionNode = nullptr;
	std::vector<GltfModelCommponennt::NodePose>		nodePoses;
	bool								animationPlaying = false;
	bool								animationLoop = false;
	float								animationSamplingRate = 60;
	float								animationBlendSeconds = 0;
	float								animationSpeed = 1.0f;
	float								currentAnimationSeconds = 0;
	int									currentAnimationIndex = -1;

	std::unique_ptr<SceneGame> m_pSceneGame;
	std::unique_ptr<SceneTitle> m_pSceneTitle;
};

