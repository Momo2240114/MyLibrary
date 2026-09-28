#include "PrecompiledHeader.h"

#include "AttackWorkspaceWindow.h"

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Combat/AttackEffectHitComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/VFX/DangerAreaComponent.h"
#include "Engine/GamePlay/VFX/EffectEmitterComponent.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Core/LightManager.h"
#include "Engine/Graphics/Core/RenderContext.h"
#include "Engine/Graphics/Decal/DecalComponent.h"
#include "Engine/Graphics/Decal/DecalManager.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"
#include "Game/AI/Dragon/DragonAIComponent.h"
#include "Game/AI/Dragon/DragonAttackControllerComponent.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <imgui.h>
#include <imgui_stdlib.h>

namespace
{
    bool ContainsCaseInsensitive(
        const std::string& text_,
        const std::string& query_)
    {
        if (query_.empty()) return true;

        return std::search(
            text_.begin(),
            text_.end(),
            query_.begin(),
            query_.end(),
            [](unsigned char left_, unsigned char right_)
            {
                return std::tolower(left_) == std::tolower(right_);
            }) != text_.end();
    }

    struct AttackWorkspaceAction
    {
        const char* label = "";
        const char* actionName = "";
    };
}

AttackWorkspaceWindow::AttackWorkspaceWindow(GameObjectManager* manager_)
    : EditorWorkspaceWindow("Attack Workspace", manager_)
{
    isVisible = false;
}

void AttackWorkspaceWindow::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("AttackWorkspaceWindow::Update");

    if (!autoSelectSceneSelection || !gameObjectManager) return;

    const std::shared_ptr<GameObject> selectedObject_ = gameObjectManager->GetSelectedObject();
    if (HasAttackWorkspaceComponents(selectedObject_))
    {
        targetObject = selectedObject_;
    }
}

void AttackWorkspaceWindow::Draw()
{
    BS_PROFILE_SCOPE("AttackWorkspaceWindow::Draw");

    EnsurePreviewResources();

    if (!BeginWorkspaceWindow(ImGuiWindowFlags_MenuBar))
    {
        EndEditorWindow();
        return;
    }

    DrawToolbar();

    const float splitterSize_ = 6.0f;
    const float verticalSpacing_ = ImGui::GetStyle().ItemSpacing.y * 2.0f;
    const float minimumTopHeight_ = 220.0f;
    const float minimumBottomHeight_ = 180.0f;
    const float minimumInspectorWidth_ = 320.0f;
    const ImVec2 available_ = ImGui::GetContentRegionAvail();

    bottomPanelHeight = std::clamp(
        bottomPanelHeight,
        minimumBottomHeight_,
        (std::max)(
            minimumBottomHeight_,
            available_.y - minimumTopHeight_ - splitterSize_ - verticalSpacing_));
    inspectorPanelWidth = std::clamp(
        inspectorPanelWidth,
        minimumInspectorWidth_,
        (std::max)(minimumInspectorWidth_, available_.x - 360.0f));

    const float topHeight_ = (std::max)(
        minimumTopHeight_,
        available_.y - bottomPanelHeight - splitterSize_ - verticalSpacing_);
    const float previewWidth_ = (std::max)(
        280.0f,
        available_.x - inspectorPanelWidth - ImGui::GetStyle().ItemSpacing.x);

    ImGui::BeginChild("AttackWorkspacePreviewPanel", ImVec2(previewWidth_, topHeight_), true);
    DrawPreviewPanel();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::InvisibleButton("##AttackWorkspaceInspectorSplitter", ImVec2(splitterSize_, topHeight_));
    if (ImGui::IsItemActive()) inspectorPanelWidth -= ImGui::GetIO().MouseDelta.x;
    ImGui::SameLine();

    ImGui::BeginChild("AttackWorkspaceInspectorPanel", ImVec2(0.0f, topHeight_), true);
    DrawInspectorPanel();
    ImGui::EndChild();

    ImGui::InvisibleButton("##AttackWorkspaceHorizontalSplitter", ImVec2(-1.0f, splitterSize_));
    const bool horizontalHovered_ = ImGui::IsItemHovered();
    const bool horizontalActive_ = ImGui::IsItemActive();
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImGui::GetItemRectMin(),
        ImGui::GetItemRectMax(),
        ImGui::GetColorU32(
            horizontalHovered_ || horizontalActive_
            ? ImGuiCol_SeparatorHovered
            : ImGuiCol_Separator));
    if (ImGui::IsItemActive()) bottomPanelHeight -= ImGui::GetIO().MouseDelta.y;

    ImGui::BeginChild("AttackWorkspaceBottomPanel", ImVec2(0.0f, 0.0f), true);
    DrawBottomPanel();
    ImGui::EndChild();

    EndEditorWindow();
}

void AttackWorkspaceWindow::EnsurePreviewResources()
{
    BS_PROFILE_SCOPE("AttackWorkspaceWindow::EnsurePreviewResources");

    if (isPreviewResourceInitialized) return;

    ID3D11Device* device_ = GraphicsDeviceManager::Instance().GetDevice();
    if (!device_) return;

    // Attack Workspaceは実Sceneを停止し、専用RTへ現在の攻撃調整対象を描画する。
    editorPrimitiveRenderer.Initialize(device_);
    previewModelRenderSystem.Initialize(device_);
    previewParticleRenderSystem.Initialize(device_);
    previewMeshVFXRenderSystem.Initialize(device_);
    editorCamera.ResetPreviewCamera(previewCamera);
    previewCamera.SetPerspectiveFov(45.0f, 16.0f / 9.0f, 0.01f, 1000.0f);

    isPreviewResourceInitialized = true;
}

void AttackWorkspaceWindow::DrawToolbar()
{
    BS_PROFILE_SCOPE("AttackWorkspaceWindow::DrawToolbar");

    if (!ImGui::BeginMenuBar()) return;

    ImGui::MenuItem("Attack Objects Only", nullptr, &showOnlyAttackObjects);
    ImGui::SameLine();
    ImGui::MenuItem("Follow Scene Selection", nullptr, &autoSelectSceneSelection);
    ImGui::SameLine();
    ImGui::MenuItem("Grid", nullptr, &showGrid);
    ImGui::SameLine();
    ImGui::MenuItem("Free Camera", nullptr, &editorCamera.enablePreviewFreeCamera);
    ImGui::SameLine();
    if (ImGui::MenuItem("Reset Camera"))
    {
        editorCamera.ResetPreviewCamera(previewCamera);
    }
    ImGui::SameLine();
    if (ImGui::MenuItem("Use Scene Selection"))
    {
        SelectTarget(gameObjectManager ? gameObjectManager->GetSelectedObject() : nullptr);
    }

    ImGui::EndMenuBar();
}

void AttackWorkspaceWindow::DrawPreviewPanel()
{
    BS_PROFILE_SCOPE("AttackWorkspaceWindow::DrawPreviewPanel");

    ImGui::TextUnformatted("Attack Preview");
    ImGui::Separator();

    ImVec2 previewSize_ = ImGui::GetContentRegionAvail();
    previewSize_.x = (std::max)(1.0f, previewSize_.x);
    previewSize_.y = (std::max)(1.0f, previewSize_.y);

    const bool previewHovered_ =
        ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    editorCamera.UpdatePreviewFreeCamera(
        previewCamera,
        ImGui::GetIO().DeltaTime,
        previewHovered_);

    RenderPreview(previewSize_);

    if (previewTarget.GetShaderResourceView())
    {
        ImGui::Image(reinterpret_cast<void*>(previewTarget.GetShaderResourceView()), previewSize_);
    }
    else
    {
        ImGui::Button("Preview Not Ready", previewSize_);
    }

    if (previewHovered_)
    {
        ImGui::SetTooltip("RMB + Mouse: Look / WASD: Move / Q,E: Up Down / Wheel: Speed");
    }
}

void AttackWorkspaceWindow::DrawTargetListPanel()
{
    BS_PROFILE_SCOPE("AttackWorkspaceWindow::DrawTargetListPanel");

    ImGui::TextUnformatted("Targets");
    ImGui::Separator();
    ImGui::InputText("Filter", &targetFilterText);

    if (!gameObjectManager)
    {
        ImGui::TextDisabled("GameObjectManager is not ready.");
        return;
    }

    const std::shared_ptr<GameObject> currentTarget_ = ResolveTarget();
    for (const std::shared_ptr<GameObject>& object_ : gameObjectManager->GetObjects())
    {
        if (!object_) continue;
        if (showOnlyAttackObjects && !HasAttackWorkspaceComponents(object_)) continue;
        if (!ContainsCaseInsensitive(object_->GetName(), targetFilterText)) continue;

        const bool selected_ = object_ == currentTarget_;
        if (ImGui::Selectable(object_->GetName().c_str(), selected_))
        {
            SelectTarget(object_);
        }
    }
}

void AttackWorkspaceWindow::DrawActionPanel()
{
    BS_PROFILE_SCOPE("AttackWorkspaceWindow::DrawActionPanel");

    const std::shared_ptr<GameObject> target_ = ResolveTarget();
    ImGui::TextUnformatted("Live Test");
    ImGui::Separator();
    if (!target_)
    {
        ImGui::TextDisabled("Select an attack target object.");
        return;
    }

    ImGui::Text("Target: %s", target_->GetName().c_str());

    if (std::shared_ptr<DragonAIComponent> dragonAI_ = target_->GetComponent<DragonAIComponent>())
    {
        DrawDragonAIActionCombo(dragonAI_);
    }

    if (target_->GetComponent<DragonAttackControllerComponent>())
    {
        ImGui::SeparatorText("Dragon Attack Actions");
        constexpr std::array<AttackWorkspaceAction, 12> ACTIONS =
        {
            AttackWorkspaceAction{ "Show Breath Danger", "DragonAttack_ShowBreathDangerArea" },
            AttackWorkspaceAction{ "Breath Single", "DragonAttack_PlayBreathProjectileSingle" },
            AttackWorkspaceAction{ "Breath Beam", "DragonAttack_PlayBreathProjectileBeam" },
            AttackWorkspaceAction{ "Breath Fan", "DragonAttack_PlayBreathProjectileFan" },
            AttackWorkspaceAction{ "Bite", "DragonAttack_PlayBite" },
            AttackWorkspaceAction{ "Show Bite Danger", "DragonAttack_ShowBiteDangerArea" },
            AttackWorkspaceAction{ "Tail Sweep", "DragonAttack_PlayTailSweep" },
            AttackWorkspaceAction{ "Show Tail Danger", "DragonAttack_ShowTailSweepDangerArea" },
            AttackWorkspaceAction{ "Spin Sweep", "DragonAttack_PlaySpinSweep" },
            AttackWorkspaceAction{ "Show Spin Danger", "DragonAttack_ShowSpinSweepDangerArea" },
            AttackWorkspaceAction{ "Stamp", "DragonAttack_PlayStamp" },
            AttackWorkspaceAction{ "Stop Attack", "DragonAttack_Stop" },
        };

        for (size_t index_ = 0; index_ < ACTIONS.size(); ++index_)
        {
            if (ImGui::Button(ACTIONS[index_].label, ImVec2(180.0f, 0.0f)))
            {
                ExecuteTargetAction(ACTIONS[index_].actionName);
            }
            if (index_ % 2 == 0) ImGui::SameLine();
        }
    }

    if (std::shared_ptr<EffectEmitterComponent> emitter_ = target_->GetComponent<EffectEmitterComponent>())
    {
        ImGui::SeparatorText("EffectEmitter Templates");
        const std::vector<EffectEmitterTemplate>& templates_ = emitter_->GetTemplates();
        for (const EffectEmitterTemplate& template_ : templates_)
        {
            const std::string label_ = template_.name.empty() ? template_.actionName : template_.name;
            if (ImGui::Button(label_.c_str(), ImVec2(180.0f, 0.0f)))
            {
                emitter_->EmitByName(label_);
            }
        }
    }

    if (std::shared_ptr<DangerAreaComponent> dangerArea_ = target_->GetComponent<DangerAreaComponent>())
    {
        ImGui::SeparatorText("DangerArea Templates");
        const std::vector<DangerAreaTemplate>& templates_ = dangerArea_->GetTemplates();
        for (const DangerAreaTemplate& template_ : templates_)
        {
            const std::string label_ = template_.name.empty() ? template_.actionName : template_.name;
            if (ImGui::Button(label_.c_str(), ImVec2(180.0f, 0.0f)))
            {
                dangerArea_->ShowByName(label_);
            }
        }
    }
}

void AttackWorkspaceWindow::DrawInspectorPanel()
{
    BS_PROFILE_SCOPE("AttackWorkspaceWindow::DrawInspectorPanel");

    const std::shared_ptr<GameObject> target_ = ResolveTarget();
    ImGui::TextUnformatted("Attack Inspector");
    ImGui::Separator();
    if (!target_)
    {
        ImGui::TextDisabled("Select an attack target object.");
        return;
    }

    if (ImGui::BeginTabBar("AttackWorkspaceInspectorTabs"))
    {
        if (std::shared_ptr<DragonAttackControllerComponent> dragonAttack_ =
            target_->GetComponent<DragonAttackControllerComponent>())
        {
            if (ImGui::BeginTabItem("Dragon Attack"))
            {
                dragonAttack_->Gui();
                ImGui::EndTabItem();
            }
        }
        if (std::shared_ptr<DragonAIComponent> dragonAI_ = target_->GetComponent<DragonAIComponent>())
        {
            if (ImGui::BeginTabItem("Dragon AI"))
            {
                dragonAI_->Gui();
                ImGui::EndTabItem();
            }
        }
        if (std::shared_ptr<EffectEmitterComponent> emitter_ = target_->GetComponent<EffectEmitterComponent>())
        {
            if (ImGui::BeginTabItem("EffectEmitter"))
            {
                emitter_->Gui();
                ImGui::EndTabItem();
            }
        }
        if (std::shared_ptr<AttackEffectHitComponent> attackHit_ = target_->GetComponent<AttackEffectHitComponent>())
        {
            if (ImGui::BeginTabItem("Hit"))
            {
                attackHit_->Gui();
                ImGui::EndTabItem();
            }
        }
        if (std::shared_ptr<DangerAreaComponent> dangerArea_ = target_->GetComponent<DangerAreaComponent>())
        {
            if (ImGui::BeginTabItem("Danger"))
            {
                dangerArea_->Gui();
                ImGui::EndTabItem();
            }
        }
        if (ImGui::BeginTabItem("Object"))
        {
            target_->Gui();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Camera"))
        {
            editorCamera.Gui();
            if (ImGui::Button("Reset Camera"))
            {
                editorCamera.ResetPreviewCamera(previewCamera);
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void AttackWorkspaceWindow::DrawBottomPanel()
{
    BS_PROFILE_SCOPE("AttackWorkspaceWindow::DrawBottomPanel");

    const float targetPanelWidth_ = 320.0f;
    ImGui::BeginChild("AttackWorkspaceBottomTargets", ImVec2(targetPanelWidth_, 0.0f), true);
    DrawTargetListPanel();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("AttackWorkspaceBottomActions", ImVec2(0.0f, 0.0f), true);
    DrawActionPanel();
    ImGui::EndChild();
}

void AttackWorkspaceWindow::RenderPreview(const ImVec2& previewSize_)
{
    BS_PROFILE_SCOPE("AttackWorkspaceWindow::RenderPreview");

    if (!gameObjectManager) return;

    auto& graphics_ = GraphicsDeviceManager::Instance();
    ID3D11Device* device_ = graphics_.GetDevice();
    ID3D11DeviceContext* context_ = graphics_.GetContext();
    if (!device_ || !context_) return;

    const int width_ = (std::max)(1, static_cast<int>(previewSize_.x));
    const int height_ = (std::max)(1, static_cast<int>(previewSize_.y));
    if (!previewTarget.Resize(device_, width_, height_)) return;

    previewCamera.SetPerspectiveFov(
        45.0f,
        static_cast<float>(width_) / static_cast<float>(height_),
        0.01f,
        1000.0f);

    const float clearColor_[4] = { 0.035f, 0.038f, 0.042f, 1.0f };
    previewTarget.Begin(context_, clearColor_);

    LightManager lightManager_;
    DirectionalLight directionalLight_;
    directionalLight_.direction = { 0.35f, -0.8f, 0.45f };
    directionalLight_.color = { 1.0f, 0.97f, 0.92f };
    directionalLight_.intensity = 5.0f;
    lightManager_.SetDirectionalLight(directionalLight_);

    DecalManager decalManager_;

    RenderContext renderContext_{};
    renderContext_.device = device_;
    renderContext_.deviceContext = context_;
    renderContext_.renderState = graphics_.GetRenderState();
    renderContext_.camera = &previewCamera;
    renderContext_.lights = &lightManager_;
    renderContext_.decal = &decalManager_;
    renderContext_.depthSRV = previewTarget.GetDepthShaderResourceView();
    renderContext_.baseColorSRV = previewTarget.GetShaderResourceView();
    renderContext_.shapeSystem = &graphics_.GetShapeRendererSystem();
    renderContext_.cascadeShadowMap = nullptr;
    renderContext_.screenWidth = static_cast<float>(width_);
    renderContext_.screenHeight = static_cast<float>(height_);

    context_->OMSetBlendState(renderContext_.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    context_->OMSetDepthStencilState(renderContext_.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
    context_->RSSetState(renderContext_.renderState->GetRasterizerState(RasterizerState::SolidCullBack));

    if (showGrid)
    {
        // 攻撃範囲の大きさを地面グリッドと比較できるよう、Sceneとは独立して描画する。
        editorPrimitiveRenderer.DrawGrid(
            30,
            1.0f,
            { 0.25f, 0.25f, 0.25f, 1.0f },
            true);
        editorPrimitiveRenderer.Render(
            context_,
            previewCamera.GetView(),
            previewCamera.GetProjection());
        previewTarget.ResetRenderTarget(context_);
    }

    for (const std::shared_ptr<GameObject>& object_ : gameObjectManager->GetObjects())
    {
        if (!object_) continue;

        // 停止中のSceneでもInspector変更後の位置関係をPreviewへ反映する。
        object_->UpdateTransform();

        if (const std::shared_ptr<GltfModelCommponennt> model_ = object_->GetComponent<GltfModelCommponennt>())
        {
            previewModelRenderSystem.PushSubmit(model_->GetShaderID(), model_);
        }

        if (const std::shared_ptr<ParticleComponent> particle_ = object_->GetComponent<ParticleComponent>())
        {
            if (particle_->IsEnabled())
            {
                previewParticleRenderSystem.PushSubmit(particle_.get());
            }
        }

        if (const std::shared_ptr<DecalComponent> decal_ = object_->GetComponent<DecalComponent>())
        {
            if (decal_->IsEnabled())
            {
                decal_->SubmitDecal(decalManager_);
            }
        }

        for (const std::shared_ptr<MeshVFXComponent>& meshVFX_ : object_->GetComponents<MeshVFXComponent>())
        {
            if (meshVFX_ && meshVFX_->IsEnabled())
            {
                previewMeshVFXRenderSystem.PushSubmit(meshVFX_.get());
            }
        }
    }

    previewModelRenderSystem.RenderAll(renderContext_);
    previewTarget.ResetRenderTarget(context_);
    previewMeshVFXRenderSystem.RenderAll(
        renderContext_,
        true,
        previewTarget.GetRenderTargetView(),
        previewTarget.GetDepthStencilView());
    previewTarget.ResetRenderTarget(context_);
    context_->OMSetDepthStencilState(
        renderContext_.renderState->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
    graphics_.GetShapeRendererSystem().Render(
        context_,
        renderContext_.camera->GetView(),
        renderContext_.camera->GetProjection());
    context_->OMSetDepthStencilState(
        renderContext_.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
    previewParticleRenderSystem.RenderAll(renderContext_);

    previewTarget.End(context_);
    graphics_.SetBackBufferRenderTarget();
}

std::shared_ptr<GameObject> AttackWorkspaceWindow::ResolveTarget() const
{
    std::shared_ptr<GameObject> target_ = targetObject.lock();
    if (target_) return target_;
    return gameObjectManager ? gameObjectManager->GetSelectedObject() : nullptr;
}

bool AttackWorkspaceWindow::HasAttackWorkspaceComponents(
    const std::shared_ptr<GameObject>& object_) const
{
    if (!object_) return false;

    return object_->GetComponent<DragonAIComponent>() ||
        object_->GetComponent<DragonAttackControllerComponent>() ||
        object_->GetComponent<EffectEmitterComponent>() ||
        object_->GetComponent<AttackEffectHitComponent>() ||
        object_->GetComponent<DangerAreaComponent>();
}

void AttackWorkspaceWindow::SelectTarget(const std::shared_ptr<GameObject>& object_)
{
    targetObject = object_;
    if (gameObjectManager && object_)
    {
        gameObjectManager->SetSelectedObject(object_);
    }
}

void AttackWorkspaceWindow::ExecuteTargetAction(const std::string& actionName_)
{
    const std::shared_ptr<GameObject> target_ = ResolveTarget();
    if (!target_) return;

    // Workspaceからのテスト発火はGameObjectのAction探索経路へ通し、Sequencerと同じ呼び出し条件に揃える。
    target_->ExecuteAction(actionName_);
}

void AttackWorkspaceWindow::DrawDragonAIActionCombo(
    const std::shared_ptr<DragonAIComponent>& dragonAI_)
{
    BS_PROFILE_SCOPE("AttackWorkspaceWindow::DrawDragonAIActionCombo");

    if (!dragonAI_) return;

    ImGui::SeparatorText("Dragon AI");
    constexpr int DRAGON_ACTION_COUNT_ =
        static_cast<int>(DRAGON_ACTION_TYPE::ULTIMATE_ATTACK) + 1;
    selectedDragonActionIndex = std::clamp(
        selectedDragonActionIndex,
        0,
        DRAGON_ACTION_COUNT_ - 1);
    const DRAGON_ACTION_TYPE selectedAction_ =
        static_cast<DRAGON_ACTION_TYPE>(selectedDragonActionIndex);

    ImGui::SetNextItemWidth(240.0f);
    if (ImGui::BeginCombo("AI Action", GetDragonActionName(selectedAction_)))
    {
        for (int actionIndex_ = 0; actionIndex_ < DRAGON_ACTION_COUNT_; ++actionIndex_)
        {
            const DRAGON_ACTION_TYPE action_ =
                static_cast<DRAGON_ACTION_TYPE>(actionIndex_);
            const bool selected_ = selectedDragonActionIndex == actionIndex_;
            if (ImGui::Selectable(GetDragonActionName(action_), selected_))
            {
                selectedDragonActionIndex = actionIndex_;
            }
            if (selected_) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    const DRAGON_ACTION_TYPE action_ =
        static_cast<DRAGON_ACTION_TYPE>(selectedDragonActionIndex);
    if (ImGui::Button("Request AI Action", ImVec2(180.0f, 0.0f)))
    {
        dragonAI_->RequestAction(action_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Force AI Action", ImVec2(180.0f, 0.0f)))
    {
        dragonAI_->ForceAction(action_);
    }
}
