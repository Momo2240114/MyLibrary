#pragma once

#include "EditorWorkspaceWindow.h"
#include "EditorRenderTarget.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include "Engine/Graphics/RenderSystem/ParticleRenderSystem.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"
#include "Engine/Graphics/VFX/MeshVFXRenderSystem.h"

#include <memory>
#include <string>
#include <vector>

class ParticleEditorWindow : public EditorWorkspaceWindow
{
public:
    explicit ParticleEditorWindow(GameObjectManager* manager_);
    void Draw() override;
    bool ShouldPauseScene() const override { return true; }

private:
    enum class TimelineTrackType
    {
        PARTICLE_EMITTER,
        MESH_VFX,
    };

    struct TimelineTrack
    {
        TimelineTrackType type = TimelineTrackType::PARTICLE_EMITTER;
        std::string targetName;
        float startTime = 0.0f;
        float duration = 1.0f;
        bool active = false;
    };

    void EnsurePreviewObjects();
    void DrawToolbar();
    void DrawPreviewPanel();
    void DrawTimelinePanel();
    void DrawAssetBrowserPanel();
    void DrawInspectorPanel();
    void RenderPreview(const ImVec2& previewSize_);
    void RefreshParticleAssets();
    void RefreshSequenceAssets();
    void UpdateSequence(float deltaTime_);
    void ResetSequence();
    bool SaveSequence(const std::string& path_);
    bool LoadSequence(const std::string& path_);

private:
    EditorRenderTarget previewTarget;
    ModelRenderSystem modelRenderSystem;
    ParticleRenderSystem particleRenderSystem;
    MeshVFXRenderSystem meshVFXRenderSystem;
    CameraComponent previewCamera;
    std::shared_ptr<GameObject> previewObject;
    std::shared_ptr<GltfModelCommponennt> previewModel;
    std::shared_ptr<ParticleComponent> previewParticle;
    std::shared_ptr<MeshVFXComponent> previewMeshVFX;
    std::vector<std::string> particleAssetPaths;
    std::vector<std::string> sequenceAssetPaths;
    std::vector<TimelineTrack> timelineTracks;
    int selectedAssetIndex = -1;
    int selectedSequenceIndex = -1;
    int selectedTimelineTrack = -1;
    char sequenceNameBuffer[128] = "NewVFXSequence";
    float sequenceTime = 0.0f;
    float sequenceDuration = 5.0f;
    float inspectorWidth = 360.0f;
    float timelineHeight = 190.0f;
    bool isInitialized = false;
    bool sequencePlaying = false;
    bool sequenceLoop = true;
    bool showGrid = true;
};
