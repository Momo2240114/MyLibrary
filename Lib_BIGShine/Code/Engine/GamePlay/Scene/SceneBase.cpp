#include "PrecompiledHeader.h"

#include"SceneBase.h"

#include"Engine/Graphics/Core/GraphicsDeviceManager.h"
#include"Engine/GamePlay/Camera/CameraComponent.h"

#include"Engine/Physics/BoxCollider.h"
#include"Engine/Physics/CapsuleCollider.h"
#include"Engine/GamePlay/Serialize/ActionRegistry.h"
#include"Engine/GamePlay/Input/KeyInput.h"
#include "Engine/Audio/AudioManager.h"
#include "Editor/Window/HierarchyWindow.h"
#include "Editor/Window/InspectorWindow.h"
#include "Editor/Window/ShaderWindow.h"
#include "Editor/Window/GpuRenderInvestigationWindow.h"
#include "Editor/Window/AnimationEditorWindow.h"
#include "Editor/Window/AttackTemplateEditorWindow.h"
#include "Editor/Window/AssetBrowserWindow.h"
#include "Editor/Window/CurveAssetEditorWindow.h"
#include "Editor/Window/EasingPresetEditorWindow.h"
#include "Editor/Window/VFXMaterialEditorWindow.h"
#include "Editor/Window/VFXMaterialPreviewWindow.h"
#include "Editor/Window/ActionSequenceEditorWindow.h"
#include "Editor/Window/EffectAssetListWindow.h"
#include "Editor/Window/MaterialAssetListWindow.h"
#include "Editor/Window/ShadowWindow.h"
#include "Editor/Window/PostEffectWindow.h"
#include "Editor/Window/DebugTextureViewerWindow.h"
#include "Editor/Window/InputEditorWindow.h"
#include "Editor/Window/InputMonitorWindow.h"
#include "Editor/Window/ComponentUpdateSequencerWindow.h"
#include "Editor/Window/CollisionShapeManagerWindow.h"
#include "Editor/Window/StartupResourceEditorWindow.h"
#include "Editor/Window/TimeManagerWindow.h"
#include "Editor/Window/GameModeWindow.h"
#include "Editor/Window/DebugLogWindow.h"
#include "Editor/Window/EditorSceneViewWindow.h"
#include "Editor/NodeEditor/AnimNodeEditorWindow.h"
#include "Editor/NodeEditor/NodeGraphMonitorWindow.h"
#include "Editor/Workspace/ParticleEditorWindow.h"
#include "Editor/Workspace/PlayerStateEditorWindow.h"
#include "Editor/Workspace/AudioWorkspaceWindow.h"
#include "Editor/Workspace/AIBehaviorEditorWindow.h"
#include "Editor/Workspace/UIWidgetEditorWindow.h"
#include "Editor/Workspace/PrefabWorkspaceWindow.h"
#include <imgui.h>

std::shared_ptr<GameObjectManager> SceneBase::GetGameObjectManager()
{ 
	return _GameObjectManager;
}

void SceneBase::Finalize()
{
	// RenderSystem と ActionRegistry を先に解放し、Scene 内参照を次シーンへ持ち越さない。
	m_SpriteRenderSystem.Finalize();
	m_ShapeRenderSystem.Finalize();
	actionRegistry.reset();


	if (_GameObjectManager) {
		_GameObjectManager->Clear();
		_GameObjectManager.reset(); //メモリ解放
	}
}

void SceneBase::Initialize()
{
	// シーン独自のマネージャーを生成する
	_GameObjectManager = std::make_shared<GameObjectManager>();
	_GameObjectManager->Initialize(); // マネージャーの初期化
	_GameObjectManager->SetModelRenderSystem(&m_ModelRenderSystem);

	editorWindowManager = std::make_unique<EditorWindowManager>(_GameObjectManager.get());

	// EditorWindow は実ファイル階層ではなく、用途別の UI としてここで一括登録する。
	editorWindowManager->AddWindow(std::make_unique<HierarchyWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<InspectorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<ShaderWindow>("",_GameObjectManager.get(),m_ModelRenderSystem));
	editorWindowManager->AddWindow(std::make_unique<GpuRenderInvestigationWindow>(_GameObjectManager.get(), m_ModelRenderSystem));
	editorWindowManager->AddWindow(std::make_unique<ShadowWindow>("",_GameObjectManager.get(),m_ModelRenderSystem));
	editorWindowManager->AddWindow(std::make_unique<PostEffectWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<DebugTextureViewerWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<InputEditorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<InputMonitorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<ComponentUpdateSequencerWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<CollisionShapeManagerWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<StartupResourceEditorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<TimeManagerWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<GameModeWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<DebugLogWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<EditorSceneViewWindow>(
		_GameObjectManager.get(),
		&m_SpriteRenderSystem,
		&m_ModelRenderSystem));
	editorWindowManager->AddWindow(std::make_unique<ActionSequenceEditorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<EffectAssetListWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<MaterialAssetListWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<AnimationEditorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<AttackTemplateEditorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<AnimNodeEditorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<NodeGraphMonitorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<ParticleEditorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<PlayerStateEditorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<AudioWorkspaceWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<AIBehaviorEditorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<UIWidgetEditorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<PrefabWorkspaceWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<AssetBrowserWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<CurveAssetEditorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<EasingPresetEditorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<VFXMaterialEditorWindow>(_GameObjectManager.get()));
	editorWindowManager->AddWindow(std::make_unique<VFXMaterialPreviewWindow>(_GameObjectManager.get()));

	actionRegistry = std::make_shared<ActionRegistry>();
	ActionRegistry::SetInstance(actionRegistry);

	// Scene 内で使う描画システムを D3D11Device から初期化し、RenderAll で共有する。
	m_ModelRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice());
	m_SpriteRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice(), GraphicsDeviceManager::Instance().GetContext());
	m_ShapeRenderSystem.Initialize(GraphicsDeviceManager::Instance().GetDevice());
}

void SceneBase::Update(float elapsedTime)
{
	if (KeyInput::KeyPressedIn(KeyInput::Key::K_F1))
	{
		// F1 でエディタ表示を切り替える。非表示時も Scene の更新は通常通り進む。
		isEditorVisible = !isEditorVisible;
	}

	if (editorWindowManager)
	{
		editorWindowManager->Update(elapsedTime);
	}

	if (isEditorVisible && editorWindowManager && editorWindowManager->ShouldPauseScene())
	{
		// エディタが Pause を要求している間は GameObject の Update だけ止める。
		return;
	}

	_GameObjectManager->Update(elapsedTime);
}

void SceneBase::Render(float elapsedTime)
{
	if (isEditorVisible &&
		editorWindowManager &&
		editorWindowManager->ShouldPauseScene() &&
		!editorWindowManager->ShouldRenderSceneWhenPaused())
	{
		// Pause 中に Scene 描画もしない設定なら、ImGui 用にバックバッファだけ戻しておく。
		GraphicsDeviceManager::Instance().SetBackBufferRenderTarget();
		return;
	}

	// Docked SceneView 使用時は最終出力先を専用 RenderTarget に差し替える。
    const bool useDockedSceneView_ = isEditorVisible &&
        editorWindowManager &&
        editorWindowManager->ShouldUseDockedSceneView();
    ID3D11RenderTargetView* finalRenderTargetView_ = useDockedSceneView_
        ? GraphicsDeviceManager::Instance().GetSceneViewRenderTargetView()
        : nullptr;
    _GameObjectManager->RenderAll(m_SpriteRenderSystem, m_ModelRenderSystem, finalRenderTargetView_);

    if (useDockedSceneView_)
    {
		// SceneView 描画後は ImGui を通常のバックバッファへ描けるよう戻す。
        GraphicsDeviceManager::Instance().SetBackBufferRenderTarget();
    }

}

void SceneBase::DrawGUI()
{
	if (_GameObjectManager)
	{
		const SCENE_RUNTIME_STATE sceneRuntimeState_ = _GameObjectManager->GetSceneRuntimeState();
		if (sceneRuntimeState_ == SCENE_RUNTIME_STATE::LOADING ||
			sceneRuntimeState_ == SCENE_RUNTIME_STATE::INITIALIZING ||
			sceneRuntimeState_ == SCENE_RUNTIME_STATE::FAILED)
		{
			// 非同期シーンロード中は、EditorWindow とは独立した小さな状態表示を出す。
			ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_Always);
			const ImGuiWindowFlags windowFlags_ =
				ImGuiWindowFlags_NoDecoration |
				ImGuiWindowFlags_AlwaysAutoResize |
				ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoNav;

			if (ImGui::Begin("Scene Runtime Status", nullptr, windowFlags_))
			{
				if (sceneRuntimeState_ == SCENE_RUNTIME_STATE::FAILED)
				{
					ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.2f, 1.0f), "Scene load failed");
					ImGui::TextWrapped("%s", _GameObjectManager->GetSceneRuntimeError().c_str());
				}
				else
				{
					const float progress_ = _GameObjectManager->GetSceneLoadProgress() * 100.0f;
					ImGui::Text("Scene loading %.0f%%", progress_);
				}
			}
			ImGui::End();
		}
	}

	if(isEditorVisible)
	{
		editorWindowManager->DrawAll();
	}
}
