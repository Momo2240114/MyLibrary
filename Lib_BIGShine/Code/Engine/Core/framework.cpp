#include "PrecompiledHeader.h"

#include "framework.h"
#include "Engine/Graphics/RenderSystem/ImGuiRenderer.h"
#include "ImGui.h"

#include "Engine/Graphics/Core/CPUResourceLoader.h"

#include "Engine/Graphics/Resource/GltfModel.h"


#include "Engine/Physics/BoxCollider.h"
#include "Engine/Physics/SphereCollider.h"
#include "Engine/Physics/CapsuleCollider.h"

#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"

#include "Engine/GamePlay/Input/MouseInput.h"
#include "Engine/GamePlay/Input/ControllerInput.h"
#include "Engine/GamePlay/Camera/FollowCameraComponent.h"

#include "Engine/Physics/PhysXSystem.h"
#include "Engine/GamePlay/Scene/SceneManager.h"
#include "Engine/Audio/AudioManager.h"

#include "Engine/Debug/TracyProfiler.h"

#include <imnodes.h>
#include <imnodes_internal.h>

#include <chrono>

framework::framework(HWND hwnd) : hwnd(hwnd){}										  


bool framework::initialize()
{
	GraphicsDeviceManager::Instance().Initialize(hwnd, SCREEN_WIDTH, SCREEN_HEIGHT, FULLSCREEN);

	ImGuiRenderer::Initialize(hwnd,
		GraphicsDeviceManager::Instance().GetDevice(),
		GraphicsDeviceManager::Instance().GetContext());

	// 2. imnodesの初期化
	ImNodes::CreateContext();
	// 3. スタイルの設定 (Dark/Classic/Lightから選択可能)
	ImNodes::StyleColorsDark();
	// (任意) IO設定: Alt+左ドラッグで画面をパン（移動）できるようにする
	ImNodes::GetIO().EmulateThreeButtonMouse.Modifier = &ImGui::GetIO().KeyAlt;

//デバッグレイヤー表示
#if defined(DEBUG) || defined(_DEBUG)
	// デバッグ用のインターフェースを取得する
	ID3D11InfoQueue* infoQueue = nullptr;
	HRESULT hr = GraphicsDeviceManager::Instance().GetDevice()->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&infoQueue);

	if (SUCCEEDED(hr)) {
		// エラー時にブレークさせる設定
		infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
		infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);

		// 使い終わったら Release する（生ポインタの場合）
		infoQueue->Release();
	}
#endif

	AudioManager::Instance().LoadFile();

	PhysXSystem::Initialize();

	//Guiでココンポーネントを追加できるようにレジスターにコンポーネントを登録する関数
	RegisterAllComponents();

	//シーン作成
	m_pSceneTitle = std::make_unique<SceneTitle>();

	SceneManager::Initialize(std::move(m_pSceneTitle));


	spriteRenderSystem.Initialize(
		GraphicsDeviceManager::Instance().GetDevice(),
		GraphicsDeviceManager::Instance().GetContext());



	return true;
}

void framework::update(float elapsedTime_/*Elapsed seconds from last frame*/)
{
	BS_PROFILE_SCOPE("framework::update");

	{
		BS_PROFILE_SCOPE("ImGuiRenderer::NewFrame");
		ImGuiRenderer::NewFrame();
	}

	{
		BS_PROFILE_SCOPE("Input");
		MouseInput::Instance().Update(hwnd);
		ControllerInput::Instance().Update();
	}

	{
		BS_PROFILE_SCOPE("SceneManager::Update");
		SceneManager::Update(elapsedTime_);
	}

	{
		BS_PROFILE_SCOPE("AudioManager::Update");
		AudioManager::Instance().Update();
	}

	{
		BS_PROFILE_SCOPE("PhysXSystem::Step");
		PhysXSystem::Step(elapsedTime_);
	}
}

//void framework::update(float elapsedTime_/*Elapsed seconds from last frame*/)
//{
//	ImGuiRenderer::NewFrame();
//
//	MouseInput::Instance().Update(hwnd);
//	ControllerInput::Instance().Update();
//
//	//シーン更新
//	//m_pSceneGame->Update(elapsedTime_);
//	SceneManager::Update(elapsedTime_);
//	AudioManager::Instance().Update();
//
//	PhysXSystem::Step(elapsedTime_);
//}

//void framework::render(float elapsedTime_/*Elapsed seconds from last frame*/)
//{
//	float color[4] = { 0.5f,0.5f, 0.5f, 1.0f };
//	GraphicsDeviceManager::Instance().BeginFrame(color);
//
//	SceneManager::Render(elapsedTime_);
//	// IMGUI描画
//	ImGuiRenderer::RenderAll(GraphicsDeviceManager::Instance().GetContext());
//
//	GraphicsDeviceManager::Instance().EndFrame();
//}

void framework::render(float elapsedTime_/*Elapsed seconds from last frame*/)
{
	BS_PROFILE_SCOPE("framework::render");

	float color[4] = { 0.5f,0.5f, 0.5f, 1.0f };

	{
		BS_PROFILE_SCOPE("GraphicsDeviceManager::BeginFrame");
		GraphicsDeviceManager::Instance().BeginFrame(color);
	}

	{
		BS_PROFILE_SCOPE("SceneManager::Render");
		SceneManager::Render(elapsedTime_);
	}

	{
		BS_PROFILE_SCOPE("ImGuiRenderer::RenderAll");
		ImGuiRenderer::RenderAll(GraphicsDeviceManager::Instance().GetContext());
	}

	{
		BS_PROFILE_SCOPE("GraphicsDeviceManager::EndFrame");
		GraphicsDeviceManager::Instance().EndFrame();
	}
}

int framework::run()
{
	MSG msg{};

	if (!initialize())
	{
		return 0;
	}

	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			BS_PROFILE_SCOPE("Win32 Message");

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			BS_PROFILE_SCOPE("Frame");
			const auto frameProfileStart_ = std::chrono::steady_clock::now();

			{
				BS_PROFILE_SCOPE("tictoc.tick");
				tictoc.tick();
			}

			{
				BS_PROFILE_SCOPE("calculate_frame_stats");
				calculate_frame_stats();
			}

			const float elapsedTime_ = tictoc.time_interval();
			BS_PROFILE_PLOT("Frame.TotalMilliseconds", static_cast<double>(elapsedTime_) * 1000.0);

			{
				BS_PROFILE_SCOPE("framework::update");
				const auto updateProfileStart_ = std::chrono::steady_clock::now();
				update(elapsedTime_);
				const auto updateProfileEnd_ = std::chrono::steady_clock::now();
				BS_PROFILE_PLOT(
					"Frame.UpdateMilliseconds",
					std::chrono::duration<double, std::milli>(updateProfileEnd_ - updateProfileStart_).count());
			}

			{
				BS_PROFILE_SCOPE("framework::GuiDraw");
				const auto guiProfileStart_ = std::chrono::steady_clock::now();
				GuiDraw();
				const auto guiProfileEnd_ = std::chrono::steady_clock::now();
				BS_PROFILE_PLOT(
					"Frame.GuiMilliseconds",
					std::chrono::duration<double, std::milli>(guiProfileEnd_ - guiProfileStart_).count());
			}

			{
				BS_PROFILE_SCOPE("framework::render");
				const auto renderProfileStart_ = std::chrono::steady_clock::now();
				render(elapsedTime_);
				const auto renderProfileEnd_ = std::chrono::steady_clock::now();
				BS_PROFILE_PLOT(
					"Frame.RenderCpuMilliseconds",
					std::chrono::duration<double, std::milli>(renderProfileEnd_ - renderProfileStart_).count());
			}

			const auto frameProfileEnd_ = std::chrono::steady_clock::now();
			BS_PROFILE_PLOT(
				"Frame.CpuMeasuredMilliseconds",
				std::chrono::duration<double, std::milli>(frameProfileEnd_ - frameProfileStart_).count());

			BS_PROFILE_FRAME_MARK();
		}
	}

	return uninitialize() ? static_cast<int>(msg.wParam) : 0;
}

LRESULT framework::handle_message(HWND hWnd_, UINT msg_, WPARAM wParam_, LPARAM lParam_)
{
    // Handle the system shortcut before ImGui can consume keyboard input.
    if (msg_ == WM_SYSKEYDOWN &&
        wParam_ == VK_RETURN &&
        (lParam_ & (1LL << 29)) != 0)
    {
        auto& graphics_ = GraphicsDeviceManager::Instance();
        graphics_.SetFullscreen(!graphics_.IsFullscreen());
        return 0;
    }

    if (ImGuiRenderer::HandleMessage(hWnd_, msg_, wParam_, lParam_))
    {
        return 0;
    }

    switch (msg_)
    {
    case WM_SIZE:
    {
        // 最小化中以外、かつデバイス初期化済みならリサイズ実行
        if (wParam_ != SIZE_MINIMIZED && GraphicsDeviceManager::Instance().GetDevice())
        {
            int w_ = LOWORD(lParam_);
            int h_ = HIWORD(lParam_);
            GraphicsDeviceManager::Instance().OnResize(w_, h_);
        }
        return 0;
    }

    case WM_MOUSEWHEEL:
    {
        int delta_ = GET_WHEEL_DELTA_WPARAM(wParam_);
        MouseInput::Instance().OnMouseWheel(delta_);
        return 0;
    }

    case WM_KILLFOCUS:
    {
        // Alt+Tab、Windowsキー、別ウィンドウ選択などで一時解除
        MouseInput::Instance().ReleaseCursorLock();
        return 0;
    }

    case WM_ACTIVATEAPP:
    {
        // wParam_ == FALSE のときアプリが非アクティブ
        if (wParam_ == FALSE)
        {
            MouseInput::Instance().ReleaseCursorLock();
        }
        return 0;
    }

    case WM_SETCURSOR:
    {
        // ゲーム中カーソル固定が有効ならカーソル表示を抑える
        if (MouseInput::Instance().IsCursorLockEnabled())
        {
            SetCursor(nullptr);
            return TRUE;
        }
        break;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps_{};
        BeginPaint(hWnd_, &ps_);
        EndPaint(hWnd_, &ps_);
        return 0;
    }

    case WM_DESTROY:
    {
        MouseInput::Instance().ReleaseCursorLock();
        PostQuitMessage(0);
        return 0;
    }

    case WM_CREATE:
    {
        return 0;
    }

    case WM_KEYDOWN:
    {
        if (wParam_ == VK_ESCAPE)
        {
            MouseInput::Instance().ReleaseCursorLock();
            PostMessage(hWnd_, WM_CLOSE, 0, 0);
        }
        return 0;
    }

    case WM_ENTERSIZEMOVE:
    {
        MouseInput::Instance().ReleaseCursorLock();
        tictoc.stop();
        return 0;
    }

    case WM_EXITSIZEMOVE:
    {
        tictoc.start();
        return 0;
    }

    default:
        return DefWindowProc(hWnd_, msg_, wParam_, lParam_);
    }

    return DefWindowProc(hWnd_, msg_, wParam_, lParam_);
}


bool framework::uninitialize()
{
	//m_pSceneGame->Finalize();
	SceneManager::Clear();

	PhysXSystem::Finalize();
	AudioManager::Instance().Finalize();

	spriteRenderSystem.Finalize();
	GraphicsDeviceManager::Instance().Finalize();


	ImNodes::DestroyContext();
	// IMGUI終了化
	ImGuiRenderer::Finalize();

	return true;
}

void framework::GuiDraw()
{
	//#if defined(DEBUG) || defined(_DEBUG)
	SceneManager::DrawGUI();
	//GraphicsDeviceManager::Instance().ShowPostEffectGui(); // ここで呼び出す
	//#endif // DEBUG
}

framework::~framework()
{
}


