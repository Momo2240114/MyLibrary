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
    MouseInput::Instance().RegisterRawMouse(hwnd);

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
        const auto scene_ = dynamic_cast<SceneBase*>(SceneManager::GetCurrentScene());
        const auto manager_ = scene_ ? scene_->GetGameObjectManager() : nullptr;
        const bool focused_ = GetForegroundWindow() == hwnd && GetFocus() == hwnd && !IsIconic(hwnd);
        const bool systemKey_ = KeyInput::KeyPressed(VK_LWIN) || KeyInput::KeyPressed(VK_RWIN);
        const bool editorPlay_ = manager_ && manager_->SupportsEditorPlayMode();
        const bool canCapture_ = manager_ && manager_->GetEditorPlayMode() == EditorPlayMode::PLAY &&
            manager_->GetSceneRuntimeState() == SCENE_RUNTIME_STATE::RUNNING &&
            !manager_->IsEditorStopRequested() && !manager_->IsEditorPauseRequested() &&
            !manager_->IsCinematicSequencePreview() && !manager_->IsPlayerDeathActive() &&
            !scene_->IsEditorWorkspaceOpen() && !scene_->RequiresFreeCursor() &&
            !ScreenTransitionManager::Instance().IsInputBlocked();
        auto& mouse_ = MouseInput::Instance();
        const bool releaseKey_ = KeyInput::KeyPressedIn(VK_ESCAPE) ||
            (editorPlay_ && KeyInput::KeyPressedIn(VK_F1));
        if (!focused_ || systemKey_ || (mouse_.IsCursorLocked() && releaseKey_))
        {
            if (mouse_.IsCursorLockEnabled()) mouseRecaptureRequired = true;
            if (editorPlay_) manager_->SetPlayInputCaptured(false);
        }

        bool capture_ = false;
        if (focused_ && !systemKey_ && canCapture_)
        {
            const bool toggle_ = KeyInput::KeyPressedIn(VK_F2) && !ImGui::GetIO().WantTextInput;
            if (editorPlay_)
            {
                // Scene画像のクリックが前フレームで捕捉を要求した場合は、解除待ちを終える。
                if (mouseRecaptureRequired)
                {
                    if (manager_->IsPlayInputCaptured()) mouseRecaptureRequired = false;
                    else manager_->SetPlayInputCaptured(false);
                }
                if (toggle_ && !releaseKey_)
                {
                    manager_->SetPlayInputCaptured(!manager_->IsPlayInputCaptured());
                    mouseRecaptureRequired = false;
                }
                capture_ = manager_->IsPlayInputCaptured() && !mouseRecaptureRequired;
            }
            else if (dynamic_cast<SceneGame*>(scene_))
            {
                if (toggle_ && !releaseKey_) mouseRecaptureRequired = !mouseRecaptureRequired;
                // OSから戻った直後は、ゲーム領域をクリックするまでカーソルを自由にする。
                if (mouseRecaptureRequired && !releaseKey_ && KeyInput::KeyPressedIn(VK_LBUTTON))
                {
                    POINT position_{};
                    RECT client_{};
                    if (GetCursorPos(&position_) && ScreenToClient(hwnd, &position_) &&
                        GetClientRect(hwnd, &client_) && PtInRect(&client_, position_))
                    {
                        mouseRecaptureRequired = false;
                    }
                }
                capture_ = !mouseRecaptureRequired;
            }
        }
        else if (editorPlay_)
        {
            manager_->SetPlayInputCaptured(false);
        }
        mouse_.SetCursorLockEnabled(capture_);
        mouse_.Update(hwnd);
		ControllerInput::Instance().Update();
		KeyConfig::Instance().UpdateActiveDevice(GetForegroundWindow() == hwnd);
	}

	{
		BS_PROFILE_SCOPE("SceneManager::Update");
        // 前フレームの被覆通知を、Object更新やSceneの破棄に入る前に処理する。
        const auto scene_ = dynamic_cast<SceneBase*>(SceneManager::GetCurrentScene());
        if (scene_) scene_->ActivateContext();
        const auto manager_ = scene_ ? scene_->GetGameObjectManager() : nullptr;
        if (!manager_ || !manager_->IsCinematicSequencePreview())
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
            const auto scene_ = dynamic_cast<SceneBase*>(SceneManager::GetCurrentScene());
            const auto manager_ = scene_ ? scene_->GetGameObjectManager() : nullptr;
            if (!manager_ || !manager_->IsCinematicSequencePreview())
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
    auto& mouse_ = MouseInput::Instance();
    if (msg_ == WM_INPUT)
    {
        mouse_.OnRawMouseInput(reinterpret_cast<HRAWINPUT>(lParam_));
        return DefWindowProc(hWnd_, msg_, wParam_, lParam_);
    }
    // フォーカス喪失はImGuiの処理より先に扱い、同じフレーム内で戻っても自動再捕捉しない。
    if (msg_ == WM_KILLFOCUS || (msg_ == WM_ACTIVATEAPP && !wParam_) ||
        (msg_ == WM_ACTIVATE && LOWORD(wParam_) == WA_INACTIVE) ||
        (msg_ == WM_SIZE && wParam_ == SIZE_MINIMIZED) ||
        ((msg_ == WM_KEYDOWN || msg_ == WM_SYSKEYDOWN) && (wParam_ == VK_LWIN || wParam_ == VK_RWIN)) ||
        msg_ == WM_ENTERSIZEMOVE || msg_ == WM_ENTERMENULOOP || msg_ == WM_DESTROY)
    {
        if (mouse_.IsCursorLockEnabled()) mouseRecaptureRequired = true;
        mouse_.SetCursorLockEnabled(false);
    }
    if (msg_ == WM_SETCURSOR && LOWORD(lParam_) == HTCLIENT &&
        GetForegroundWindow() == hWnd_ && mouse_.IsCursorLocked())
    {
        SetCursor(nullptr);
        return TRUE;
    }

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

    case WM_PAINT:
    {
        PAINTSTRUCT ps_{};
        BeginPaint(hWnd_, &ps_);
        EndPaint(hWnd_, &ps_);
        return 0;
    }

    case WM_DESTROY:
    {
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
    MouseInput::Instance().SetCursorLockEnabled(false);
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


