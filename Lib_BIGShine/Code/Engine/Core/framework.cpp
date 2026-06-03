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
#include "PlayerInput.h"
#include "Engine/GamePlay/Camera/FollowCameraComponent.h"

#include "Engine/Physics/PhysXSystem.h"
#include "Engine/GamePlay/Scene/SceneManager.h"
#include "Engine/Audio/AudioManager.h"

#include <imnodes.h>
#include <imnodes_internal.h>

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
	ImGuiRenderer::NewFrame();

	MouseInput::Instance().Update(hwnd);

	//シーン更新
	//m_pSceneGame->Update(elapsedTime_);
	SceneManager::Update(elapsedTime_);

	PhysXSystem::Step(elapsedTime_);
}

void framework::render(float elapsedTime_/*Elapsed seconds from last frame*/)
{
	float color[4] = { 0.5f,0.5f, 0.5f, 1.0f };
	GraphicsDeviceManager::Instance().BeginFrame(color);

	SceneManager::Render(elapsedTime_);
	// IMGUI描画
	ImGuiRenderer::RenderAll(GraphicsDeviceManager::Instance().GetContext());

	GraphicsDeviceManager::Instance().EndFrame();
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
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			tictoc.tick();
			calculate_frame_stats();

			update(tictoc.time_interval());
			GuiDraw();
			render(tictoc.time_interval());
		}
	}




	return uninitialize() ? static_cast<int>(msg.wParam) : 0;
}

LRESULT framework::handle_message(HWND hWnd_, UINT msg_, WPARAM wParam_, LPARAM lParam_)
{
	if (ImGuiRenderer::HandleMessage(hWnd_, msg_, wParam_, lParam_))
		return 0; 

	switch (msg_)
	{
	case WM_SIZE:
		// 最小化中以外、かつデバイス初期化済みならリサイズ実行
		if (wParam_ != SIZE_MINIMIZED && GraphicsDeviceManager::Instance().GetDevice())
		{
			int w = LOWORD(lParam_);
			int h = HIWORD(lParam_);
			GraphicsDeviceManager::Instance().OnResize(w, h);
		}
		return 0;
	case WM_MOUSEWHEEL:
	{
		int delta = GET_WHEEL_DELTA_WPARAM(wParam_);
		MouseInput::Instance().OnMouseWheel(delta);
		break;
	}
	case WM_PAINT:
	{
		PAINTSTRUCT ps{};
		BeginPaint(hwnd, &ps);

		EndPaint(hwnd, &ps);
	}
	break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_CREATE:
		break;
	case WM_KEYDOWN:
		if (wParam_ == VK_ESCAPE)
		{
			PostMessage(hWnd_, WM_CLOSE, 0, 0);
		}
		break;
	case WM_ENTERSIZEMOVE:
		tictoc.stop();
		break;
	case WM_EXITSIZEMOVE:
		tictoc.start();
		break;
	default:
		return DefWindowProc(hWnd_, msg_, wParam_, lParam_);
	}
	return 0;
}


bool framework::uninitialize()
{
	//m_pSceneGame->Finalize();
	SceneManager::Clear();

	PhysXSystem::Finalize();

	spriteRenderSystem.Finalize();
	GraphicsDeviceManager::Instance().Finalize();


	ImNodes::DestroyContext();
	// IMGUI終了化
	ImGuiRenderer::Finalize();

	return true;
}

void framework::GuiDraw()
{
#if defined(DEBUG) || defined(_DEBUG)
	SceneManager::DrawGUI();
	//GraphicsDeviceManager::Instance().ShowPostEffectGui(); // ここで呼び出す
#endif // DEBUG
}

framework::~framework()
{
}