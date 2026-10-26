#include "PrecompiledHeader.h"
#include "Engine/UI/ScreenTransitionManager.h"

#include "framework.h"
#include "Engine/Core/GameRuntimeSettings.h"
#include "Engine/Graphics/RenderSystem/ImGuiRenderer.h"
#include "ImGui.h"

#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Graphics/Resource/ModelResourceManager.h"
#include "Engine/Graphics/Resource/TextureResourceManager.h"
#include "Engine/Graphics/VFXMaterial/VFXMaterialResourceManager.h"

#include "Engine/Graphics/Resource/GltfModel.h"


#include "Engine/Physics/BoxCollider.h"
#include "Engine/Physics/SphereCollider.h"
#include "Engine/Physics/CapsuleCollider.h"

#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"

#include "Engine/GamePlay/Input/KeyInput.h"
#include "Engine/GamePlay/Input/MouseInput.h"
#include "Engine/GamePlay/Input/ControllerInput.h"
#include "Engine/GamePlay/Input/KeyConfig.h"
#include "Engine/GamePlay/Camera/FollowCameraComponent.h"

#include "Engine/Physics/PhysXSystem.h"
#include "Engine/GamePlay/Scene/SceneManager.h"
#include "Engine/GamePlay/Scene/SceneBase.h"
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
#if BS_ENABLE_D3D11_DEBUG_LAYER && (defined(DEBUG) || defined(_DEBUG))
	// デバッグ用のインターフェースを取得する
	ID3D11InfoQueue* infoQueue = nullptr;
	HRESULT hr = GraphicsDeviceManager::Instance().GetDevice()->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&infoQueue);

	if (SUCCEEDED(hr)) {
		// エラー時にブレークさせる設定
		infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
		infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);

		// DirectX Debug Layerの保存ログが増え続けないよう、直近のメッセージだけ保持する
		constexpr UINT64 INFO_QUEUE_MESSAGE_LIMIT = 1024u;
		infoQueue->SetMessageCountLimit(INFO_QUEUE_MESSAGE_LIMIT);

		// 使い終わったら Release する（生ポインタの場合）
		infoQueue->Release();
	}
#endif

	AudioManager::Instance().LoadFile();

	PhysXSystem::Initialize();

	//Guiでココンポーネントを追加できるようにレジスターにコンポーネントを登録する関数
	RegisterAllComponents();

	//シーン作成
	m_pSceneStartUp = std::make_unique<SceneStartUp>();

    ScreenTransitionManager::Instance().Initialize();
	SceneManager::Initialize(std::move(m_pSceneStartUp));

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
        // 入力を参照しないフレームも状態を進め、次の押下を確実に検出する。
        KeyInput::Update();
        const auto currentScene_ = SceneManager::GetCurrentScene();
        const auto gameScene_ = dynamic_cast<SceneBase*>(currentScene_);
        const bool playerDeathActive_ = gameScene_ && gameScene_->GetGameObjectManager() &&
            gameScene_->GetGameObjectManager()->IsPlayerDeathActive();
        const bool isWindowActive_ = GetForegroundWindow() == hwnd;
        const bool toggleCursor_ = !GameRuntimeSettings::IsGameOnly() && isWindowActive_ &&
            currentScene_ && !currentScene_->RequiresFreeCursor() &&
            ImGui::IsKeyPressed(ImGuiKey_F2, false);
        bool usesEditorPlayMode_ = false;
        if (const auto scene_ = dynamic_cast<SceneBase*>(SceneManager::GetCurrentScene()))
            if (const auto manager_ = scene_->GetGameObjectManager(); manager_ && manager_->SupportsEditorPlayMode())
            {
                usesEditorPlayMode_ = true;
                // クリックやEnterでは捕捉せず、F2を押した実行中のフレームだけ切り替える。
                if (!isWindowActive_) manager_->SetPlayInputCaptured(false);
                else if (toggleCursor_ && manager_->GetEditorPlayMode() == EditorPlayMode::PLAY)
                {
                    MouseInput::Instance().SetDebugFlag(false);
                    manager_->SetPlayInputCaptured(!manager_->IsPlayInputCaptured());
                }
                const bool capture_ = manager_->IsPlayInputCaptured();
                // 状態が同じ時の再設定で、自由マウスの移動差分を毎フレーム消さない。
                if (MouseInput::Instance().IsCursorLockEnabled() != capture_)
                    MouseInput::Instance().SetCursorLockEnabled(capture_);
            }
        // Editor Play非対応の開発用Sceneでも、解放操作はF2へ統一する。
        if (!usesEditorPlayMode_ && toggleCursor_) MouseInput::Instance().ToggleDebugCursor();
        // メニューSceneでは自由マウスを使い、本編の配布版はゲーム入力を取得する。
        // コンテニュー画面では本編の自動捕捉を止め、毎フレーム自由なマウス操作を維持する。
        if (playerDeathActive_)
        {
            if (MouseInput::Instance().IsCursorLockEnabled()) MouseInput::Instance().SetCursorLockEnabled(false);
        }
        else if (currentScene_ && currentScene_->RequiresFreeCursor())
        {
            if (MouseInput::Instance().IsCursorLockEnabled()) MouseInput::Instance().SetCursorLockEnabled(false);
        }
        else if (GameRuntimeSettings::IsGameOnly() && !MouseInput::Instance().IsCursorLockEnabled())
            MouseInput::Instance().SetCursorLockEnabled(true);
		MouseInput::Instance().Update(hwnd);
		ControllerInput::Instance().Update();
		KeyConfig::Instance().UpdateActiveDevice(GetForegroundWindow() == hwnd);
	}

	{
		BS_PROFILE_SCOPE("SceneManager::Update");
        // 前フレームの被覆通知を、Object更新やSceneの破棄に入る前に処理する。
        if (const auto scene_ = dynamic_cast<SceneBase*>(SceneManager::GetCurrentScene())) scene_->ActivateContext();
        ScreenTransitionManager::Instance().Update(elapsedTime_);
		SceneManager::Update(elapsedTime_);
	}

	{
		BS_PROFILE_SCOPE("AudioManager::Update");
        if (const auto scene_ = dynamic_cast<SceneBase*>(SceneManager::GetCurrentScene()))
            if (const auto manager_ = scene_->GetGameObjectManager())
            {
                // ゲーム内メニューの操作音と死亡時のBGM減衰を維持し、Editor Pauseは従来どおり反映する。
                const bool paused_ = manager_->IsCinematicSequencePreview() ?
                    manager_->IsCinematicSequencePaused() || manager_->IsCinematicSequenceSeeking() :
                    manager_->GetEditorPlayMode() != EditorPlayMode::EDITOR &&
                    !manager_->ShouldSimulatePhysics() && !(manager_->GetEditorPlayMode() == EditorPlayMode::PLAY &&
                        (manager_->IsGameMenuPaused() || manager_->IsPlayerDeathActive()));
                AudioManager::Instance().SetEditorPlayPaused(paused_);
            }
		AudioManager::Instance().Update();
	}

	{
		BS_PROFILE_SCOPE("PhysXSystem::Step");
        bool simulate_ = true;
        if (const auto scene_ = dynamic_cast<SceneBase*>(SceneManager::GetCurrentScene()))
            if (const auto manager_ = scene_->GetGameObjectManager()) simulate_ = manager_->ShouldSimulatePhysics();
        if (simulate_) PhysXSystem::Step(elapsedTime_);
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
	auto tracyGpuContext_ = GraphicsDeviceManager::Instance().GetTracyGpuContext();

	float color[4] = { 0.5f,0.5f, 0.5f, 1.0f };

	{
		BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Frame");

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
            // Sprite・Particle・ImGuiを含む最終画面を覆ってからPresentする。
            SceneManager::RenderTransition();
            ScreenTransitionManager::Instance().Render();
		}
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
        // 解放中や非クライアント領域では隠さず、実際に固定中のカーソルだけを抑える。
        if (LOWORD(lParam_) == HTCLIENT && GetForegroundWindow() == hWnd_ &&
            MouseInput::Instance().IsCursorLocked())
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
        // ESCはScene側のポーズ入力として扱い、ウィンドウを閉じない。
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
	BS_PROFILE_SCOPE("framework::uninitialize");

	//m_pSceneGame->Finalize();
	SceneManager::Clear();
    ScreenTransitionManager::Instance().Shutdown();

	PhysXSystem::Finalize();
	AudioManager::Instance().Finalize();

	spriteRenderSystem.Finalize();


	ImGuiRenderer::Finalize();
	ImNodes::DestroyContext();

	VFXMaterialResourceManager::GetInstance().Clear();
	TextureResourceManager::Instance().Clear();
	ModelResourceManager::Instance().Clear();

	GraphicsDeviceManager::Instance().Finalize();
	return true;
}

void framework::GuiDraw()
{
	//#if defined(DEBUG) || defined(_DEBUG)
	SceneManager::DrawGUI();
    ScreenTransitionManager::Instance().DrawGUI(!GameRuntimeSettings::IsGameOnly());
	//GraphicsDeviceManager::Instance().ShowPostEffectGui(); // ここで呼び出す
	//#endif // DEBUG
}

framework::~framework()
{
}


