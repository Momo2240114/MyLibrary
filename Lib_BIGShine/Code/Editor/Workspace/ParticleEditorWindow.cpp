
#include "ParticleEditorWindow.h"

#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Core/RenderContext.h"
#include "ImSequencer.h"
#include "imgui_internal.h"

#include <algorithm>
#include <DirectXMath.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace
{
    constexpr float TIMELINE_FPS = 60.0f;
}

ParticleEditorWindow::ParticleEditorWindow(GameObjectManager* manager_)
    : EditorWorkspaceWindow("VFX Editor", manager_),
    assetBrowser(manager_)
{
    isVisible = false;
    style.backgroundColor = ImVec4(0.08f, 0.09f, 0.10f, 1.0f);
    style.titleBarColor = ImVec4(0.10f, 0.34f, 0.18f, 1.0f);
}

void ParticleEditorWindow::Draw()
{
    EnsurePreviewObjects();

    if (!BeginWorkspaceWindow(ImGuiWindowFlags_MenuBar))
    {
        EndEditorWindow();
        return;
    }

    if (ImGui::BeginMenuBar())
    {
        DrawToolbar();
        ImGui::EndMenuBar();
    }

    const float splitterSize_ = 6.0f;
    const float minimumLeftWidth_ = 280.0f;
    const float minimumInspectorWidth_ = 240.0f;
    const float minimumPreviewHeight_ = 160.0f;
    const float minimumTimelineHeight_ = 120.0f;
    const float verticalSpacing_ = ImGui::GetStyle().ItemSpacing.y * 2.0f;
    ImVec2 available_ = ImGui::GetContentRegionAvail();
    inspectorWidth = std::clamp(
        inspectorWidth,
        minimumInspectorWidth_,
        (std::max)(minimumInspectorWidth_, available_.x - minimumLeftWidth_ - splitterSize_));
    timelineHeight = std::clamp(
        timelineHeight,
        minimumTimelineHeight_,
        (std::max)(
            minimumTimelineHeight_,
            available_.y - minimumPreviewHeight_ - splitterSize_ - verticalSpacing_));

    const float leftWidth_ = (std::max)(minimumLeftWidth_, available_.x - inspectorWidth - splitterSize_);
    const float previewHeight_ =
        (std::max)(
            minimumPreviewHeight_,
            available_.y - timelineHeight - splitterSize_ - verticalSpacing_);

    ImGui::BeginGroup();
    ImGui::BeginChild("ParticlePreviewPanel", ImVec2(leftWidth_, previewHeight_), true);
    DrawPreviewPanel();
    ImGui::EndChild();

    ImGui::InvisibleButton("##VFXHorizontalSplitter", ImVec2(leftWidth_, splitterSize_));
    const bool isHorizontalSplitterHovered_ = ImGui::IsItemHovered();
    const bool isHorizontalSplitterActive_ = ImGui::IsItemActive();
    const ImU32 horizontalSplitterColor_ = ImGui::GetColorU32(
        isHorizontalSplitterHovered_ || isHorizontalSplitterActive_
        ? ImGuiCol_SeparatorHovered
        : ImGuiCol_Separator);
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImGui::GetItemRectMin(),
        ImGui::GetItemRectMax(),
        horizontalSplitterColor_);
    if (isHorizontalSplitterHovered_ || isHorizontalSplitterActive_)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    if (isHorizontalSplitterActive_)
    {
        timelineHeight -= ImGui::GetIO().MouseDelta.y;
    }

    ImGui::BeginChild("VFXTimelinePanel", ImVec2(leftWidth_, timelineHeight), true);
    DrawTimelinePanel();
    ImGui::EndChild();
    ImGui::EndGroup();

    ImGui::SameLine(0.0f, 0.0f);

    ImGui::InvisibleButton("##VFXVerticalSplitter", ImVec2(splitterSize_, available_.y));
    const bool isVerticalSplitterHovered_ = ImGui::IsItemHovered();
    const bool isVerticalSplitterActive_ = ImGui::IsItemActive();
    const ImU32 verticalSplitterColor_ = ImGui::GetColorU32(
        isVerticalSplitterHovered_ || isVerticalSplitterActive_
        ? ImGuiCol_SeparatorHovered
        : ImGuiCol_Separator);
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImGui::GetItemRectMin(),
        ImGui::GetItemRectMax(),
        verticalSplitterColor_);
    if (isVerticalSplitterHovered_ || isVerticalSplitterActive_)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    if (isVerticalSplitterActive_)
    {
        inspectorWidth -= ImGui::GetIO().MouseDelta.x;
    }

    ImGui::SameLine(0.0f, 0.0f);

    ImGui::BeginChild("ParticleInspectorPanel", ImVec2(inspectorWidth, available_.y), true);
    DrawInspectorPanel();
    ImGui::EndChild();

    EndEditorWindow();
}

void ParticleEditorWindow::EnsurePreviewObjects()
{
    if (isInitialized) return;

    auto& graphics_ = GraphicsDeviceManager::Instance();
    modelRenderSystem.Initialize(graphics_.GetDevice());
    particleRenderSystem.Initialize(graphics_.GetDevice());
    meshVFXRenderSystem.Initialize(graphics_.GetDevice());
    editorPrimitiveRenderer.Initialize(graphics_.GetDevice());

    previewObject = std::make_shared<GameObject>("VFXPreviewObject");
    previewObject->SetManager(gameObjectManager);
    previewObject->transform->gameObject = previewObject;
    previewModel = previewObject->AddComponent<GltfModelCommponennt>();
    previewParticle = previewObject->AddComponent<ParticleComponent>();
    previewMeshVFX = previewObject->AddComponent<MeshVFXComponent>();
    previewAttackHit = previewObject->AddComponent<AttackEffectHitComponent>();
    previewImpactEffect = previewObject->AddComponent<ImpactEffectComponent>();
    if (previewAttackHit) previewAttackHit->SetDestroyOnLifeEnd(false);
    if (previewParticle)
    {
        previewParticle->RefreshTextureList();
    }
    if (previewMeshVFX)
    {
        previewMeshVFX->Stop();
    }

    previewImpactGroundObject = std::make_shared<GameObject>("ImpactGroundPreviewObject");
    previewImpactGroundObject->SetManager(gameObjectManager);
    previewImpactGroundObject->transform->gameObject = previewImpactGroundObject;
    previewImpactGroundObject->transform->localPosition = { 0.0f, -0.05f, 0.0f };
    previewImpactGroundObject->transform->localScale = { 4.0f, 0.05f, 4.0f };
    previewImpactGroundObject->transform->MarkDirty();
    previewImpactGroundModel =
        previewImpactGroundObject->AddComponent<GltfModelCommponennt>();
    if (previewImpactGroundModel)
    {
        previewImpactGroundModel->LoadModel(
            graphics_.GetDevice(),
            "Data/Model/VFX/Cube.gltf");
    }

    previewDecalObject = std::make_shared<GameObject>("ImpactDecalPreviewObject");
    previewDecalObject->SetManager(gameObjectManager);
    previewDecalObject->transform->gameObject = previewDecalObject;
    previewDecal = previewDecalObject->AddComponent<DecalComponent>();
    if (previewDecal) previewDecal->SetDestroyOnLifeEnd(false);

    editorCamera.ResetPreviewCamera(previewCamera);
    previewCamera.SetPerspectiveFov(45.0f, 16.0f / 9.0f, 0.01f, 100.0f);

    RefreshParticleAssets();
    RefreshSequenceAssets();
    RefreshImpactEffectAssets();
    isInitialized = true;
}

void ParticleEditorWindow::DrawToolbar()
{
    if (ImGui::MenuItem("Refresh Assets"))
    {
        RefreshParticleAssets();
        RefreshSequenceAssets();
        RefreshImpactEffectAssets();
    }

    ImGui::Separator();

    if (ImGui::MenuItem(sequencePlaying ? "Pause" : "Play"))
    {
        sequencePlaying = !sequencePlaying;
        if (sequencePlaying && previewAttackHit) previewAttackHit->Play();
    }
    if (ImGui::MenuItem("Stop"))
    {
        sequencePlaying = false;
        ResetSequence();
    }
    if (ImGui::MenuItem("Reset"))
    {
        ResetSequence();
    }
    ImGui::MenuItem("Loop", nullptr, &sequenceLoop);
    ImGui::MenuItem("Grid", nullptr, &showGrid);
    ImGui::MenuItem("Impact Preview", nullptr, &impactPreviewMode);
    if (impactPreviewMode && ImGui::MenuItem("Drop Impact")) StartImpactPreview();
    if (ImGui::MenuItem("Reset Layout"))
    {
        inspectorWidth = 360.0f;
        timelineHeight = 190.0f;
    }

    ImGui::MenuItem("Free Camera", nullptr, &editorCamera.enablePreviewFreeCamera);

    if (ImGui::MenuItem("Reset Camera"))
    {
        editorCamera.ResetPreviewCamera(previewCamera);
    }
}

void ParticleEditorWindow::DrawPreviewPanel()
{
    ImGui::TextUnformatted("Preview");
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
        ImGui::Dummy(previewSize_);
    }

    if (previewHovered_)
    {
        ImGui::TextDisabled("RMB + Mouse: Look / WASD: Move / Q,E: Up Down / Wheel: Speed");
    }
}


void ParticleEditorWindow::DrawAssetBrowserPanel()
{
    ImGui::SeparatorText("Impact Effects");
    ImGui::InputText("Impact Name", impactEffectNameBuffer, IM_ARRAYSIZE(impactEffectNameBuffer));
    if (ImGui::Button("Save Impact") && previewImpactEffect)
    {
        if (previewImpactEffect->SaveAsset(
            "Data/ImpactEffect/" + std::string(impactEffectNameBuffer) + ".json"))
        {
            RefreshImpactEffectAssets();
            if (gameObjectManager)
            {
                EffectManager& effectManager_ = gameObjectManager->GetEffectManager();
                effectManager_.RefreshAssetCache();
                effectManager_.BuildAssetNameMap();
                effectManager_.PreloadAllEffects();
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Drop Preview"))
    {
        impactPreviewMode = true;
        StartImpactPreview();
    }
    for (int impactIndex_ = 0;
        impactIndex_ < static_cast<int>(impactEffectAssetPaths.size());
        ++impactIndex_)
    {
        const std::filesystem::path path_(impactEffectAssetPaths[impactIndex_]);
        if (ImGui::Selectable(path_.filename().string().c_str()))
        {
            if (previewImpactEffect && previewImpactEffect->LoadAsset(impactEffectAssetPaths[impactIndex_]))
            {
                const std::string name_ = path_.stem().string();
                strncpy_s(impactEffectNameBuffer, name_.c_str(), sizeof(impactEffectNameBuffer) - 1);
                impactEffectNameBuffer[sizeof(impactEffectNameBuffer) - 1] = '\0';
                impactPreviewMode = true;
            }
        }
    }

    ImGui::TextUnformatted("VFX Sequences");
    ImGui::Separator();

    ImGui::InputText("Sequence Name", sequenceNameBuffer, IM_ARRAYSIZE(sequenceNameBuffer));
    if (ImGui::Button("Save Sequence"))
    {
        SaveSequence("Data/VFXSequence/" + std::string(sequenceNameBuffer) + ".json");
        RefreshSequenceAssets();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Sequence") &&
        selectedSequenceIndex >= 0 &&
        selectedSequenceIndex < static_cast<int>(sequenceAssetPaths.size()))
    {
        LoadSequence(sequenceAssetPaths[selectedSequenceIndex]);
    }

    for (int sequenceIndex_ = 0;
        sequenceIndex_ < static_cast<int>(sequenceAssetPaths.size());
        ++sequenceIndex_)
    {
        const std::filesystem::path path_(sequenceAssetPaths[sequenceIndex_]);
        if (ImGui::Selectable(path_.filename().string().c_str(), selectedSequenceIndex == sequenceIndex_))
        {
            selectedSequenceIndex = sequenceIndex_;
        }
    }

    ImGui::SeparatorText("Particle Assets");
    if (ImGui::Button("Refresh Assets"))
    {
        RefreshParticleAssets();
        RefreshSequenceAssets();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Particle") &&
        previewParticle &&
        selectedAssetIndex >= 0 &&
        selectedAssetIndex < static_cast<int>(particleAssetPaths.size()))
    {
        previewParticle->LoadAsset(particleAssetPaths[selectedAssetIndex]);
        previewParticle->Play();
    }

    if (particleAssetPaths.empty())
    {
        ImGui::TextDisabled("No particle json assets found.");
        return;
    }

    for (int i_ = 0; i_ < static_cast<int>(particleAssetPaths.size()); ++i_)
    {
        const std::filesystem::path path_(particleAssetPaths[i_]);
        if (ImGui::Selectable(path_.filename().string().c_str(), selectedAssetIndex == i_))
        {
            selectedAssetIndex = i_;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && previewParticle)
        {
            previewParticle->LoadAsset(particleAssetPaths[i_]);
            previewParticle->Play();
        }
    }
}

void ParticleEditorWindow::DrawTimelinePanel()
{
    struct TimelineSequenceAdapter : public ImSequencer::SequenceInterface
    {
        std::vector<TimelineTrack>* tracks = nullptr;
        ParticleComponent* particle = nullptr;
        float duration = 0.0f;

        int ToFrame(float time_) const { return static_cast<int>(time_ * TIMELINE_FPS); }
        float ToTime(int frame_) const { return static_cast<float>(frame_) / TIMELINE_FPS; }

        int GetFrameMin() const override { return 0; }
        int GetFrameMax() const override { return (std::max)(1, ToFrame(duration)); }
        int GetItemCount() const override { return tracks ? static_cast<int>(tracks->size()) : 0; }
        int GetItemTypeCount() const override { return 2; }
        const char* GetItemTypeName(int typeIndex_) const override
        {
            return typeIndex_ == 0 ? "Particle Emitter" : "Mesh VFX";
        }

        void BeginEdit(int index_) override {}

        void EndEdit() override
        {
            if (!tracks) return;

            for (TimelineTrack& track_ : *tracks)
            {
                track_.frameStart = std::clamp(track_.frameStart, 0, GetFrameMax() - 1);
                track_.frameEnd = std::clamp(track_.frameEnd, track_.frameStart + 1, GetFrameMax());
                track_.startTime = ToTime(track_.frameStart);
                track_.duration = ToTime(track_.frameEnd - track_.frameStart);
            }
        }

        void Get(int index_, int** start_, int** end_, int* type_, unsigned int* color_) override
        {
            if (!tracks || index_ < 0 || index_ >= static_cast<int>(tracks->size())) return;

            TimelineTrack& track_ = (*tracks)[index_];
            track_.frameStart = ToFrame(track_.startTime);
            track_.frameEnd = ToFrame(track_.startTime + track_.duration);
            if (start_) *start_ = &track_.frameStart;
            if (end_) *end_ = &track_.frameEnd;
            if (type_) *type_ = static_cast<int>(track_.type);
            if (color_)
            {
                *color_ = track_.type == TimelineTrackType::PARTICLE_EMITTER
                    ? 0xFF4FA3FF
                    : 0xFFFF8A4F;
            }
        }

        const char* GetItemLabel(int index_) const override
        {
            if (!tracks || index_ < 0 || index_ >= static_cast<int>(tracks->size())) return "";
            const TimelineTrack& track_ = (*tracks)[index_];
            return track_.targetName.empty()
                ? (track_.type == TimelineTrackType::PARTICLE_EMITTER ? "Particle Emitter" : "Mesh VFX")
                : track_.targetName.c_str();
        }

        void CustomDraw(
            int index_,
            ImDrawList* drawList_,
            const ImRect& rect_,
            const ImRect& legendRect_,
            const ImRect& clippingRect_,
            const ImRect& legendClippingRect_) override
        {
            drawList_->PushClipRect(rect_.Min, rect_.Max, true);
            drawList_->AddText(
                ImVec2(rect_.Min.x + 3.0f, rect_.Min.y + 2.0f),
                0xFFFFFFFF,
                GetItemLabel(index_));
            drawList_->PopClipRect();
        }

        void Add(int type_) override
        {
            if (!tracks) return;

            TimelineTrack track_{};
            track_.type = type_ == 0
                ? TimelineTrackType::PARTICLE_EMITTER
                : TimelineTrackType::MESH_VFX;
            if (track_.type == TimelineTrackType::PARTICLE_EMITTER)
            {
                if (particle && !particle->GetTracks().empty())
                {
                    track_.targetName = particle->GetTracks().front().name;
                }
            }
            else
            {
                track_.targetName = "Mesh VFX";
            }

            track_.startTime = 0.0f;
            track_.duration = (std::min)(1.0f, duration);
            tracks->push_back(track_);
        }

        void Del(int index_) override
        {
            if (!tracks || index_ < 0 || index_ >= static_cast<int>(tracks->size())) return;
            tracks->erase(tracks->begin() + index_);
        }
    };

    ImGui::Text("VFX Timeline  %.2f / %.2f", sequenceTime, sequenceDuration);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    if (ImGui::DragFloat("Duration", &sequenceDuration, 0.05f, 0.1f, 600.0f))
    {
        sequenceDuration = (std::max)(0.1f, sequenceDuration);
        const float maximumStartTime_ = (std::max)(0.0f, sequenceDuration - (1.0f / TIMELINE_FPS));
        for (TimelineTrack& track_ : timelineTracks)
        {
            track_.startTime = std::clamp(track_.startTime, 0.0f, maximumStartTime_);
            track_.duration = std::clamp(
                track_.duration,
                1.0f / TIMELINE_FPS,
                sequenceDuration - track_.startTime);
        }
    }

    TimelineSequenceAdapter adapter_;
    adapter_.tracks = &timelineTracks;
    adapter_.particle = previewParticle.get();
    adapter_.duration = sequenceDuration;

    int currentFrame_ = adapter_.ToFrame(sequenceTime);
    const int previousFrame_ = currentFrame_;
    ImGui::PushItemWidth(-1.0f);
    ImSequencer::Sequencer(
        &adapter_,
        &currentFrame_,
        &timelineExpanded,
        &selectedTimelineTrack,
        &firstTimelineFrame,
        ImSequencer::SEQUENCER_EDIT_ALL |
        ImSequencer::SEQUENCER_ADD |
        ImSequencer::SEQUENCER_DEL |
        ImSequencer::SEQUENCER_CHANGE_FRAME);
    ImGui::PopItemWidth();

    if (currentFrame_ != previousFrame_)
    {
        sequencePlaying = false;
        ResetSequence();
        sequenceTime = std::clamp(adapter_.ToTime(currentFrame_), 0.0f, sequenceDuration);
    }

    if (selectedTimelineTrack < 0 ||
        selectedTimelineTrack >= static_cast<int>(timelineTracks.size()))
    {
        ImGui::TextDisabled("Add or select a Particle / Mesh VFX bar to edit it.");
        return;
    }

    TimelineTrack& selectedTrack_ = timelineTracks[selectedTimelineTrack];
    ImGui::SeparatorText("Selected Track");
    const char* trackTypes_[] = { "Particle Emitter", "Mesh VFX" };
    int selectedType_ = static_cast<int>(selectedTrack_.type);
    if (ImGui::Combo("Type", &selectedType_, trackTypes_, IM_ARRAYSIZE(trackTypes_)))
    {
        selectedTrack_.type = static_cast<TimelineTrackType>(selectedType_);
        selectedTrack_.targetName = selectedTrack_.type == TimelineTrackType::MESH_VFX
            ? "Mesh VFX"
            : "";
    }

    if (selectedTrack_.type == TimelineTrackType::PARTICLE_EMITTER && previewParticle)
    {
        if (ImGui::BeginCombo("Emitter", selectedTrack_.targetName.c_str()))
        {
            for (const auto& emitterTrack_ : previewParticle->GetTracks())
            {
                if (ImGui::Selectable(
                    emitterTrack_.name.c_str(),
                    selectedTrack_.targetName == emitterTrack_.name))
                {
                    selectedTrack_.targetName = emitterTrack_.name;
                }
            }
            ImGui::EndCombo();
        }
    }

    float endTime_ = selectedTrack_.startTime + selectedTrack_.duration;
    bool timeChanged_ = ImGui::DragFloat("Start Time", &selectedTrack_.startTime, 0.01f, 0.0f, sequenceDuration);
    timeChanged_ |= ImGui::DragFloat("End Time", &endTime_, 0.01f, 0.0f, sequenceDuration);
    if (timeChanged_)
    {
        const float maximumStartTime_ = (std::max)(0.0f, sequenceDuration - (1.0f / TIMELINE_FPS));
        selectedTrack_.startTime = std::clamp(selectedTrack_.startTime, 0.0f, maximumStartTime_);
        endTime_ = std::clamp(endTime_, selectedTrack_.startTime + (1.0f / TIMELINE_FPS), sequenceDuration);
        selectedTrack_.duration = endTime_ - selectedTrack_.startTime;
    }
}

void ParticleEditorWindow::DrawInspectorPanel()
{
    ImGui::TextUnformatted("VFX Assets / Inspector");
    ImGui::Separator();

    if (!previewParticle || !previewMeshVFX)
    {
        ImGui::TextDisabled("Preview particle is not ready.");
        return;
    }

    const float splitterSize_ = 6.0f;
    const float availableHeight_ = ImGui::GetContentRegionAvail().y;
    assetBrowserHeight = std::clamp(assetBrowserHeight, 140.0f, (std::max)(140.0f, availableHeight_ - 180.0f));
    const float inspectorHeight_ = (std::max)(180.0f, availableHeight_ - assetBrowserHeight - splitterSize_);

    ImGui::BeginChild("VFXInspectorContent", ImVec2(0.0f, inspectorHeight_), false);
    if (ImGui::BeginTabBar("VFXInspectorTabs"))
    {
        if (ImGui::BeginTabItem("Assets"))
        {
            DrawAssetBrowserPanel();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Particle"))
        {
            previewParticle->Gui();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Mesh VFX"))
        {
            previewMeshVFX->Gui();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Attack Hit"))
        {
            if (previewAttackHit) previewAttackHit->Gui();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Impact Effect"))
        {
            impactPreviewMode = true;
            if (ImGui::Button("Drop Preview")) StartImpactPreview();
            ImGui::SameLine();
            ImGui::DragFloat("Height", &impactPreviewHeight, 0.1f, 0.1f, 20.0f);
            ImGui::DragFloat("Fall Speed", &impactPreviewFallSpeed, 0.1f, 0.1f, 50.0f);
            if (previewImpactEffect) previewImpactEffect->Gui();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Source Model"))
        {
            if (previewObject->transform) previewObject->transform->Gui();
            ImGui::Separator();
            if (previewModel) previewModel->Gui();
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
    ImGui::EndChild();

    ImGui::InvisibleButton("##VFXAssetBrowserSplitter", ImVec2(-1.0f, splitterSize_));
    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    if (ImGui::IsItemActive())
    {
        assetBrowserHeight -= ImGui::GetIO().MouseDelta.y;
    }

    ImGui::BeginChild("VFXEmbeddedAssetBrowser", ImVec2(0.0f, assetBrowserHeight), true);
    assetBrowser.DrawContent();
    ImGui::EndChild();
}

void ParticleEditorWindow::RenderPreview(const ImVec2& previewSize_)
{
    auto& graphics_ = GraphicsDeviceManager::Instance();
    ID3D11Device* device_ = graphics_.GetDevice();
    ID3D11DeviceContext* context_ = graphics_.GetContext();
    if (!device_ || !context_ || !previewParticle || !previewMeshVFX) return;

    const int width_ = (std::max)(1, static_cast<int>(previewSize_.x));
    const int height_ = (std::max)(1, static_cast<int>(previewSize_.y));
    if (!previewTarget.Resize(device_, width_, height_)) return;

    previewCamera.SetPerspectiveFov(45.0f, static_cast<float>(width_) / static_cast<float>(height_), 0.01f, 100.0f);

    const float clearColor_[4] = { 0.025f, 0.028f, 0.032f,0.0f };
    previewTarget.Begin(context_, clearColor_);

    const float deltaTime_ = ImGui::GetIO().DeltaTime;

    if (previewObject && previewObject->transform)
    {
        // Keep the preview world matrix synchronized with inspector edits.
        previewObject->transform->UpdateWorldMatrix();
    }

    if (previewModel)
    {
        previewModel->Update(deltaTime_);
    }
    if (impactPreviewMode && previewImpactGroundObject && previewImpactGroundModel)
    {
        previewImpactGroundObject->UpdateTransform();
        previewImpactGroundModel->Update(deltaTime_);
    }
    UpdateSequence(deltaTime_);
    UpdateImpactPreview(deltaTime_);
    previewParticle->Update(deltaTime_);
    previewMeshVFX->Update(deltaTime_);
    if (previewDecal) previewDecal->Update(deltaTime_);

    RenderContext renderContext_{};
    DecalManager decalManager_;
    renderContext_.device = device_;
    renderContext_.deviceContext = context_;
    renderContext_.renderState = graphics_.GetRenderState();
    renderContext_.camera = &previewCamera;
    renderContext_.backBufferRTV = nullptr;
    renderContext_.depthSRV = previewTarget.GetDepthShaderResourceView();
    renderContext_.baseColorSRV = previewTarget.GetShaderResourceView();
    renderContext_.decal = &decalManager_;
    renderContext_.screenWidth = static_cast<float>(width_);
    renderContext_.screenHeight = static_cast<float>(height_);

    if (showGrid)
    {
        editorPrimitiveRenderer.DrawGrid(
            20,
            1.0f,
            { 0.25f, 0.25f, 0.25f, 1.0f },
            true);

        editorPrimitiveRenderer.Render(
            context_,
            previewCamera.GetView(),
            previewCamera.GetProjection());

        previewTarget.ResetRenderTarget(context_);
    }

    auto& shapeRenderer_ = graphics_.GetShapeRendererSystem();
    if (previewAttackHit) previewAttackHit->SubmitDebugShapes();
    if (impactPreviewMode)
    {
        if (impactPreviewPlaying)
        {
            shapeRenderer_.DrawSphere(
                { 0.0f, impactPreviewPositionY, 0.0f },
                0.15f,
                { 1.0f, 0.75f, 0.2f, 1.0f });
        }
    }



    if (previewDecal && previewDecal->IsEnabled())
    {
        previewDecal->SubmitDecal(decalManager_);
    }

    if (previewModel && !previewModel->GetMeshes().empty())
    {
        modelRenderSystem.PushSubmit(previewModel->GetShaderID(), previewModel);
    }
    if (impactPreviewMode &&
        previewImpactGroundModel &&
        !previewImpactGroundModel->GetMeshes().empty())
    {
        modelRenderSystem.PushSubmit(
            previewImpactGroundModel->GetShaderID(),
            previewImpactGroundModel);
    }
    if ((previewModel && !previewModel->GetMeshes().empty()) ||
        (impactPreviewMode && previewImpactGroundModel &&
            !previewImpactGroundModel->GetMeshes().empty()))
    {
        modelRenderSystem.RenderAll(renderContext_);
        previewTarget.ResetRenderTarget(context_);
    }
    else
    {
        particleRenderSystem.ApplySceneConstants(renderContext_);
        graphics_.RenderSkyBox(renderContext_);
    }

    particleRenderSystem.PushSubmit(previewParticle.get());
    particleRenderSystem.RenderAll(renderContext_, true);
    previewTarget.ResetRenderTarget(context_);
    meshVFXRenderSystem.PushSubmit(previewMeshVFX.get());
    meshVFXRenderSystem.RenderAll(
        renderContext_,
        true,
        previewTarget.GetRenderTargetView(),
        previewTarget.GetDepthStencilView());
    previewTarget.ResetRenderTarget(context_);
    context_->OMSetDepthStencilState(
        renderContext_.renderState->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
    shapeRenderer_.Render(
        context_,
        previewCamera.GetView(),
        previewCamera.GetProjection());
    context_->OMSetDepthStencilState(
        renderContext_.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);

    previewTarget.End(context_);
}

void ParticleEditorWindow::UpdateSequence(float deltaTime_)
{
    if (!sequencePlaying) return;

    sequenceTime += deltaTime_;
    if (previewAttackHit) previewAttackHit->Update(deltaTime_);
    for (TimelineTrack& track_ : timelineTracks)
    {
        const bool shouldBeActive_ =
            sequenceTime >= track_.startTime &&
            sequenceTime < track_.startTime + track_.duration;

        if (shouldBeActive_ && !track_.active)
        {
            track_.active = true;
            if (track_.type == TimelineTrackType::PARTICLE_EMITTER && previewParticle)
            {
                previewParticle->TriggerEmitter(track_.targetName);
            }
            else if (track_.type == TimelineTrackType::MESH_VFX && previewMeshVFX)
            {
                previewMeshVFX->Reset();
                previewMeshVFX->Play();
            }
        }
        else if (!shouldBeActive_ && track_.active)
        {
            track_.active = false;
            if (track_.type == TimelineTrackType::PARTICLE_EMITTER && previewParticle)
            {
                previewParticle->StopEmitter(track_.targetName);
            }
            else if (track_.type == TimelineTrackType::MESH_VFX && previewMeshVFX)
            {
                previewMeshVFX->Stop();
            }
        }
    }

    if (sequenceTime >= sequenceDuration)
    {
        const bool shouldContinue_ = sequenceLoop;
        ResetSequence();
        sequencePlaying = shouldContinue_;
        if (sequencePlaying && previewAttackHit) previewAttackHit->Play();
    }
}

void ParticleEditorWindow::ResetSequence()
{
    sequenceTime = 0.0f;
    if (previewParticle) previewParticle->Stop();
    if (previewMeshVFX)
    {
        previewMeshVFX->Stop();
        previewMeshVFX->Reset();
    }
    if (previewAttackHit)
    {
        previewAttackHit->Stop();
        previewAttackHit->ResetRuntime();
    }
    for (TimelineTrack& track_ : timelineTracks)
    {
        track_.active = false;
    }
}

bool ParticleEditorWindow::SaveSequence(const std::string& path_)
{
    if (path_.empty()) return false;

    nlohmann::json sequenceJson_;
    sequenceJson_["AssetType"] = "VFXSequence";
    sequenceJson_["AssetVersion"] = 1;
    sequenceJson_["Duration"] = sequenceDuration;
    sequenceJson_["Loop"] = sequenceLoop;
    sequenceJson_["Tracks"] = nlohmann::json::array();

    for (const TimelineTrack& track_ : timelineTracks)
    {
        sequenceJson_["Tracks"].push_back(
            {
                { "Type", static_cast<int>(track_.type) },
                { "Target", track_.targetName },
                { "StartTime", track_.startTime },
                { "Duration", track_.duration },
            });
    }

    if (previewParticle)
    {
        previewParticle->Serialize(sequenceJson_["Particle"]);
        sequenceJson_["Particle"]["IsPlaying"] = false;
    }
    if (previewMeshVFX)
    {
        previewMeshVFX->Serialize(sequenceJson_["MeshVFX"]);
    }
    if (previewAttackHit)
    {
        previewAttackHit->Serialize(sequenceJson_["AttackEffectHit"]);
    }

    const std::filesystem::path sequencePath_(path_);
    std::filesystem::create_directories(sequencePath_.parent_path());
    std::ofstream file_(sequencePath_, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) return false;

    file_ << sequenceJson_.dump(4);
    return true;
}

bool ParticleEditorWindow::LoadSequence(const std::string& path_)
{
    if (path_.empty() || !std::filesystem::exists(path_)) return false;

    std::ifstream file_(path_);
    if (!file_.is_open()) return false;

    nlohmann::json sequenceJson_;
    file_ >> sequenceJson_;
    if (sequenceJson_.value("AssetType", "") != "VFXSequence") return false;

    sequencePlaying = false;
    ResetSequence();
    sequenceDuration = (std::max)(0.1f, sequenceJson_.value("Duration", 5.0f));
    sequenceLoop = sequenceJson_.value("Loop", true);
    timelineTracks.clear();

    if (sequenceJson_.contains("Tracks") && sequenceJson_["Tracks"].is_array())
    {
        for (const auto& trackJson_ : sequenceJson_["Tracks"])
        {
            TimelineTrack track_{};
            track_.type = static_cast<TimelineTrackType>(
                std::clamp(trackJson_.value("Type", 0), 0, 1));
            track_.targetName = trackJson_.value("Target", "");
            track_.startTime = (std::max)(0.0f, trackJson_.value("StartTime", 0.0f));
            track_.duration = (std::max)(0.01f, trackJson_.value("Duration", 1.0f));
            timelineTracks.push_back(track_);
        }
    }

    if (previewParticle && sequenceJson_.contains("Particle"))
    {
        previewParticle->Deserialize(sequenceJson_["Particle"]);
    }
    if (previewMeshVFX && sequenceJson_.contains("MeshVFX"))
    {
        previewMeshVFX->Deserialize(sequenceJson_["MeshVFX"]);
        previewMeshVFX->Stop();
    }
    if (previewAttackHit && sequenceJson_.contains("AttackEffectHit"))
    {
        previewAttackHit->Deserialize(sequenceJson_["AttackEffectHit"]);
        previewAttackHit->Stop();
    }

    const std::string sequenceName_ = std::filesystem::path(path_).stem().string();
    strncpy_s(sequenceNameBuffer, sequenceName_.c_str(), sizeof(sequenceNameBuffer) - 1);
    sequenceNameBuffer[sizeof(sequenceNameBuffer) - 1] = '\0';
    selectedTimelineTrack = -1;
    return true;
}

void ParticleEditorWindow::RefreshParticleAssets()
{
    particleAssetPaths.clear();
    const std::filesystem::path particleDir_("Data/Particle");
    if (!std::filesystem::exists(particleDir_)) return;

    for (const auto& entry_ : std::filesystem::directory_iterator(particleDir_))
    {
        if (!entry_.is_regular_file()) continue;
        if (entry_.path().extension() == ".json")
        {
            particleAssetPaths.push_back(entry_.path().generic_string());
        }
    }
}

void ParticleEditorWindow::RefreshSequenceAssets()
{
    sequenceAssetPaths.clear();
    const std::filesystem::path sequenceDirectory_("Data/VFXSequence");
    if (!std::filesystem::exists(sequenceDirectory_)) return;

    for (const auto& entry_ : std::filesystem::directory_iterator(sequenceDirectory_))
    {
        if (!entry_.is_regular_file() || entry_.path().extension() != ".json") continue;
        sequenceAssetPaths.push_back(entry_.path().generic_string());
    }
}

void ParticleEditorWindow::RefreshImpactEffectAssets()
{
    impactEffectAssetPaths.clear();
    const std::filesystem::path directory_("Data/ImpactEffect");
    if (!std::filesystem::exists(directory_)) return;
    for (const auto& entry_ : std::filesystem::recursive_directory_iterator(directory_))
    {
        if (entry_.is_regular_file() && entry_.path().extension() == ".json")
        {
            impactEffectAssetPaths.push_back(entry_.path().generic_string());
        }
    }
    std::sort(impactEffectAssetPaths.begin(), impactEffectAssetPaths.end());
}

void ParticleEditorWindow::StartImpactPreview()
{
    impactPreviewPositionY = (std::max)(impactPreviewHeight, 0.1f);
    impactPreviewPlaying = true;
    if (previewParticle) previewParticle->Stop();
    if (previewMeshVFX)
    {
        previewMeshVFX->Stop();
        previewMeshVFX->Reset();
    }
    if (previewDecal) previewDecal->SetEnabled(false);
}

void ParticleEditorWindow::UpdateImpactPreview(float deltaTime_)
{
    if (!impactPreviewMode || !impactPreviewPlaying) return;
    impactPreviewPositionY -= (std::max)(impactPreviewFallSpeed, 0.1f) *
        (std::max)(deltaTime_, 0.0f);
    if (impactPreviewPositionY > 0.0f) return;
    impactPreviewPositionY = 0.0f;
    impactPreviewPlaying = false;
    PlayImpactPreviewAtContact();
}

void ParticleEditorWindow::PlayImpactPreviewAtContact()
{
    if (!previewImpactEffect) return;
    const ImpactEffectAsset& asset_ = previewImpactEffect->GetAsset();
    for (const ImpactEffectEntry& entry_ : asset_.entries)
    {
        if (!entry_.enabled || entry_.assetPath.empty()) continue;
        if (entry_.type == IMPACT_EFFECT_ENTRY_TYPE::PARTICLE && previewParticle)
        {
            if (previewParticle->LoadAsset(entry_.assetPath))
            {
                if (entry_.emitterName.empty()) previewParticle->Play();
                else previewParticle->TriggerEmitter(entry_.emitterName);
            }
        }
        else if (entry_.type == IMPACT_EFFECT_ENTRY_TYPE::MESH_VFX && previewMeshVFX)
        {
            std::ifstream assetFile_(entry_.assetPath);
            if (assetFile_.is_open())
            {
                nlohmann::json assetJson_;
                try
                {
                    assetFile_ >> assetJson_;
                    if (assetJson_.value("AssetType", std::string{}) == "VFXSequence" &&
                        LoadSequence(entry_.assetPath))
                    {
                        sequencePlaying = true;
                        continue;
                    }
                }
                catch (const nlohmann::json::exception&)
                {
                }
            }
            if (previewMeshVFX->LoadAsset(entry_.assetPath))
            {
                previewMeshVFX->Reset();
                previewMeshVFX->Play();
            }
        }
        else if (entry_.type == IMPACT_EFFECT_ENTRY_TYPE::DECAL && previewDecal)
        {
            previewDecal->color = entry_.color;
            previewDecal->SetTexture(entry_.assetPath);
            previewDecal->SetLifeTime(entry_.lifeTime, entry_.fadeOutDuration);
            previewDecal->SetEnabled(true);
            if (previewDecalObject && previewDecalObject->transform)
            {
                previewDecalObject->transform->localPosition = { 0.0f, entry_.normalOffset, 0.0f };
                previewDecalObject->transform->localScale = entry_.scale;
                previewDecalObject->transform->localRotation = { -90.0f, 0.0f, 0.0f };
                previewDecalObject->transform->MarkDirty();
            }
        }
    }
}
