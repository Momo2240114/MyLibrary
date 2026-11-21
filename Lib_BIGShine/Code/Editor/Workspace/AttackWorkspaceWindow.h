#pragma once

#include "Editor/EditorCameraData.h"
#include "Editor/Utility/EditorPrimitiveRenderer.h"
#include "Editor/Workspace/EditorRenderTarget.h"
#include "Editor/Workspace/EditorWorkspaceWindow.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include "Engine/Graphics/RenderSystem/ParticleRenderSystem.h"
#include "Engine/Graphics/VFX/MeshVFXRenderSystem.h"

#include <memory>
#include <string>

class AttackEffectHitComponent;
class DangerAreaComponent;
class DragonAIComponent;
class DragonAttackControllerComponent;
class AttackEmitterComponent;
class GameObject;

// 攻撃判定、危険範囲、AttackEmitterを実シーン上の対象Objectへまとめて調整するWorkspace。
class AttackWorkspaceWindow : public EditorWorkspaceWindow
{
public:
    explicit AttackWorkspaceWindow(GameObjectManager* manager_);

    void Draw() override;
    void Update(float deltaTime_) override;

    bool ShouldPauseScene() const override
    {
        return true;
    }

    bool ShouldRenderSceneWhenPaused() const override
    {
        return false;
    }

private:
    void EnsurePreviewResources();
    void DrawToolbar();
    void DrawPreviewPanel();
    void DrawTargetListPanel();
    void DrawActionPanel();
    void DrawInspectorPanel();
    void DrawBottomPanel();
    void RenderPreview(const ImVec2& previewSize_);
    std::shared_ptr<GameObject> ResolveTarget() const;
    bool HasAttackWorkspaceComponents(const std::shared_ptr<GameObject>& object_) const;
    void SelectTarget(const std::shared_ptr<GameObject>& object_);
    void ExecuteTargetAction(const std::string& actionName_);
    void DrawDragonAIActionCombo(const std::shared_ptr<DragonAIComponent>& dragonAI_);

private:
    std::weak_ptr<GameObject> targetObject;
    std::string targetFilterText;
    EditorPrimitiveRenderer editorPrimitiveRenderer;
    EditorRenderTarget previewTarget;
    ModelRenderSystem previewModelRenderSystem;
    ParticleRenderSystem previewParticleRenderSystem;
    MeshVFXRenderSystem previewMeshVFXRenderSystem;
    CameraComponent previewCamera;
    EditorCameraData editorCamera;
    float inspectorPanelWidth = 420.0f;
    float bottomPanelHeight = 260.0f;
    bool showOnlyAttackObjects = true;
    bool autoSelectSceneSelection = true;
    bool isPreviewResourceInitialized = false;
    bool showGrid = true;
    int selectedDragonActionIndex = 4;
};
