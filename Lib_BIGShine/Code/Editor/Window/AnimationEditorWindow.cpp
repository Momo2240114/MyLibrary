#include "PrecompiledHeader.h"

#include "AnimationEditorWindow.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Camera/CameraManager.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Core/RenderContext.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"
#include "Engine/Graphics/Core/LightManager.h"
#include "Engine/Graphics/Decal/DecalManager.h"
#include "Engine/Graphics/RenderSystem/ShapeRenderer.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Graphics/Resource/Animation/AnimSequenceAdapter.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/Graphics/Resource/Animation/PoseLibraryResourceManager.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Combat/AttackTemplateManager.h"
#include "Engine/GamePlay/Combat/AttackTelegraphRuntime.h"
#include "Engine/Math/Curve/CurveComponent.h"
#include "ImSequencer.h"
#include "ImGuizmo.h"
#include "imgui_internal.h"
#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <DirectXMath.h>
#include "SimpleMath.h"
#include <string>
#include <vector>

namespace
{
    static bool ContainsCaseInsensitive(const std::string& text_, const std::string& query_)
    {
        if (query_.empty()) return true;

        return std::search(
            text_.begin(), text_.end(),
            query_.begin(), query_.end(),
            [](unsigned char left_, unsigned char right_)
            {
                return std::tolower(left_) == std::tolower(right_);
            }) != text_.end();
    }

    static std::string GetPoseLibraryAssetPath(const std::string& modelName)
    {
        return "Data/AnimeJson/" + modelName + "/" + modelName + "_PoseLib.json";
    }

    struct PoseKeyframeSequenceAdapter : public ImSequencer::SequenceInterface
    {
        struct FrameRange
        {
            int start = 0;
            int end = 1;
        };

        CustomAnimation* anim = nullptr;
        PoseLibraryResource* poseLibrary = nullptr;
        GltfModelCommponennt* modelComp = nullptr;
        int selectedPose = -1;
        int selectedSourceAnimation = -1;
        float sourceTime = 0.0f;
        float fps = 60.0f;
        std::vector<FrameRange> frameRanges;

        int ToFrame(float time) const { return static_cast<int>(time * fps); }
        float ToTime(int frame) const { return static_cast<float>(frame) / fps; }

        void SyncFrames()
        {
            const int count = GetItemCount();
            frameRanges.resize(count);
            for (int i = 0; i < count; ++i)
            {
                const int frame = ToFrame(anim->keyframes[i].time);
                frameRanges[i].start = frame;
                frameRanges[i].end = frame + 2;
            }
        }

        int GetFrameMin() const override { return 0; }
        int GetFrameMax() const override { return anim ? ToFrame(anim->duration) : 0; }
        int GetItemCount() const override { return anim ? (int)anim->keyframes.size() : 0; }
        int GetItemTypeCount() const override { return 1; }
        const char* GetItemTypeName(int typeIndex) const override { return "Pose Keyframe"; }

        void BeginEdit(int index) override {}

        void EndEdit() override
        {
            if (!anim) return;
            for (int i = 0; i < (int)anim->keyframes.size() && i < (int)frameRanges.size(); ++i)
            {
                anim->keyframes[i].time = (std::max)(0.0f, (std::min)(anim->duration, ToTime(frameRanges[i].start)));
            }
        }

        void Get(int index, int** start, int** end, int* type, unsigned int* color) override
        {
            if (!anim || index < 0 || index >= (int)anim->keyframes.size()) return;
            if (frameRanges.size() != anim->keyframes.size()) SyncFrames();

            if (start) *start = &frameRanges[index].start;
            if (end) *end = &frameRanges[index].end;
            if (type) *type = 0;
            if (color) *color = 0xFF4FA3FF;
        }

        const char* GetItemLabel(int index) const override
        {
            if (!anim || !poseLibrary || index < 0 || index >= (int)anim->keyframes.size()) return "";

            const int poseIndex = anim->keyframes[index].poseIndex;
            if (poseIndex >= 0 && poseIndex < (int)poseLibrary->poseLibrary.size())
            {
                return poseLibrary->poseLibrary[poseIndex].name.c_str();
            }
            const int sourceIndex_ = anim->keyframes[index].sourceAnimIdx;
            if (modelComp && sourceIndex_ >= 0 && sourceIndex_ < static_cast<int>(modelComp->GetAnimations().size()))
            {
                return modelComp->GetAnimations()[sourceIndex_].name.c_str();
            }
            return "Missing Source";
        }

        void CustomDraw(int index, ImDrawList* drawList, const ImRect& rc, const ImRect& legendRect, const ImRect& clippingRect, const ImRect& legendClippingRect) override
        {
            const char* label = GetItemLabel(index);
            drawList->PushClipRect(rc.Min, rc.Max, true);
            drawList->AddText(ImVec2(rc.Min.x + 3.0f, rc.Min.y + 2.0f), 0xFFFFFFFF, label);
            drawList->PopClipRect();
        }

        void Add(int type) override
        {
            if (!anim || !poseLibrary) return;

            PoseKeyframe key_;
            if (selectedPose >= 0 && selectedPose < static_cast<int>(poseLibrary->poseLibrary.size()))
            {
                key_.poseIndex = selectedPose;
                key_.sourceAnimIdx = poseLibrary->poseLibrary[selectedPose].sourceAnimIndex;
                key_.sourceTime = poseLibrary->poseLibrary[selectedPose].sourceTime;
            }
            else if (modelComp && selectedSourceAnimation >= 0 && selectedSourceAnimation < static_cast<int>(modelComp->GetAnimations().size()))
            {
                key_.poseIndex = -1;
                key_.sourceAnimIdx = selectedSourceAnimation;
                key_.sourceTime = sourceTime;
            }
            else
            {
                return;
            }

            key_.time = anim->keyframes.empty() ? 0.0f : (std::min)(anim->duration, anim->keyframes.back().time + 0.2f);
            anim->keyframes.push_back(key_);
            SyncFrames();
        }

        void Del(int index) override
        {
            if (!anim || index < 0 || index >= (int)anim->keyframes.size()) return;
            anim->keyframes.erase(anim->keyframes.begin() + index);
            SyncFrames();
        }
    };

    struct AnimationScrubSequenceAdapter : public ImSequencer::SequenceInterface
    {
        float duration = 0.0f;
        float fps = 60.0f;

        int ToFrame(float time) const { return static_cast<int>(time * fps); }
        float ToTime(int frame) const { return static_cast<float>(frame) / fps; }

        int GetFrameMin() const override { return 0; }
        int GetFrameMax() const override { return ToFrame(duration); }
        int GetItemCount() const override { return 0; }
        int GetItemTypeCount() const override { return 1; }
        const char* GetItemTypeName(int typeIndex) const override { return "Animation Time"; }
        const char* GetItemLabel(int index) const override { return ""; }
        void Get(int index, int** start, int** end, int* type, unsigned int* color) override {}
        void Add(int type) override {}
        void Del(int index) override {}
        void Duplicate(int index) override {}
    };

    static ImGuizmo::OPERATION ToGizmoOperation(int operation)
    {
        switch (operation)
        {
        case 0: return ImGuizmo::TRANSLATE;
        case 1: return ImGuizmo::ROTATE;
        case 2: return ImGuizmo::SCALE;
        default: return ImGuizmo::ROTATE;
        }
    }

    static void DrawNodeHierarchyItem(
        const std::vector<GltfModelCommponennt::Node>& nodes,
        const std::vector<std::vector<int>>& children,
        int nodeIndex,
        int& selectedNode)
    {
        if (nodeIndex < 0 || nodeIndex >= (int)nodes.size()) return;

        const bool hasChildren = !children[nodeIndex].empty();
        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_SpanAvailWidth;

        if (!hasChildren)
        {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }
        if (selectedNode == nodeIndex)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        std::string label = std::to_string(nodeIndex) + "  " + nodes[nodeIndex].name;
        bool opened = ImGui::TreeNodeEx((void*)(intptr_t)nodeIndex, flags, "%s", label.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            selectedNode = nodeIndex;
        }

        if (hasChildren && opened)
        {
            for (int child : children[nodeIndex])
            {
                DrawNodeHierarchyItem(nodes, children, child, selectedNode);
            }
            ImGui::TreePop();
        }
    }
}

AnimationEditorWindow::AnimationEditorWindow(GameObjectManager* manager_)
    : EditorWorkspaceWindow("Animation Editor", manager_),
    sceneGameObjectManager(manager_),
    assetBrowser(manager_)
{
    isVisible = false;
    previewGameObjectManager = std::make_unique<GameObjectManager>();
    previewGameObjectManager->Initialize();
    gameObjectManager = previewGameObjectManager.get();
}

bool AnimationEditorWindow::ResolveTarget(GltfModelCommponennt*& modelComp, AnimationControllerComponent*& animController) const
{
    modelComp = nullptr;
    animController = nullptr;

    auto selectedObject = GetPreviewManager() ? GetPreviewManager()->GetSelectedObject() : nullptr;
    if (!selectedObject) return false;

    auto model = selectedObject->GetComponent<GltfModelCommponennt>();
    auto animation = selectedObject->GetComponent<AnimationControllerComponent>();
    if (!model || !animation) return false;

    modelComp = model.get();
    animController = animation.get();
    return true;
}

GameObjectManager* AnimationEditorWindow::GetPreviewManager() const
{
    return previewGameObjectManager.get();
}
void AnimationEditorWindow::PlaySelectedEffectPreview(const AnimEvent& event_)
{
    StopSelectedEffectPreview();
    if (!previewGameObjectManager || event_.meshVFXAssetPath.empty()) return;

    const std::shared_ptr<GameObject> owner_ =
        previewGameObjectManager->GetSelectedObject();
    if (!owner_) return;

    AttackEffectSpawnRequest request_{};
    request_.ownerObject = owner_.get();
    request_.setting.vfxPath = event_.meshVFXAssetPath;
    request_.setting.lifeTime = (std::max)(event_.meshVFXLifeTime, 0.01f);
    request_.setting.localOffset = event_.localPosition;
    request_.setting.localRotation = event_.localRotation;
    request_.setting.localScale = event_.localScale;
    request_.setting.playbackSpeed = event_.meshVFXPlaybackSpeed;
    request_.setting.followOwner =
        event_.attachToOwner || !event_.attachNodeName.empty();
    request_.setting.attachNodeName = event_.attachNodeName;
    request_.setting.mirror = event_.mirrorMeshVFX;

    selectedEffectPreviewObject =
        previewGameObjectManager->GetEffectManager().SpawnSlashEffect(request_);
    selectedEffectPreviewTimer = 0.0f;
}

void AnimationEditorWindow::StopSelectedEffectPreview()
{
    if (previewGameObjectManager)
    {
        if (const std::shared_ptr<GameObject> object_ =
            selectedEffectPreviewObject.lock())
        {
            previewGameObjectManager->GetEffectManager().StopEffect(object_);
        }
    }
    selectedEffectPreviewObject.reset();
    selectedEffectPreviewTimer = 0.0f;
}
void AnimationEditorWindow::PlayAttackTemplateEffectPreview(
    const std::string& templateId_)
{
    StopAttackTemplateEffectPreview();
    if (!previewGameObjectManager || templateId_.empty()) return;
    const AttackTemplate* attackTemplate_ =
        AttackTemplateManager::Instance().FindTemplate(templateId_);
    if (!attackTemplate_ || !attackTemplate_->hitEffect.spawnOnHit) return;

    const std::shared_ptr<GameObject> owner_ =
        previewGameObjectManager->GetSelectedObject();
    const DirectX::XMFLOAT3 position_ = owner_ && owner_->transform
        ? owner_->transform->GetWorldPosition()
        : DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f };
    const DirectX::XMFLOAT3 normal_ = { 0.0f, 1.0f, 0.0f };
    EffectManager& effectManager_ = previewGameObjectManager->GetEffectManager();

    attackTemplateEffectPreviewDuration = 0.01f;
    if (!attackTemplate_->hitEffect.hitParticleAssetPath.empty())
    {
        ParticleSpawnRequest request_{};
        request_.assetPath = attackTemplate_->hitEffect.hitParticleAssetPath;
        request_.emitterName = attackTemplate_->hitEffect.hitParticleEmitterName;
        request_.position = position_;
        request_.normal = normal_;
        request_.lifeTime = (std::max)(attackTemplate_->hitEffect.hitParticleLifeTime, 0.01f);
        request_.normalOffset = attackTemplate_->hitEffect.hitParticleNormalOffset;
        if (const std::shared_ptr<GameObject> object_ = effectManager_.SpawnParticle(request_))
        {
            attackTemplateEffectPreviewObjects.push_back(object_);
        }
        attackTemplateEffectPreviewDuration = (std::max)(
            attackTemplateEffectPreviewDuration,
            request_.lifeTime);
    }
    if (!attackTemplate_->hitEffect.hitMeshVfxAssetPath.empty())
    {
        MeshVFXSpawnRequest request_{};
        request_.assetPath = attackTemplate_->hitEffect.hitMeshVfxAssetPath;
        request_.position = position_;
        request_.normal = normal_;
        request_.lifeTime = (std::max)(attackTemplate_->hitEffect.hitMeshVfxLifeTime, 0.01f);
        request_.normalOffset = attackTemplate_->hitEffect.hitMeshVfxNormalOffset;
        if (const std::shared_ptr<GameObject> object_ = effectManager_.SpawnMeshVFX(request_))
        {
            attackTemplateEffectPreviewObjects.push_back(object_);
        }
        attackTemplateEffectPreviewDuration = (std::max)(
            attackTemplateEffectPreviewDuration,
            request_.lifeTime);
    }
    attackTemplateEffectPreviewTimer = 0.0f;
}

void AnimationEditorWindow::StopAttackTemplateEffectPreview()
{
    if (previewGameObjectManager)
    {
        EffectManager& effectManager_ = previewGameObjectManager->GetEffectManager();
        for (const std::weak_ptr<GameObject>& weakObject_ : attackTemplateEffectPreviewObjects)
        {
            if (const std::shared_ptr<GameObject> object_ = weakObject_.lock())
            {
                effectManager_.StopEffect(object_);
            }
        }
    }
    attackTemplateEffectPreviewObjects.clear();
    attackTemplateEffectPreviewTimer = 0.0f;
}
bool AnimationEditorWindow::CopySceneObjectToPreview(
    const std::shared_ptr<GameObject>& sourceObject_)
{
    if (!sourceObject_ || !previewGameObjectManager)
    {
        return false;
    }

    if (!sourceObject_->GetComponent<GltfModelCommponennt>() ||
        !sourceObject_->GetComponent<AnimationControllerComponent>())
    {
        return false;
    }

    std::vector<std::shared_ptr<GameObject>> sourceObjects_;
    auto collectHierarchy_ = [&](auto&& self_, const std::shared_ptr<GameObject>& object_) -> void
        {
            if (!object_)
            {
                return;
            }

            sourceObjects_.push_back(object_);
            for (const auto& childTransform_ : object_->transform->children)
            {
                self_(self_, childTransform_ ? childTransform_->gameObject.lock() : nullptr);
            }
        };
    collectHierarchy_(collectHierarchy_, sourceObject_);

    // WeaponTrailなどの名前参照を維持するため、元のObject名を変えずに
    // プレビュー専用Managerへ親子階層ごと複製する。
    previewGameObjectManager->Clear();
    for (const auto& hierarchyObject_ : sourceObjects_)
    {
        nlohmann::json objectJson_ = hierarchyObject_->Save();
        if (hierarchyObject_ == sourceObject_)
        {
            objectJson_["ParentName"] = "";
            if (!keepSceneTransformOnCopy && objectJson_.contains("Transform"))
            {
                objectJson_["Transform"]["Pos"] = { 0.0f, 0.0f, 0.0f };
                objectJson_["Transform"]["Rot"] = { 0.0f, 0.0f, 0.0f };
            }
        }

        auto previewObject_ = previewGameObjectManager->Create(hierarchyObject_->GetName());
        objectJson_["Name"] = previewObject_->GetName();
        previewObject_->Load(objectJson_);
    }

    previewGameObjectManager->ResolveHierarchy();
    auto previewRootObject_ = previewGameObjectManager->Find(sourceObject_->GetName());
    previewGameObjectManager->SetSelectedObject(previewRootObject_);
    previewGameObjectManager->Update(0.0f);

    selectedPose = -1;
    selectedCustom = -1;
    selectedKeyframe = -1;
    selectedEvent = -1;
    selectedEditNode = -1;
    customPreviewEnabled = false;
    poseEditEnabled = false;
    editorPlaybackEnabled = false;
    return previewRootObject_ != nullptr;
}

void AnimationEditorWindow::EnsurePreviewScene()
{
    if (isPreviewSceneInitialized) return;

    auto& graphics_ = GraphicsDeviceManager::Instance();
    if (!previewGameObjectManager)
    {
        previewGameObjectManager = std::make_unique<GameObjectManager>();
        previewGameObjectManager->Initialize();
    }
    gameObjectManager = previewGameObjectManager.get();
    previewGameObjectManager->SetModelRenderSystem(&previewModelRenderSystem);

    previewModelRenderSystem.Initialize(graphics_.GetDevice());
    previewParticleRenderSystem.Initialize(graphics_.GetDevice());
    previewMeshVFXRenderSystem.Initialize(graphics_.GetDevice());

    auto previewObject_ = previewGameObjectManager->Create("AnimationPreviewModel");
    previewObject_->AddComponent<GltfModelCommponennt>();
    previewObject_->AddComponent<AnimationControllerComponent>();
    previewObject_->AddComponent<ParticleComponent>();
    previewGameObjectManager->SetSelectedObject(previewObject_);

    editorCamera.ResetPreviewCamera(previewCamera);
    previewCamera.SetPerspectiveFov(45.0f, 16.0f / 9.0f, 0.01f, 1000.0f);

    previewGameObjectManager->Update(0.0f);
    isPreviewSceneInitialized = true;
}

void AnimationEditorWindow::Update(float deltaTime_)
{
    EnsurePreviewScene();
    if (autoUpdatePreview && previewGameObjectManager)
    {
        previewGameObjectManager->Update(deltaTime_);
    }

    GltfModelCommponennt* modelComp_ = nullptr;
    AnimationControllerComponent* animController_ = nullptr;
    if (!customPreviewEnabled || !ResolveTarget(modelComp_, animController_)) return;

    auto poseLibrary_ = animController_->GetPoseLibrary();
    if (!poseLibrary_ || selectedCustom < 0 || selectedCustom >= static_cast<int>(poseLibrary_->customAnims.size())) return;

    const float duration_ = (std::max)(0.05f, poseLibrary_->customAnims[selectedCustom].duration);
    if (editorPlaybackEnabled)
    {
        editorPlaybackTime += deltaTime_ * editorPlaybackSpeed;
        if (editorPlaybackLoop)
        {
            editorPlaybackTime = std::fmod((std::max)(0.0f, editorPlaybackTime), duration_);
        }
        else if (editorPlaybackTime >= duration_)
        {
            editorPlaybackTime = duration_;
            editorPlaybackEnabled = false;
        }
    }

    editorPlaybackTime = (std::max)(0.0f, (std::min)(duration_, editorPlaybackTime));
    animController_->PreviewCustomAnimation(selectedCustom, editorPlaybackTime);
}

void AnimationEditorWindow::DrawPreviewPanel()
{
    ImGui::TextUnformatted("Animation Preview");
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

void AnimationEditorWindow::DrawPreviewInspectorPanel()
{
    ImGui::TextUnformatted("Animation Inspector");
    ImGui::Separator();

    if (!previewGameObjectManager || !previewGameObjectManager->GetSelectedObject())
    {
        ImGui::TextDisabled("Select an object in the Animation hierarchy.");
        return;
    }

    if (ImGui::BeginTabBar("AnimationPreviewInspectorTabs"))
    {
        if (ImGui::BeginTabItem("Object"))
        {
            previewGameObjectManager->DrawInspectorGui();
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

void AnimationEditorWindow::DrawPreviewHierarchyPanel()
{
    ImGui::TextUnformatted("Animation Hierarchy");
    ImGui::Separator();

    if (!previewGameObjectManager)
    {
        ImGui::TextDisabled("Preview manager is not ready.");
        return;
    }

    ImGui::SeparatorText("Copy From Current Scene");
    const std::vector<std::shared_ptr<GameObject>> emptySceneObjects_;
    const auto& sceneObjects_ = sceneGameObjectManager
        ? sceneGameObjectManager->GetObjects()
        : emptySceneObjects_;
    if (selectedSceneObjectIndex >= static_cast<int>(sceneObjects_.size()))
    {
        selectedSceneObjectIndex = -1;
    }

    const char* selectedName_ = selectedSceneObjectIndex >= 0
        ? sceneObjects_[selectedSceneObjectIndex]->GetName().c_str()
        : "Select a scene object";
    if (ImGui::BeginCombo("Scene Object", selectedName_))
    {
        for (int objectIndex_ = 0; objectIndex_ < static_cast<int>(sceneObjects_.size()); ++objectIndex_)
        {
            const auto& object_ = sceneObjects_[objectIndex_];
            if (!object_ ||
                !object_->GetComponent<GltfModelCommponennt>() ||
                !object_->GetComponent<AnimationControllerComponent>())
            {
                continue;
            }

            const bool selected_ = selectedSceneObjectIndex == objectIndex_;
            if (ImGui::Selectable(object_->GetName().c_str(), selected_))
            {
                selectedSceneObjectIndex = objectIndex_;
            }
            if (selected_)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Checkbox("Keep Scene Position / Rotation", &keepSceneTransformOnCopy);
    const bool canCopy_ = selectedSceneObjectIndex >= 0 &&
        selectedSceneObjectIndex < static_cast<int>(sceneObjects_.size());
    ImGui::BeginDisabled(!canCopy_);
    if (ImGui::Button("Copy Object Hierarchy To Animation Preview"))
    {
        const auto& sourceObject_ = sceneObjects_[selectedSceneObjectIndex];
        sceneObjectCopyStatus = CopySceneObjectToPreview(sourceObject_)
            ? "Copied hierarchy: " + sourceObject_->GetName()
            : "Copy failed: GltfModel and AnimationController are required.";
    }
    ImGui::EndDisabled();
    if (!sceneObjectCopyStatus.empty())
    {
        ImGui::TextWrapped("%s", sceneObjectCopyStatus.c_str());
    }

    ImGui::SeparatorText("Preview Objects");

    if (ImGui::Button("Add Attachment Object"))
    {
        auto object_ = previewGameObjectManager->Create("Attachment");
        previewGameObjectManager->SetSelectedObject(object_);
    }

    ImGui::SameLine();
    if (ImGui::Button("Add Particle Object"))
    {
        auto object_ = previewGameObjectManager->Create("ParticleAttachment");
        object_->AddComponent<ParticleComponent>();
        previewGameObjectManager->SetSelectedObject(object_);
    }

    ImGui::Separator();
    previewGameObjectManager->DrawHierarchyGui();
}

void AnimationEditorWindow::DrawEditorTimelinePanel()
{
    GltfModelCommponennt* modelComp = nullptr;
    AnimationControllerComponent* animController = nullptr;

    if (!ResolveTarget(modelComp, animController))
    {
        ImGui::TextWrapped("The selected Animation object needs both GltfModel and AnimationControllerComponent.");
        return;
    }

    bool poseCreatorTabActive_ = false;
    bool poseEditTabActive = false;
    if (ImGui::BeginTabBar("AnimationEditorTabs"))
    {
        if (ImGui::BeginTabItem("Pose Creator"))
        {
            poseCreatorTabActive_ = true;
            if (poseEditEnabled)
            {
                poseEditEnabled = false;
                animController->SetEditorPoseOverride(false);
            }
            DrawPoseAnimationCreator(modelComp, animController);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Pose Edit"))
        {
            poseEditTabActive = true;
            DrawPoseEditMode(modelComp, animController);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Event Sequence"))
        {
            if (poseEditEnabled)
            {
                poseEditEnabled = false;
                animController->SetEditorPoseOverride(false);
            }
            DrawEventSequencer(modelComp, animController);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Curve Editor"))
        {
            if (poseEditEnabled)
            {
                poseEditEnabled = false;
                animController->SetEditorPoseOverride(false);
            }
            DrawCurveEditor(animController);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    if (!poseCreatorTabActive_ && customPreviewEnabled)
    {
        customPreviewEnabled = false;
        editorPlaybackEnabled = false;
        if (!poseEditEnabled) animController->SetEditorPoseOverride(false);
    }

    if (!poseEditTabActive && poseEditEnabled)
    {
        poseEditEnabled = false;
        if (!customPreviewEnabled) animController->SetEditorPoseOverride(false);
    }
}

void AnimationEditorWindow::DrawCurveEditor(AnimationControllerComponent* animController_)
{
    if (!animController_)
    {
        ImGui::TextDisabled("AnimationControllerComponent is required.");
        return;
    }

    auto owner_ = animController_->GetGameObject();
    if (!owner_)
    {
        ImGui::TextDisabled("Animation owner is not available.");
        return;
    }

    auto curveComponent_ = owner_->GetComponent<CurveComponent>();
    if (!curveComponent_)
    {
        ImGui::TextDisabled("CurveComponent is required to edit animation curves.");
        if (ImGui::Button("Add Curve Component"))
        {
            curveComponent_ = owner_->AddComponent<CurveComponent>();
        }
        if (!curveComponent_)
        {
            return;
        }
    }

    auto& states_ = animController_->GetStates();
    if (states_.empty())
    {
        ImGui::TextDisabled("No animation states are registered.");
        return;
    }

    if (selectedCurveStateName.empty() || !states_.count(selectedCurveStateName))
    {
        const AnimState* currentState_ = animController_->GetCurrentState();
        selectedCurveStateName = currentState_ && states_.count(currentState_->name)
            ? currentState_->name
            : states_.begin()->first;
    }

    std::vector<std::string> stateNames_;
    stateNames_.reserve(states_.size());
    for (const auto& [stateName_, state_] : states_)
    {
        stateNames_.push_back(stateName_);
    }
    std::sort(stateNames_.begin(), stateNames_.end());

    ImGui::SetNextItemWidth(260.0f);
    if (ImGui::BeginCombo("State", selectedCurveStateName.c_str()))
    {
        ImGui::SetNextItemWidth(240.0f);
        ImGui::InputTextWithHint("##CurveStateSearch", "Search states...", &curveStateSearchText);
        ImGui::Separator();

        for (const std::string& stateName_ : stateNames_)
        {
            if (!ContainsCaseInsensitive(stateName_, curveStateSearchText)) continue;

            const bool selected_ = selectedCurveStateName == stateName_;
            if (ImGui::Selectable(stateName_.c_str(), selected_))
            {
                selectedCurveStateName = stateName_;
                animController_->SelectStateForEditing(stateName_);
            }
            if (selected_)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    AnimState* state_ = states_.at(selectedCurveStateName).get();
    const std::vector<std::string> curveNames_ = curveComponent_->GetTrackNames();
    auto drawCurveSelector_ = [&](
        const char* label_,
        std::string& curveName_,
        const char* nameSuffix_)
        {
            ImGui::PushID(label_);
            ImGui::SetNextItemWidth(220.0f);
            const char* preview_ = curveName_.empty() ? "No Curve" : curveName_.c_str();
            if (ImGui::BeginCombo("##Curve", preview_))
            {
                if (ImGui::Selectable("No Curve", curveName_.empty()))
                {
                    curveName_.clear();
                }
                for (const std::string& curveNameOption_ : curveNames_)
                {
                    if (ImGui::Selectable(
                        curveNameOption_.c_str(),
                        curveName_ == curveNameOption_))
                    {
                        curveName_ = curveNameOption_;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            if (ImGui::Button("+"))
            {
                curveName_ = curveComponent_->CreateTrack(
                    state_->name + nameSuffix_,
                    1.0f);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Create and assign a multiplier curve");
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(label_);
            ImGui::PopID();
        };

    ImGui::SetNextItemWidth(120.0f);
    ImGui::DragFloat("Base Speed", &state_->playbackSpeed, 0.01f, 0.0f, 10.0f);
    drawCurveSelector_("Speed Curve", state_->speedCurveName, ".Speed");

    ImGui::SetNextItemWidth(120.0f);
    ImGui::DragFloat("Root Scale", &state_->rootMotionScale, 0.01f, 0.0f, 10.0f);
    drawCurveSelector_("Root Scale Curve", state_->rootMotionScaleCurveName, ".RootScale");

    ImGui::Separator();
    const float trackPanelWidth_ = (std::min)(280.0f, ImGui::GetContentRegionAvail().x * 0.32f);
    ImGui::BeginChild("AnimationCurveTrackManager", ImVec2(trackPanelWidth_, 0.0f), true);
    curveComponent_->DrawTrackManager();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("AnimationCurveEditorContent", ImVec2(0.0f, 0.0f), true);
    curveComponent_->DrawEditorContent();
    ImGui::EndChild();
}
void AnimationEditorWindow::RenderPreview(const ImVec2& previewSize_)
{
    if (!previewGameObjectManager) return;

    auto& graphics_ = GraphicsDeviceManager::Instance();
    ID3D11Device* device_ = graphics_.GetDevice();
    ID3D11DeviceContext* context_ = graphics_.GetContext();
    if (!device_ || !context_) return;

    const int width_ = (std::max)(1, static_cast<int>(previewSize_.x));
    const int height_ = (std::max)(1, static_cast<int>(previewSize_.y));
    if (!previewTarget.Resize(device_, width_, height_)) return;

    previewCamera.SetPerspectiveFov(45.0f, static_cast<float>(width_) / static_cast<float>(height_), 0.01f, 1000.0f);

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
    // The shared cascade shadow renderer restores the main scene render target.
    // Keep it disabled so the workspace preview stays bound to its own target.
    renderContext_.cascadeShadowMap = nullptr;
    renderContext_.screenWidth = static_cast<float>(width_);
    renderContext_.screenHeight = static_cast<float>(height_);

    context_->OMSetBlendState(renderContext_.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    context_->OMSetDepthStencilState(renderContext_.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
    context_->RSSetState(renderContext_.renderState->GetRasterizerState(RasterizerState::SolidCullBack));

    for (auto& object_ : previewGameObjectManager->GetObjects())
    {
        if (!object_) continue;

        if (auto model_ = object_->GetComponent<GltfModelCommponennt>())
        {
            previewModelRenderSystem.PushSubmit(model_->GetShaderID(), model_);
        }

        if (auto particle_ = object_->GetComponent<ParticleComponent>())
        {
            previewParticleRenderSystem.PushSubmit(particle_.get());
        }

        for (const std::shared_ptr<MeshVFXComponent>& meshVFX_ :
            object_->GetComponents<MeshVFXComponent>())
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


void AnimationEditorWindow::OnVisibilityChanged(bool visible)
{
    if (!visible)
    {
        loopSelectedEffectPreview = false;
        StopSelectedEffectPreview();
        loopAttackTemplateEffectPreview = false;
        StopAttackTemplateEffectPreview();
    }
    if (visible || (!poseEditEnabled && !customPreviewEnabled)) return;

    GltfModelCommponennt* modelComp = nullptr;
    AnimationControllerComponent* animController = nullptr;
    if (ResolveTarget(modelComp, animController))
    {
        animController->SetEditorPoseOverride(false);
    }
    poseEditEnabled = false;
    customPreviewEnabled = false;
    editorPlaybackEnabled = false;
}

void AnimationEditorWindow::Draw()
{
    EnsurePreviewScene();

    if (!BeginWorkspaceWindow(ImGuiWindowFlags_MenuBar))
    {
        EndEditorWindow();
        return;
    }

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::MenuItem("Create Preview Model"))
        {
            auto object_ = GetPreviewManager()->Create("AnimationPreviewModel");
            object_->AddComponent<GltfModelCommponennt>();
            object_->AddComponent<AnimationControllerComponent>();
            object_->AddComponent<ParticleComponent>();
            GetPreviewManager()->SetSelectedObject(object_);
        }
        ImGui::MenuItem("Auto Update", nullptr, &autoUpdatePreview);
        ImGui::MenuItem("Grid", nullptr, &showPreviewGrid);
        ImGui::MenuItem("Free Camera", nullptr, &editorCamera.enablePreviewFreeCamera);
        if (ImGui::MenuItem("Reset Camera"))
        {
            editorCamera.ResetPreviewCamera(previewCamera);
        }
        ImGui::EndMenuBar();
    }

    DrawStylePopup();

    ImVec2 available_ = ImGui::GetContentRegionAvail();
    const float splitterSize_ = 6.0f;
    const float verticalSpacing_ = ImGui::GetStyle().ItemSpacing.y * 2.0f;
    const float minimumTopHeight_ = 180.0f;
    const float minimumTimelineHeight_ = 220.0f;
    const float minimumEditorWidth_ = 420.0f;
    const float minimumAssetBrowserWidth_ = 280.0f;

    if (!layoutSizeInitialized)
    {
        timelineHeight = available_.y * 0.34f;
        assetBrowserWidth = available_.x / 3.0f;
        layoutSizeInitialized = true;
    }

    timelineHeight = std::clamp(
        timelineHeight,
        minimumTimelineHeight_,
        (std::max)(
            minimumTimelineHeight_,
            available_.y - minimumTopHeight_ - splitterSize_ - verticalSpacing_));
    assetBrowserWidth = std::clamp(
        assetBrowserWidth,
        minimumAssetBrowserWidth_,
        (std::max)(minimumAssetBrowserWidth_, available_.x - minimumEditorWidth_ - splitterSize_));

    const float topHeight_ = (std::max)(
        minimumTopHeight_,
        available_.y - timelineHeight - splitterSize_ - verticalSpacing_);
    const float hierarchyWidth_ = 330.0f;
    const float inspectorWidth_ = 390.0f;
    const float previewWidth_ = (std::max)(
        260.0f,
        available_.x - hierarchyWidth_ - inspectorWidth_ - ImGui::GetStyle().ItemSpacing.x * 2.0f);

    ImGui::BeginChild("AnimationPreviewPanel", ImVec2(previewWidth_, topHeight_), true);
    DrawPreviewPanel();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("AnimationPreviewInspectorPanel", ImVec2(inspectorWidth_, topHeight_), true);
    DrawPreviewInspectorPanel();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("AnimationPreviewHierarchyPanel", ImVec2(0.0f, topHeight_), true);
    DrawPreviewHierarchyPanel();
    ImGui::EndChild();

    ImGui::InvisibleButton("##AnimationEditorHorizontalSplitter", ImVec2(-1.0f, splitterSize_));
    const bool horizontalHovered_ = ImGui::IsItemHovered();
    const bool horizontalActive_ = ImGui::IsItemActive();
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImGui::GetItemRectMin(),
        ImGui::GetItemRectMax(),
        ImGui::GetColorU32(
            horizontalHovered_ || horizontalActive_
            ? ImGuiCol_SeparatorHovered
            : ImGuiCol_Separator));
    if (horizontalHovered_ || horizontalActive_)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    if (horizontalActive_)
    {
        timelineHeight -= ImGui::GetIO().MouseDelta.y;
    }

    const float editorWidth_ = (std::max)(
        minimumEditorWidth_,
        available_.x - assetBrowserWidth - splitterSize_);

    ImGui::BeginChild("AnimationEditorTimelinePanel", ImVec2(editorWidth_, timelineHeight), true);
    DrawEditorTimelinePanel();
    ImGui::EndChild();

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::InvisibleButton("##AnimationEditorAssetSplitter", ImVec2(splitterSize_, timelineHeight));
    const bool assetSplitterHovered_ = ImGui::IsItemHovered();
    const bool assetSplitterActive_ = ImGui::IsItemActive();
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImGui::GetItemRectMin(),
        ImGui::GetItemRectMax(),
        ImGui::GetColorU32(
            assetSplitterHovered_ || assetSplitterActive_
            ? ImGuiCol_SeparatorHovered
            : ImGuiCol_Separator));
    if (assetSplitterHovered_ || assetSplitterActive_)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    if (assetSplitterActive_)
    {
        assetBrowserWidth -= ImGui::GetIO().MouseDelta.x;
    }

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::BeginChild(
        "AnimationEditorAssetBrowserPanel",
        ImVec2(assetBrowserWidth, timelineHeight),
        true);
    assetBrowser.DrawContent();
    ImGui::EndChild();

    EndEditorWindow();
    if (!isVisible)
    {
        OnVisibilityChanged(false);
    }
}

void AnimationEditorWindow::DrawPoseAnimationCreator(GltfModelCommponennt* modelComp, AnimationControllerComponent* animController)
{
    auto poseLibrary = animController->GetPoseLibrary();
    if (!poseLibrary)
    {
        ImGui::TextDisabled("Pose library is not ready.");
        return;
    }

    const std::string posePath = GetPoseLibraryAssetPath(poseLibrary->modelName);
    ImGui::Text("Pose Library: %s", poseLibrary->modelName.c_str());

    if (ImGui::Button("Save Pose Library"))
    {
        PoseLibraryResourceManager::Instance().SaveToJson(posePath, poseLibrary);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload Pose Library"))
    {
        PoseLibraryResourceManager::Instance().LoadFromJson(posePath, poseLibrary);
        selectedPose = -1;
        selectedCustom = -1;
        selectedKeyframe = -1;
    }

    ImGui::SeparatorText("Bake Source Animation To JSON");
    const auto& modelAnimations_ = modelComp->GetAnimations();
    if (modelAnimations_.empty())
    {
        ImGui::TextDisabled("The current model has no source animations.");
    }
    else
    {
        bakeSourceAnimationIndex = (std::max)(0, (std::min)(bakeSourceAnimationIndex, static_cast<int>(modelAnimations_.size()) - 1));
        ImGui::SetNextItemWidth(240.0f);
        if (ImGui::BeginCombo("Bake Source", modelAnimations_[bakeSourceAnimationIndex].name.c_str()))
        {
            for (int animationIndex_ = 0; animationIndex_ < static_cast<int>(modelAnimations_.size()); ++animationIndex_)
            {
                if (ImGui::Selectable(modelAnimations_[animationIndex_].name.c_str(), bakeSourceAnimationIndex == animationIndex_))
                {
                    bakeSourceAnimationIndex = animationIndex_;
                    bakedAnimationName = modelAnimations_[animationIndex_].name + "_Original";
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputText("Output Animation Name", &bakedAnimationName);
        ImGui::SetNextItemWidth(140.0f);
        ImGui::DragFloat("Sample Interval", &bakeSampleInterval, 0.01f, 1.0f / 120.0f, 1.0f, "%.3f s");

        if (ImGui::Button("Bake Source Animation And Save JSON"))
        {
            bakeSampleInterval = (std::max)(1.0f / 120.0f, bakeSampleInterval);
            const float duration_ = (std::max)(0.05f, modelComp->GetAnimationDuration(bakeSourceAnimationIndex));
            std::string outputName_ = bakedAnimationName.empty() ? modelAnimations_[bakeSourceAnimationIndex].name + "_Original" : bakedAnimationName;

            auto duplicateIt_ = std::find_if(poseLibrary->customAnims.begin(), poseLibrary->customAnims.end(),
                [&](const CustomAnimation& existingAnimation_) { return existingAnimation_.name == outputName_; });
            if (duplicateIt_ != poseLibrary->customAnims.end())
            {
                outputName_ += "_" + std::to_string(poseLibrary->customAnims.size());
            }

            CustomAnimation bakedAnimation_;
            bakedAnimation_.name = outputName_;
            bakedAnimation_.duration = duration_;
            bakedAnimation_.baseAnimationIndex = -1;
            bakedAnimation_.poseBlend = 1.0f;

            const int sampleCount_ = (std::max)(2, static_cast<int>(std::ceil(duration_ / bakeSampleInterval)) + 1);
            for (int sampleIndex_ = 0; sampleIndex_ < sampleCount_; ++sampleIndex_)
            {
                const float sampleTime_ = sampleIndex_ == sampleCount_ - 1
                    ? duration_
                    : (std::min)(duration_, static_cast<float>(sampleIndex_) * bakeSampleInterval);

                SavedPose pose_;
                pose_.name = outputName_ + "_Pose_" + std::to_string(sampleIndex_);
                pose_.sourceAnimIndex = bakeSourceAnimationIndex;
                pose_.sourceTime = sampleTime_;
                modelComp->ComputeAnimation(bakeSourceAnimationIndex, sampleTime_, pose_.nodePoses);

                poseLibrary->poseLibrary.push_back(std::move(pose_));
                PoseKeyframe keyframe_;
                keyframe_.time = sampleTime_;
                keyframe_.poseIndex = static_cast<int>(poseLibrary->poseLibrary.size()) - 1;
                keyframe_.sourceAnimIdx = bakeSourceAnimationIndex;
                keyframe_.sourceTime = sampleTime_;
                bakedAnimation_.keyframes.push_back(keyframe_);
            }

            poseLibrary->customAnims.push_back(std::move(bakedAnimation_));
            selectedCustom = static_cast<int>(poseLibrary->customAnims.size()) - 1;
            selectedPose = poseLibrary->customAnims[selectedCustom].keyframes.empty()
                ? -1
                : poseLibrary->customAnims[selectedCustom].keyframes.front().poseIndex;
            selectedKeyframe = -1;
            bakedAnimationName = outputName_;

            animController->AddOrUpdateCustomState(selectedCustom);
            const bool saved_ = PoseLibraryResourceManager::Instance().SaveToJson(posePath, poseLibrary);
            bakeStatusMessage = saved_
                ? "Saved: " + posePath
                : "Failed to save: " + posePath;
        }
    }

    if (!bakeStatusMessage.empty())
    {
        ImGui::TextWrapped("%s", bakeStatusMessage.c_str());
    }
    ImGui::SeparatorText("Capture Pose");
    ImGui::SetNextItemWidth(180.0f);
    ImGui::InputText("Pose Name", &newPoseName);
    ImGui::SameLine();
    if (ImGui::Button("Record Current Pose"))
    {
        SavedPose pose;
        pose.name = newPoseName.empty() ? ("Pose_" + std::to_string(poseLibrary->poseLibrary.size())) : newPoseName;
        modelComp->GetNodePoses(pose.nodePoses);

        if (const AnimState* state = animController->GetCurrentState())
        {
            pose.sourceAnimIndex = state->animationIndex;
            pose.sourceTime = animController->GetAnimatorTime();
        }

        poseLibrary->poseLibrary.push_back(pose);
        selectedPose = (int)poseLibrary->poseLibrary.size() - 1;
        newPoseName = "Pose_" + std::to_string(poseLibrary->poseLibrary.size());
    }

    const bool showPoseLibraryList_ = ImGui::BeginChild("PoseLibraryList", ImVec2(0, 115), true);
    if (showPoseLibraryList_)
    {
        int poseToDelete = -1;
        for (int i = 0; i < (int)poseLibrary->poseLibrary.size(); ++i)
        {
            ImGui::PushID(i);
            if (ImGui::Selectable(poseLibrary->poseLibrary[i].name.c_str(), selectedPose == i))
            {
                selectedPose = i;
            }
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 28.0f);
            if (ImGui::SmallButton("X")) poseToDelete = i;
            ImGui::PopID();
        }

        if (poseToDelete >= 0)
        {
            poseLibrary->poseLibrary.erase(poseLibrary->poseLibrary.begin() + poseToDelete);
            for (auto& anim : poseLibrary->customAnims)
            {
                anim.keyframes.erase(
                    std::remove_if(anim.keyframes.begin(), anim.keyframes.end(),
                        [poseToDelete](const PoseKeyframe& key) { return key.poseIndex == poseToDelete; }),
                    anim.keyframes.end());

                for (auto& key : anim.keyframes)
                {
                    if (key.poseIndex > poseToDelete) --key.poseIndex;
                }
            }
            selectedPose = -1;
        }
    }
    ImGui::EndChild();

    ImGui::SeparatorText("Custom Animation");
    ImGui::SetNextItemWidth(180.0f);
    ImGui::InputText("Animation Name", &newCustomName);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::DragFloat("Duration", &newCustomDuration, 0.05f, 0.05f, 60.0f);
    ImGui::SameLine();
    if (ImGui::Button("Create Custom Animation"))
    {
        CustomAnimation anim_;
        anim_.name = newCustomName.empty() ? ("CustomAnim_" + std::to_string(poseLibrary->customAnims.size())) : newCustomName;
        anim_.duration = (std::max)(0.05f, newCustomDuration);

        if (const AnimState* currentState_ = animController->GetCurrentState())
        {
            if (currentState_->type == AnimStateType::Single &&
                currentState_->animationIndex >= 0 &&
                currentState_->animationIndex < static_cast<int>(modelComp->GetAnimations().size()))
            {
                anim_.baseAnimationIndex = currentState_->animationIndex;
                anim_.duration = (std::max)(0.05f, modelComp->GetAnimationDuration(anim_.baseAnimationIndex));
            }
        }

        poseLibrary->customAnims.push_back(anim_);
        selectedCustom = static_cast<int>(poseLibrary->customAnims.size()) - 1;
        animController->AddOrUpdateCustomState(selectedCustom);
        selectedKeyframe = -1;
        newCustomName = "CustomAnim_" + std::to_string(poseLibrary->customAnims.size());
    }

    if (poseLibrary->customAnims.empty())
    {
        customPreviewEnabled = false;
        editorPlaybackEnabled = false;
        animController->SetEditorPoseOverride(false);
        ImGui::TextDisabled("Create a custom animation, then add recorded poses as keyframes.");
        return;
    }

    if (selectedCustom < 0 || selectedCustom >= (int)poseLibrary->customAnims.size()) selectedCustom = 0;

    std::vector<const char*> animNames;
    for (auto& anim : poseLibrary->customAnims) animNames.push_back(anim.name.c_str());
    ImGui::SetNextItemWidth(240.0f);
    if (ImGui::Combo("Editing Custom", &selectedCustom, animNames.data(), (int)animNames.size()))
    {
        selectedKeyframe = -1;
        editorPlaybackTime = 0.0f;
    }

    CustomAnimation& anim = poseLibrary->customAnims[selectedCustom];
    ImGui::InputText("Name##CustomAnim", &anim.name);
    ImGui::DragFloat("Duration##CustomAnim", &anim.duration, 0.05f, 0.05f, 60.0f);

    ImGui::SeparatorText("Playback");
    if (ImGui::Checkbox("Preview Editing Custom", &customPreviewEnabled) && !customPreviewEnabled)
    {
        editorPlaybackEnabled = false;
        animController->SetEditorPoseOverride(false);
    }
    ImGui::SameLine();
    if (ImGui::Button(editorPlaybackEnabled ? "Pause" : "Play"))
    {
        customPreviewEnabled = true;
        editorPlaybackEnabled = !editorPlaybackEnabled;
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop"))
    {
        customPreviewEnabled = true;
        editorPlaybackEnabled = false;
        editorPlaybackTime = 0.0f;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Loop", &editorPlaybackLoop);
    ImGui::SetNextItemWidth(140.0f);
    ImGui::DragFloat("Speed##EditorPlayback", &editorPlaybackSpeed, 0.05f, 0.0f, 4.0f, "%.2fx");
    editorPlaybackTime = (std::max)(0.0f, (std::min)((std::max)(0.05f, anim.duration), editorPlaybackTime));
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderFloat("Time##EditorPlayback", &editorPlaybackTime, 0.0f, (std::max)(0.05f, anim.duration), "%.3f s"))
    {
        customPreviewEnabled = true;
        editorPlaybackEnabled = false;
    }

    ImGui::SeparatorText("Base Animation");
    const auto& sourceAnimations_ = modelComp->GetAnimations();
    std::string baseAnimationName_ = "None";
    if (anim.baseAnimationIndex >= 0 && anim.baseAnimationIndex < static_cast<int>(sourceAnimations_.size()))
    {
        baseAnimationName_ = sourceAnimations_[anim.baseAnimationIndex].name;
    }
    ImGui::SetNextItemWidth(240.0f);
    if (ImGui::BeginCombo("Base Animation", baseAnimationName_.c_str()))
    {
        if (ImGui::Selectable("None", anim.baseAnimationIndex < 0))
        {
            anim.baseAnimationIndex = -1;
        }
        for (int animationIndex_ = 0; animationIndex_ < static_cast<int>(sourceAnimations_.size()); ++animationIndex_)
        {
            if (ImGui::Selectable(sourceAnimations_[animationIndex_].name.c_str(), anim.baseAnimationIndex == animationIndex_))
            {
                anim.baseAnimationIndex = animationIndex_;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::DragFloat("Base Start Time", &anim.baseAnimationStartTime, 0.01f, 0.0f, 60.0f, "%.3f s");
    ImGui::DragFloat("Base Speed", &anim.baseAnimationSpeed, 0.01f, 0.0f, 10.0f);
    ImGui::SliderFloat("Pose Edit Blend", &anim.poseBlend, 0.0f, 1.0f);
    if (anim.baseAnimationIndex >= 0 && anim.baseAnimationIndex < static_cast<int>(sourceAnimations_.size()) &&
        ImGui::Button("Match Duration To Base"))
    {
        anim.duration = (std::max)(0.05f, modelComp->GetAnimationDuration(anim.baseAnimationIndex));
    }

    if (ImGui::Button("Add/Update State"))
    {
        animController->AddOrUpdateCustomState(selectedCustom);
    }
    ImGui::SameLine();
    if (ImGui::Button("Play State"))
    {
        customPreviewEnabled = false;
        editorPlaybackEnabled = false;
        animController->SetEditorPoseOverride(false);
        animController->AddOrUpdateCustomState(selectedCustom);
        animController->ChangeState(anim.name, 0.2f);
    }

    std::string selectedStateName_ = animController->GetStates().count(anim.name) ? anim.name : "Select State";
    ImGui::SetNextItemWidth(240.0f);
    if (ImGui::BeginCombo("Registered State", selectedStateName_.c_str()))
    {
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputTextWithHint(
            "##RegisteredStateSearch",
            "Search states...",
            &registeredStateSearchText);
        ImGui::Separator();

        for (auto& [stateName_, state_] : animController->GetStates())
        {
            if (!state_) continue;
            if (!ContainsCaseInsensitive(stateName_, registeredStateSearchText)) continue;

            const bool selected_ = stateName_ == selectedStateName_;
            if (ImGui::Selectable(stateName_.c_str(), selected_))
            {
                animController->SelectStateForEditing(stateName_);
                if (state_->type == AnimStateType::Custom &&
                    state_->customAnimIndex >= 0 &&
                    state_->customAnimIndex < static_cast<int>(poseLibrary->customAnims.size()))
                {
                    selectedCustom = state_->customAnimIndex;
                    selectedKeyframe = -1;
                }
            }
        }
        ImGui::EndCombo();
    }

    DrawPoseKeyframeSequencer(anim, *poseLibrary, modelComp);

    ImGui::SeparatorText("Natural Motion");
    auto& natural = anim.naturalMotion;
    ImGui::Checkbox("Enable Natural Motion", &natural.enabled);
    ImGui::SetNextItemWidth(180.0f);
    ImGui::InputText("Target Node Filter", &natural.nodeFilter);
    ImGui::DragFloat("Rotation Amplitude", &natural.rotationAmplitudeDeg, 0.1f, 0.0f, 30.0f, "%.2f deg");
    ImGui::DragFloat("Position Amplitude", &natural.positionAmplitude, 0.001f, 0.0f, 1.0f);
    ImGui::DragFloat("Frequency", &natural.frequency, 0.05f, 0.0f, 20.0f);
    ImGui::DragFloat("Phase", &natural.phase, 0.01f, 0.0f, 1.0f);
    ImGui::SliderFloat("Blend", &natural.blend, 0.0f, 1.0f);
}

void AnimationEditorWindow::DrawPoseKeyframeSequencer(CustomAnimation& anim, PoseLibraryResource& poseLibrary, GltfModelCommponennt* modelComp)
{
    ImGui::SeparatorText("Pose Keyframe Sequencer");

    PoseKeyframeSequenceAdapter adapter;
    adapter.anim = &anim;
    adapter.poseLibrary = &poseLibrary;
    adapter.modelComp = modelComp;
    adapter.selectedPose = selectedPose;
    adapter.selectedSourceAnimation = anim.baseAnimationIndex;
    adapter.sourceTime = anim.baseAnimationStartTime + editorPlaybackTime * anim.baseAnimationSpeed;
    adapter.SyncFrames();

    int currentFrame = static_cast<int>(editorPlaybackTime * 60.0f);
    const int previousFrame_ = currentFrame;
    const bool showPoseKeyframeSequencer_ = ImGui::BeginChild("PoseKeyframeSequencerRegion", ImVec2(0, 230), true);
    if (showPoseKeyframeSequencer_)
    {
        if (ImSequencer::Sequencer(&adapter, &currentFrame, &poseSequencerExpanded, &selectedKeyframe, &firstPoseFrame,
            ImSequencer::SEQUENCER_EDIT_ALL | ImSequencer::SEQUENCER_ADD | ImSequencer::SEQUENCER_DEL | ImSequencer::SEQUENCER_CHANGE_FRAME))
        {
            adapter.EndEdit();
        }
    }
    ImGui::EndChild();

    if (currentFrame != previousFrame_)
    {
        editorPlaybackTime = (std::max)(0.0f, (std::min)(anim.duration, currentFrame / 60.0f));
        customPreviewEnabled = true;
        editorPlaybackEnabled = false;
    }

    if (ImGui::Button("Sort Keyframes"))
    {
        std::sort(anim.keyframes.begin(), anim.keyframes.end(), [](const PoseKeyframe& a_, const PoseKeyframe& b_) { return a_.time < b_.time; });
    }
    if (modelComp && anim.baseAnimationIndex >= 0 && anim.baseAnimationIndex < static_cast<int>(modelComp->GetAnimations().size()))
    {
        ImGui::SameLine();
        if (ImGui::Button("Add Base Source Keyframe"))
        {
            PoseKeyframe key_;
            key_.time = (std::max)(0.0f, (std::min)(anim.duration, editorPlaybackTime));
            key_.poseIndex = -1;
            key_.sourceAnimIdx = anim.baseAnimationIndex;
            key_.sourceTime = anim.baseAnimationStartTime + editorPlaybackTime * anim.baseAnimationSpeed;
            const auto insertIt_ = std::upper_bound(anim.keyframes.begin(), anim.keyframes.end(), key_.time,
                [](float time_, const PoseKeyframe& existingKey_) { return time_ < existingKey_.time; });
            selectedKeyframe = static_cast<int>(insertIt_ - anim.keyframes.begin());
            anim.keyframes.insert(insertIt_, key_);
        }
    }

    if (selectedKeyframe < 0 || selectedKeyframe >= (int)anim.keyframes.size())
    {
        ImGui::TextDisabled("Select a keyframe on the sequencer to edit its pose and interpolation.");
        return;
    }

    PoseKeyframe& key = anim.keyframes[selectedKeyframe];
    ImGui::SeparatorText("Selected Keyframe");
    ImGui::DragFloat("Time", &key.time, 0.01f, 0.0f, anim.duration);

    int sourceMode_ = key.poseIndex >= 0 ? 0 : 1;
    if (ImGui::RadioButton("Recorded Pose", sourceMode_ == 0))
    {
        sourceMode_ = 0;
        if (key.poseIndex < 0 && !poseLibrary.poseLibrary.empty()) key.poseIndex = 0;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Source Animation", sourceMode_ == 1))
    {
        sourceMode_ = 1;
        key.poseIndex = -1;
        if (modelComp && (key.sourceAnimIdx < 0 || key.sourceAnimIdx >= static_cast<int>(modelComp->GetAnimations().size())))
        {
            key.sourceAnimIdx = anim.baseAnimationIndex >= 0 ? anim.baseAnimationIndex : 0;
        }
    }

    if (sourceMode_ == 0)
    {
        std::string posePreview_ = "None";
        if (key.poseIndex >= 0 && key.poseIndex < static_cast<int>(poseLibrary.poseLibrary.size()))
        {
            posePreview_ = poseLibrary.poseLibrary[key.poseIndex].name;
        }

        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::BeginCombo("Pose", posePreview_.c_str()))
        {
            for (int poseIndex_ = 0; poseIndex_ < static_cast<int>(poseLibrary.poseLibrary.size()); ++poseIndex_)
            {
                if (ImGui::Selectable(poseLibrary.poseLibrary[poseIndex_].name.c_str(), key.poseIndex == poseIndex_))
                {
                    key.poseIndex = poseIndex_;
                    key.sourceAnimIdx = poseLibrary.poseLibrary[poseIndex_].sourceAnimIndex;
                    key.sourceTime = poseLibrary.poseLibrary[poseIndex_].sourceTime;
                }
            }
            ImGui::EndCombo();
        }
    }
    else if (modelComp)
    {
        const auto& sourceAnimations_ = modelComp->GetAnimations();
        std::string sourceName_ = "None";
        if (key.sourceAnimIdx >= 0 && key.sourceAnimIdx < static_cast<int>(sourceAnimations_.size()))
        {
            sourceName_ = sourceAnimations_[key.sourceAnimIdx].name;
        }
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::BeginCombo("Source Animation", sourceName_.c_str()))
        {
            for (int animationIndex_ = 0; animationIndex_ < static_cast<int>(sourceAnimations_.size()); ++animationIndex_)
            {
                if (ImGui::Selectable(sourceAnimations_[animationIndex_].name.c_str(), key.sourceAnimIdx == animationIndex_))
                {
                    key.sourceAnimIdx = animationIndex_;
                    key.poseIndex = -1;
                }
            }
            ImGui::EndCombo();
        }

        float sourceDuration_ = 60.0f;
        if (key.sourceAnimIdx >= 0 && key.sourceAnimIdx < static_cast<int>(sourceAnimations_.size()))
        {
            sourceDuration_ = (std::max)(0.0f, modelComp->GetAnimationDuration(key.sourceAnimIdx));
        }
        ImGui::DragFloat("Source Time", &key.sourceTime, 0.01f, 0.0f, sourceDuration_, "%.3f s");
        ImGui::SameLine();
        if (ImGui::Button("Use Timeline Time"))
        {
            key.sourceTime = (std::max)(0.0f, (std::min)(sourceDuration_, editorPlaybackTime));
        }
    }
    const char* interpolationNames[] = { "Linear", "SmoothStep", "EaseIn", "EaseOut", "EaseInOut", "Hold" };
    ImGui::SetNextItemWidth(160.0f);
    ImGui::Combo("Interpolation", &key.interpolation, interpolationNames, IM_ARRAYSIZE(interpolationNames));
}

void AnimationEditorWindow::SampleEditPoseFromAnimation(GltfModelCommponennt* modelComp)
{
    const auto& animations = modelComp->GetAnimations();
    if (animations.empty()) return;

    poseEditAnimIndex = (std::max)(0, (std::min)(poseEditAnimIndex, (int)animations.size() - 1));
    const float duration = (std::max)(0.0f, modelComp->GetAnimationDuration(poseEditAnimIndex));
    poseEditTime = (std::max)(0.0f, (std::min)(poseEditTime, duration));

    modelComp->ComputeAnimation(poseEditAnimIndex, poseEditTime, editPoseBuffer);
    ApplyEditPoseToModel(modelComp);
    poseEditPoseDirty = false;
}

void AnimationEditorWindow::ApplyEditPoseToModel(GltfModelCommponennt* modelComp)
{
    if (editPoseBuffer.size() != modelComp->GetNodes().size())
    {
        modelComp->GetNodePoses(editPoseBuffer);
    }

    modelComp->SetNodePoses(editPoseBuffer);
    modelComp->UpdateTransform();
}

void AnimationEditorWindow::DrawSelectedNodeGizmo(GltfModelCommponennt* modelComp)
{
    if (selectedEditNode < 0 || selectedEditNode >= (int)modelComp->GetNodes().size())
    {
        return;
    }

    const CameraComponent* camera = &previewCamera;

    const auto& nodes = modelComp->GetNodes();
    const auto& node = nodes[selectedEditNode];

    DirectX::XMFLOAT4X4 view = camera->GetView();
    DirectX::XMFLOAT4X4 projection = camera->GetProjection();

    DirectX::XMFLOAT4X4 gizmoMatrix = node.worldTransform;
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

    if (!ImGuizmo::Manipulate(
        &view._11,
        &projection._11,
        ToGizmoOperation(gizmoOperation),
        ImGuizmo::LOCAL,
        &gizmoMatrix._11))
    {
        return;
    }

    if (!ImGuizmo::IsUsing()) return;

    DirectX::XMMATRIX newWorld = DirectX::XMLoadFloat4x4(&gizmoMatrix);
    DirectX::XMMATRIX parentWorld = DirectX::XMMatrixIdentity();

    if (node.parentIndex >= 0 && node.parentIndex < (int)nodes.size())
    {
        parentWorld = DirectX::XMLoadFloat4x4(&nodes[node.parentIndex].worldTransform);
    }
    else if (auto owner = modelComp->GetGameObject())
    {
        if (owner->transform)
        {
            parentWorld = owner->transform->worldMatrix;
        }
    }

    DirectX::XMMATRIX local = newWorld * DirectX::XMMatrixInverse(nullptr, parentWorld);
    DirectX::XMVECTOR scale;
    DirectX::XMVECTOR rotation;
    DirectX::XMVECTOR translation;

    if (!DirectX::XMMatrixDecompose(&scale, &rotation, &translation, local))
    {
        return;
    }

    auto& pose = editPoseBuffer[selectedEditNode];
    DirectX::XMStoreFloat3(&pose.position, translation);
    DirectX::XMStoreFloat4(&pose.rotation, rotation);
    DirectX::XMStoreFloat3(&pose.scale, scale);

    poseEditPoseDirty = true;
    ApplyEditPoseToModel(modelComp);
}

void AnimationEditorWindow::DrawPoseEditMode(GltfModelCommponennt* modelComp, AnimationControllerComponent* animController)
{
    const auto& animations = modelComp->GetAnimations();
    if (animations.empty())
    {
        ImGui::TextDisabled("This model has no animations to sample.");
        return;
    }

    if (!poseEditEnabled)
    {
        poseEditEnabled = true;
        animController->SetEditorPoseOverride(true);
        if (const AnimState* currentState = animController->GetCurrentState())
        {
            if (currentState->animationIndex >= 0 && currentState->animationIndex < (int)animations.size())
            {
                poseEditAnimIndex = currentState->animationIndex;
                poseEditTime = animController->GetAnimatorTime();
            }
        }
        SampleEditPoseFromAnimation(modelComp);
        if (selectedEditNode < 0 && !modelComp->GetNodes().empty()) selectedEditNode = 0;
    }
    else
    {
        animController->SetEditorPoseOverride(true);
    }

    ImGui::TextDisabled("Pose Edit pauses animation updates while this tab is active.");

    std::vector<const char*> animationNames;
    animationNames.reserve(animations.size());
    for (const auto& anim : animations)
    {
        animationNames.push_back(anim.name.c_str());
    }

    poseEditAnimIndex = (std::max)(0, (std::min)(poseEditAnimIndex, (int)animations.size() - 1));
    ImGui::SetNextItemWidth(260.0f);
    if (ImGui::Combo("Source Animation", &poseEditAnimIndex, animationNames.data(), (int)animationNames.size()))
    {
        poseEditTime = 0.0f;
        SampleEditPoseFromAnimation(modelComp);
    }

    const float duration = (std::max)(0.0f, modelComp->GetAnimationDuration(poseEditAnimIndex));
    bool timeChanged = false;

    AnimationScrubSequenceAdapter scrubAdapter;
    scrubAdapter.duration = duration;
    scrubAdapter.fps = 60.0f;
    int scrubFrame = scrubAdapter.ToFrame(poseEditTime);
    int selectedScrubEntry = -1;
    const bool showPoseEditScrubSequencer_ = ImGui::BeginChild("PoseEditAnimationScrubSequencer", ImVec2(0, 62), true);
    if (showPoseEditScrubSequencer_)
    {
        ImSequencer::Sequencer(&scrubAdapter, &scrubFrame, &editScrubExpanded, &selectedScrubEntry, &firstEditFrame,
            ImSequencer::SEQUENCER_CHANGE_FRAME);
    }
    ImGui::EndChild();
    const float scrubTime = scrubAdapter.ToTime(scrubFrame);
    if (std::abs(scrubTime - poseEditTime) > 0.0001f)
    {
        poseEditTime = scrubTime;
        timeChanged = true;
    }

    ImGui::SetNextItemWidth((std::max)(160.0f, ImGui::GetContentRegionAvail().x - 210.0f));
    timeChanged |= ImGui::SliderFloat("Time", &poseEditTime, 0.0f, duration, "%.3f s");
    ImGui::SameLine();
    if (ImGui::Button("Sample"))
    {
        SampleEditPoseFromAnimation(modelComp);
    }
    if (timeChanged)
    {
        SampleEditPoseFromAnimation(modelComp);
    }

    ImGui::SameLine();
    if (ImGui::Button("Use Current"))
    {
        modelComp->GetNodePoses(editPoseBuffer);
        ApplyEditPoseToModel(modelComp);
        poseEditPoseDirty = false;
    }

    ImGui::SeparatorText("Node Edit");
    const auto& nodes = modelComp->GetNodes();
    if (editPoseBuffer.size() != nodes.size())
    {
        modelComp->GetNodePoses(editPoseBuffer);
    }

    const float nodeAreaHeight = (std::max)(160.0f, (std::min)(260.0f, ImGui::GetContentRegionAvail().y - 86.0f));

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float nodeListWidth = availableWidth * 0.5f; // 左右で50%ずつ分け合うのだ



    ImGui::BeginChild("PoseEditNodeList", ImVec2(nodeListWidth, nodeAreaHeight), true);
    const char* listModes[] = { "Hierarchy", "Bones", "Flat" };
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::Combo("##PoseEditListMode", &poseEditListMode, listModes, IM_ARRAYSIZE(listModes)))
    {
        showNodeHierarchy = (poseEditListMode == 0);
    }
    ImGui::Separator();

    if (poseEditListMode == 0)
    {
        std::vector<std::vector<int>> children(nodes.size());
        std::vector<int> roots;
        roots.reserve(nodes.size());

        for (int i = 0; i < (int)nodes.size(); ++i)
        {
            const int parent = nodes[i].parentIndex;
            if (parent >= 0 && parent < (int)nodes.size())
            {
                children[parent].push_back(i);
            }
            else
            {
                roots.push_back(i);
            }
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        for (int root : roots)
        {
            DrawNodeHierarchyItem(nodes, children, root, selectedEditNode);
        }
    }
    else if (poseEditListMode == 1)
    {
        const auto& meshes = modelComp->GetMeshes();
        bool hasBone = false;
        for (int meshIndex = 0; meshIndex < (int)meshes.size(); ++meshIndex)
        {
            const auto& mesh = meshes[meshIndex];
            if (mesh.bones.empty())
            {
                continue;
            }

            hasBone = true;
            std::string meshLabel = "Mesh " + std::to_string(meshIndex);
            if (mesh.material)
            {
                meshLabel += " / " + mesh.material->name;
            }

            ImGuiTreeNodeFlags meshFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (ImGui::TreeNodeEx((void*)(intptr_t)(100000 + meshIndex), meshFlags, "%s", meshLabel.c_str()))
            {
                for (int boneIndex = 0; boneIndex < (int)mesh.bones.size(); ++boneIndex)
                {
                    const int nodeIndex = mesh.bones[boneIndex].nodeIndex;
                    if (nodeIndex < 0 || nodeIndex >= (int)nodes.size())
                    {
                        continue;
                    }

                    ImGui::PushID(meshIndex * 10000 + boneIndex);
                    std::string label = std::to_string(boneIndex) + "  " + nodes[nodeIndex].name + "  (node " + std::to_string(nodeIndex) + ")";
                    if (ImGui::Selectable(label.c_str(), selectedEditNode == nodeIndex))
                    {
                        selectedEditNode = nodeIndex;
                    }
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
        }

        if (!hasBone)
        {
            ImGui::TextDisabled("No skin bones found.");
        }
    }
    else
    {
        for (int i = 0; i < (int)nodes.size(); ++i)
        {
            ImGui::PushID(i);
            std::string label = std::to_string(i) + "  " + nodes[i].name;
            if (ImGui::Selectable(label.c_str(), selectedEditNode == i))
            {
                selectedEditNode = i;
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("PoseEditNodeParams", ImVec2(0, nodeAreaHeight), true);
    if (selectedEditNode >= 0 && selectedEditNode < (int)editPoseBuffer.size())
    {
        auto& pose = editPoseBuffer[selectedEditNode];
        ImGui::Text("Selected: %s", nodes[selectedEditNode].name.c_str());
        if (nodes[selectedEditNode].parentIndex >= 0 && nodes[selectedEditNode].parentIndex < (int)nodes.size())
        {
            ImGui::TextDisabled("Parent: %s", nodes[nodes[selectedEditNode].parentIndex].name.c_str());
        }
        else
        {
            ImGui::TextDisabled("Parent: Root");
        }

        int selectedBoneRefCount = 0;
        std::string selectedBoneMeshLabel;
        const auto& selectedMeshes = modelComp->GetMeshes();
        for (int meshIndex = 0; meshIndex < (int)selectedMeshes.size(); ++meshIndex)
        {
            const auto& mesh = selectedMeshes[meshIndex];
            for (const auto& bone : mesh.bones)
            {
                if (bone.nodeIndex == selectedEditNode)
                {
                    ++selectedBoneRefCount;
                    if (selectedBoneMeshLabel.empty())
                    {
                        selectedBoneMeshLabel = "Mesh " + std::to_string(meshIndex);
                        if (mesh.material)
                        {
                            selectedBoneMeshLabel += " / " + mesh.material->name;
                        }
                    }
                }
            }
        }
        if (selectedBoneRefCount > 0)
        {
            ImGui::TextDisabled("Bone: %d ref(s)  %s", selectedBoneRefCount, selectedBoneMeshLabel.c_str());
        }
        else
        {
            ImGui::TextDisabled("Bone: Not used by skin");
        }

        bool changed = false;
        ImGui::SetNextItemWidth((std::max)(120.0f, (std::min)(420.0f, ImGui::GetContentRegionAvail().x - 110.0f)));
        changed |= ImGui::DragFloat3("Position", reinterpret_cast<float*>(&pose.position), 0.01f);
        ImGui::SetNextItemWidth((std::max)(120.0f, (std::min)(420.0f, ImGui::GetContentRegionAvail().x - 110.0f)));
        changed |= ImGui::DragFloat4("Rotation", reinterpret_cast<float*>(&pose.rotation), 0.01f);
        ImGui::SetNextItemWidth((std::max)(120.0f, (std::min)(420.0f, ImGui::GetContentRegionAvail().x - 110.0f)));
        changed |= ImGui::DragFloat3("Scale", reinterpret_cast<float*>(&pose.scale), 0.01f);

        if (changed)
        {
            poseEditPoseDirty = true;
            ApplyEditPoseToModel(modelComp);
        }

        ImGui::Separator();
        ImGui::RadioButton("Move", &gizmoOperation, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Rotate", &gizmoOperation, 1);
        ImGui::SameLine();
        ImGui::RadioButton("Scale", &gizmoOperation, 2);

        DrawSelectedNodeGizmo(modelComp);
    }
    else
    {
        ImGui::TextDisabled("Select a node from the list.");
    }
    ImGui::EndChild();

    ImGui::SeparatorText("Save Edited Pose");
    ImGui::SetNextItemWidth((std::min)(220.0f, ImGui::GetContentRegionAvail().x * 0.35f));
    ImGui::InputText("Pose Name", &editPoseName);
    ImGui::SameLine();
    if (ImGui::Button("Save Pose"))
    {
        auto poseLibrary = animController->GetPoseLibrary();
        if (poseLibrary)
        {
            SavedPose pose;
            pose.name = editPoseName.empty() ? ("EditedPose_" + std::to_string(poseLibrary->poseLibrary.size())) : editPoseName;
            pose.nodePoses = editPoseBuffer;
            pose.sourceAnimIndex = poseEditAnimIndex;
            pose.sourceTime = poseEditTime;
            poseLibrary->poseLibrary.push_back(pose);
            selectedPose = (int)poseLibrary->poseLibrary.size() - 1;
            editPoseName = "EditedPose_" + std::to_string(poseLibrary->poseLibrary.size());
            poseEditPoseDirty = false;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Save Pose Library"))
    {
        auto poseLibrary = animController->GetPoseLibrary();
        if (poseLibrary)
        {
            PoseLibraryResourceManager::Instance().SaveToJson(GetPoseLibraryAssetPath(poseLibrary->modelName), poseLibrary);
        }
    }

    if (poseEditPoseDirty)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("Unsaved edits");
    }
}
void AnimationEditorWindow::DrawEventSequencer(
    GltfModelCommponennt* modelComp,
    AnimationControllerComponent* animController)
{
    AttackTemplateManager& attackTemplateManager_ =
        AttackTemplateManager::Instance();

    auto& states_ = animController->GetStates();
    if (states_.empty())
    {
        ImGui::TextDisabled("No animation states are registered.");
        return;
    }

    if (selectedEventStateName.empty() || !states_.count(selectedEventStateName))
    {
        if (const AnimState* currentState_ = animController->GetCurrentState())
        {
            selectedEventStateName = currentState_->name;
        }

        if (selectedEventStateName.empty() || !states_.count(selectedEventStateName))
        {
            selectedEventStateName = states_.begin()->first;
        }

        selectedEvent = -1;
    }

    ImGui::SetNextItemWidth(240.0f);
    if (ImGui::BeginCombo("Editing State", selectedEventStateName.c_str()))
    {
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputTextWithHint(
            "##EventStateSearch",
            "Search states...",
            &eventStateSearchText);

        ImGui::Separator();

        for (const auto& [stateName_, state_] : states_)
        {
            if (!state_ ||
                !ContainsCaseInsensitive(stateName_, eventStateSearchText))
            {
                continue;
            }

            const bool selected_ = stateName_ == selectedEventStateName;
            if (ImGui::Selectable(stateName_.c_str(), selected_))
            {
                selectedEventStateName = stateName_;
                selectedEvent = -1;
            }

            if (selected_)
            {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    AnimState* targetState_ = states_[selectedEventStateName].get();
    if (!targetState_)
    {
        ImGui::TextDisabled("Selected animation state is invalid.");
        return;
    }

    float duration_ = 1.0f;

    if (targetState_->type == AnimStateType::Custom)
    {
        auto poseLibrary_ = animController->GetPoseLibrary();
        if (poseLibrary_ &&
            targetState_->customAnimIndex >= 0 &&
            targetState_->customAnimIndex < static_cast<int>(poseLibrary_->customAnims.size()))
        {
            duration_ =
                poseLibrary_->customAnims[targetState_->customAnimIndex].duration;
        }
    }
    else if (targetState_->type == AnimStateType::Blend1D &&
        targetState_->animIndexA >= 0)
    {
        duration_ = modelComp->GetAnimationDuration(targetState_->animIndexA);
    }
    else if (targetState_->animationIndex >= 0)
    {
        duration_ =
            modelComp->GetAnimationDuration(targetState_->animationIndex);
    }

    duration_ = (std::max)(0.05f, duration_);

    if (ImGui::Button(animController->IsAnimationPaused() ? "Play" : "Pause"))
    {
        if (animController->IsAnimationPaused())
        {
            const AnimState* currentState_ =
                animController->GetCurrentState();

            if (!currentState_ || currentState_->name != selectedEventStateName)
            {
                animController->ChangeState(selectedEventStateName, 0.0f);
            }

            animController->SetAnimationPaused(false);
        }
        else
        {
            animController->SetAnimationPaused(true);
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Stop"))
    {
        animController->ChangeState(selectedEventStateName, 0.0f);
        animController->SetAnimatorTime(0.0f);
        animController->SetAnimationPaused(true);
    }

    ImGui::SameLine();

    ImGui::Text(
        "Time: %.3f s / %.3f s",
        animController->GetAnimatorTime(),
        duration_);

    AnimSequenceAdapter adapter_;
    adapter_.targetState = targetState_;
    adapter_.duration = duration_;
    adapter_.fps = 60.0f;
    adapter_.SyncFrames();

    if (ImGui::Button("Add Action Event"))
    {
        AnimEvent actionEvent_{};
        actionEvent_.eventLabel = "Action";
        actionEvent_.eventTemplateType =
            ANIMATION_EVENT_TEMPLATE_TYPE::NONE;
        actionEvent_.startTime = animController->GetAnimatorTime();
        actionEvent_.endTime =
            (std::min)(actionEvent_.startTime + 0.2f, adapter_.duration);
        actionEvent_.frameStart = adapter_.ToFrame(actionEvent_.startTime);
        actionEvent_.frameEnd = adapter_.ToFrame(actionEvent_.endTime);
        actionEvent_.callEndOnStateExit = true;

        targetState_->events.push_back(actionEvent_);
        selectedEvent = static_cast<int>(targetState_->events.size()) - 1;
        adapter_.SyncFrames();
    }

    ImGui::SameLine();

    if (ImGui::Button("Add Attack Template Event"))
    {
        attackTemplateManager_.LoadAll();

        std::string newTemplateId_ = "NewAttackTemplate";
        int templateIndex_ = 2;
        while (attackTemplateManager_.FindTemplate(newTemplateId_))
        {
            newTemplateId_ =
                "NewAttackTemplate" + std::to_string(templateIndex_++);
        }

        AttackTemplate newTemplate_{};
        newTemplate_.id = newTemplateId_;
        newTemplate_.displayName = "New Attack Template";
        const bool templateCreated_ =
            attackTemplateManager_.SaveTemplate(newTemplate_);

        if (templateCreated_)
        {
            attackTemplateManager_.LoadAll();
        }

        AnimEvent attackEvent_{};
        attackEvent_.eventLabel = "AttackTemplate";
        attackEvent_.eventTemplateType =
            ANIMATION_EVENT_TEMPLATE_TYPE::ATTACK_TEMPLATE;
        attackEvent_.ownerTarget = "Self";
        attackEvent_.startTime = animController->GetAnimatorTime();
        attackEvent_.endTime =
            (std::min)(attackEvent_.startTime + 0.2f, adapter_.duration);
        attackEvent_.callEndOnStateExit = true;

        if (templateCreated_)
        {
            attackEvent_.attackTemplateId = newTemplateId_;
        }

        targetState_->events.push_back(attackEvent_);
        selectedEvent = static_cast<int>(targetState_->events.size()) - 1;

        adapter_.SyncFrames();
    }

    ImGui::SameLine();

    if (ImGui::Button("Add Mesh VFX Event"))
    {
        AnimEvent vfxEvent_{};
        vfxEvent_.eventLabel = "MeshVFX";
        vfxEvent_.eventTemplateType =
            ANIMATION_EVENT_TEMPLATE_TYPE::VFX_TEMPLATE;
        vfxEvent_.startTime = animController->GetAnimatorTime();
        vfxEvent_.endTime =
            (std::min)(vfxEvent_.startTime + 0.2f, adapter_.duration);
        vfxEvent_.frameStart = adapter_.ToFrame(vfxEvent_.startTime);
        vfxEvent_.frameEnd = adapter_.ToFrame(vfxEvent_.endTime);
        vfxEvent_.meshVFXLifeTime =
            (std::max)(vfxEvent_.endTime - vfxEvent_.startTime, 0.01f);
        vfxEvent_.meshVFXPlaybackSpeed = 1.0f;
        vfxEvent_.localScale = { 1.0f, 1.0f, 1.0f };

        if (gameObjectManager)
        {
            std::vector<std::string> assets_ =
                gameObjectManager->GetEffectManager().GetMeshVFXAssetKeys();
            const std::vector<std::string>& sequenceAssets_ =
                gameObjectManager->GetEffectManager().GetVFXSequenceAssetKeys();
            assets_.insert(
                assets_.end(),
                sequenceAssets_.begin(),
                sequenceAssets_.end());

            if (!assets_.empty())
            {
                vfxEvent_.meshVFXAssetPath = assets_.front();
            }
        }

        targetState_->events.push_back(vfxEvent_);
        selectedEvent = static_cast<int>(targetState_->events.size()) - 1;

        adapter_.SyncFrames();
    }

    ImGui::SameLine();

    if (ImGui::Button("Reload Attack Templates"))
    {
        attackTemplateManager_.LoadAll();
    }

    if (ImGui::Button("Sort by Start Time"))
    {
        std::stable_sort(
            targetState_->events.begin(),
            targetState_->events.end(),
            [](const AnimEvent& left_, const AnimEvent& right_)
            {
                return left_.startTime < right_.startTime;
            });
        selectedEvent = -1;
        adapter_.SyncFrames();
    }

    int currentFrame_ =
        static_cast<int>(animController->GetAnimatorTime() * adapter_.fps);

    const int previousFrame_ = currentFrame_;

    ImGui::PushItemWidth(-1.0f);

    const bool showEventSequencer_ =
        ImGui::BeginChild(
            "InlineEventSequencerRegion",
            ImVec2(0.0f, 300.0f),
            true);

    if (showEventSequencer_)
    {
        if (ImSequencer::Sequencer(
            &adapter_,
            &currentFrame_,
            &eventSequencerExpanded,
            &selectedEvent,
            &firstEventFrame,
            ImSequencer::SEQUENCER_EDIT_ALL |
            ImSequencer::SEQUENCER_ADD |
            ImSequencer::SEQUENCER_DEL |
            ImSequencer::SEQUENCER_CHANGE_FRAME))
        {
            adapter_.EndEdit();
        }
    }

    ImGui::EndChild();
    ImGui::PopItemWidth();

    if (currentFrame_ != previousFrame_)
    {
        const int clampedFrame_ =
            (std::max)(
                adapter_.GetFrameMin(),
                (std::min)(adapter_.GetFrameMax(), currentFrame_));

        animController->ChangeState(selectedEventStateName, 0.0f);
        animController->SetAnimatorTime(adapter_.ToTime(clampedFrame_));
        animController->SetAnimationPaused(true);
    }

    if (selectedEvent < 0 ||
        selectedEvent >= static_cast<int>(targetState_->events.size()))
    {
        ImGui::TextDisabled("Click an event bar or name to edit details.");
        return;
    }

    if (!ImGui::BeginChild("InlineEventDetails", ImVec2(0.0f, 0.0f), true))
    {
        ImGui::EndChild();
        return;
    }

    AnimEvent& ev_ = targetState_->events[selectedEvent];

    ImGui::TextColored(
        ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
        "Editing Event #%d",
        selectedEvent);

    ImGui::SameLine();

    ImGui::PushStyleColor(
        ImGuiCol_Button,
        ImVec4(0.6f, 0.1f, 0.1f, 1.0f));

    ImGui::PushStyleColor(
        ImGuiCol_ButtonHovered,
        ImVec4(0.8f, 0.1f, 0.1f, 1.0f));

    if (ImGui::Button("Delete Event"))
    {
        adapter_.Del(selectedEvent);
        selectedEvent = -1;

        ImGui::PopStyleColor(2);
        ImGui::EndChild();
        return;
    }

    ImGui::PopStyleColor(2);

    ImGui::SameLine();

    if (ImGui::Button("Deselect"))
    {
        selectedEvent = -1;
        ImGui::EndChild();
        return;
    }

    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputText("Display Label", &ev_.eventLabel);

    const bool isAttackTemplateEvent_ =
        ev_.eventTemplateType ==
        ANIMATION_EVENT_TEMPLATE_TYPE::ATTACK_TEMPLATE;
    const bool isVFXEvent_ =
        ev_.eventTemplateType ==
        ANIMATION_EVENT_TEMPLATE_TYPE::VFX_TEMPLATE;

    // 従来の有効化Actionにも予兆設定を保存でき、Templateイベントの設定とは分けて編集する。
    if (!isAttackTemplateEvent_) ImGui::Checkbox("Telegraph Hit Start", &ev_.telegraphHitStart);
    if (!isAttackTemplateEvent_ && (ev_.telegraphHitStart || (ev_.startAction.find("Enable") != std::string::npos &&
        ev_.startAction.find("HitBox") != std::string::npos)))
        AttackTelegraphRuntime::DrawSettings(ev_.telegraph, gameObjectManager, modelComp->GetGameObject());

    if (isAttackTemplateEvent_)
    {
        ImGui::SeparatorText("Attack Template Event");

        if (attackTemplateManager_.GetTemplates().empty())
        {
            attackTemplateManager_.LoadAll();
        }

        const char* preview_ = ev_.attackTemplateId.empty()
            ? "(Select Template)"
            : ev_.attackTemplateId.c_str();

        ImGui::SetNextItemWidth(280.0f);
        if (ImGui::BeginCombo("Attack Template", preview_))
        {
            for (const AttackTemplate& attackTemplate_ :
                attackTemplateManager_.GetTemplates())
            {
                const bool selected_ =
                    ev_.attackTemplateId == attackTemplate_.id;

                std::string label_ = attackTemplate_.displayName.empty()
                    ? attackTemplate_.id
                    : attackTemplate_.displayName;

                if (label_ != attackTemplate_.id)
                {
                    label_ += "##" + attackTemplate_.id;
                }

                if (ImGui::Selectable(label_.c_str(), selected_))
                {
                    ev_.attackTemplateId = attackTemplate_.id;
                    attackTemplateManager_.OpenEditor(ev_.attackTemplateId);
                }

                if (selected_)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        ImGui::SameLine();

        if (ImGui::Button("Reload Templates"))
        {
            attackTemplateManager_.LoadAll();
        }

        const char* ownerTargets_[] =
        {
            "Self",
            "Owner",
            "Weapon",
            "Target",
            "LockOnTarget"
        };

        int ownerTargetIndex_ = 0;
        for (int index_ = 0; index_ < IM_ARRAYSIZE(ownerTargets_); ++index_)
        {
            if (ev_.ownerTarget == ownerTargets_[index_])
            {
                ownerTargetIndex_ = index_;
                break;
            }
        }

        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::Combo(
            "Owner Target",
            &ownerTargetIndex_,
            ownerTargets_,
            IM_ARRAYSIZE(ownerTargets_)))
        {
            ev_.ownerTarget = ownerTargets_[ownerTargetIndex_];
        }

        if (ImGui::Button("Duplicate Attack Template") &&
            !ev_.attackTemplateId.empty())
        {
            const AttackTemplate* sourceTemplate_ =
                attackTemplateManager_.FindTemplate(ev_.attackTemplateId);

            if (sourceTemplate_)
            {
                std::string newId_ = ev_.attackTemplateId + "_Copy";
                int copyIndex_ = 2;

                while (attackTemplateManager_.FindTemplate(newId_))
                {
                    newId_ =
                        ev_.attackTemplateId +
                        "_Copy" +
                        std::to_string(copyIndex_++);
                }

                if (attackTemplateManager_.DuplicateTemplate(
                    *sourceTemplate_,
                    newId_))
                {
                    ev_.attackTemplateId = newId_;
                    attackTemplateManager_.OpenEditor(newId_);
                }
            }
        }

        ImGui::TextDisabled(
            "Damage / HitStop / VFX / HitBox are edited in the template below.");

        ImGui::Separator();

        if (ev_.attackTemplateId.empty())
        {
            ImGui::TextDisabled("Select an AttackTemplate to edit.");
        }
        else
        {
            ImGui::SeparatorText("Hit Effect Preview");
            if (ImGui::Button("Play Hit Effect"))
            {
                PlayAttackTemplateEffectPreview(ev_.attackTemplateId);
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop Hit Effect"))
            {
                loopAttackTemplateEffectPreview = false;
                StopAttackTemplateEffectPreview();
            }
            ImGui::SameLine();
            if (ImGui::Checkbox("Loop Hit Effect", &loopAttackTemplateEffectPreview))
            {
                if (loopAttackTemplateEffectPreview)
                {
                    PlayAttackTemplateEffectPreview(ev_.attackTemplateId);
                }
                else
                {
                    StopAttackTemplateEffectPreview();
                }
            }
            if (loopAttackTemplateEffectPreview)
            {
                attackTemplateEffectPreviewTimer += ImGui::GetIO().DeltaTime;
                if (attackTemplateEffectPreviewTimer >= attackTemplateEffectPreviewDuration)
                {
                    PlayAttackTemplateEffectPreview(ev_.attackTemplateId);
                }
            }
            if (ImGui::CollapsingHeader(
                "Inline Attack Template Editor",
                ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::BeginChild(
                    "InlineAttackTemplateEditor",
                    ImVec2(0.0f, 520.0f),
                    true);

                attackTemplateManager_.DrawInlineEditor(
                    ev_.attackTemplateId,
                    gameObjectManager);

                ImGui::EndChild();
            }
        }
    }

    if (isVFXEvent_)
    {
        ImGui::SeparatorText("Mesh VFX Event");

        if (ImGui::Button("Play Effect"))
        {
            PlaySelectedEffectPreview(ev_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop Effect"))
        {
            loopSelectedEffectPreview = false;
            StopSelectedEffectPreview();
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("Loop Preview", &loopSelectedEffectPreview))
        {
            if (loopSelectedEffectPreview) PlaySelectedEffectPreview(ev_);
            else StopSelectedEffectPreview();
        }

        if (loopSelectedEffectPreview)
        {
            selectedEffectPreviewTimer += ImGui::GetIO().DeltaTime;
            const float previewDuration_ =
                (std::max)(ev_.meshVFXLifeTime, 0.01f);
            const std::shared_ptr<GameObject> previewObject_ =
                selectedEffectPreviewObject.lock();
            const std::shared_ptr<MeshVFXComponent> previewMeshVFX_ =
                previewObject_
                ? previewObject_->GetComponent<MeshVFXComponent>()
                : nullptr;
            if (!previewMeshVFX_ ||
                !previewMeshVFX_->IsEnabled() ||
                selectedEffectPreviewTimer >= previewDuration_)
            {
                PlaySelectedEffectPreview(ev_);
            }
        }

        bool effectPreviewChanged_ = false;

        if (gameObjectManager)
        {
            std::vector<std::string> assets_ =
                gameObjectManager->GetEffectManager().GetMeshVFXAssetKeys();
            const std::vector<std::string>& sequenceAssets_ =
                gameObjectManager->GetEffectManager().GetVFXSequenceAssetKeys();
            assets_.insert(
                assets_.end(),
                sequenceAssets_.begin(),
                sequenceAssets_.end());
            std::string previewValue_ = ev_.meshVFXAssetPath;
            const std::string resolvedValue_ =
                gameObjectManager->GetEffectManager().ResolveMeshVFXPath(
                    ev_.meshVFXAssetPath);
            for (const std::string& assetKey_ : assets_)
            {
                if (!resolvedValue_.empty() &&
                    resolvedValue_ ==
                    gameObjectManager->GetEffectManager().ResolveMeshVFXPath(assetKey_))
                {
                    previewValue_ = assetKey_;
                    break;
                }
            }

            ImGui::SetNextItemWidth(360.0f);
            if (ImGui::BeginCombo(
                "Mesh VFX / Sequence Asset",
                previewValue_.empty()
                ? "(None)"
                : previewValue_.c_str()))
            {
                if (ImGui::Selectable(
                    "(None)",
                    ev_.meshVFXAssetPath.empty()))
                {
                    ev_.meshVFXAssetPath.clear();
                    effectPreviewChanged_ = true;
                }

                for (const std::string& assetKey_ : assets_)
                {
                    const bool selected_ = previewValue_ == assetKey_;
                    if (ImGui::Selectable(
                        assetKey_.c_str(),
                        selected_))
                    {
                        ev_.meshVFXAssetPath = assetKey_;
                        effectPreviewChanged_ = true;
                    }
                    if (selected_) ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }
        }

        ImGui::SetNextItemWidth(360.0f);
        effectPreviewChanged_ |= ImGui::InputText("Mesh VFX Asset Path", &ev_.meshVFXAssetPath);

        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::BeginCombo(
            "Attach Node",
            ev_.attachNodeName.empty()
            ? "(Owner Root)"
            : ev_.attachNodeName.c_str()))
        {
            if (ImGui::Selectable(
                "(Owner Root)",
                ev_.attachNodeName.empty()))
            {
                ev_.attachNodeName.clear();
                effectPreviewChanged_ = true;
            }

            if (modelComp)
            {
                for (const auto& node_ : modelComp->GetNodes())
                {
                    if (node_.name.empty())
                    {
                        continue;
                    }

                    if (ImGui::Selectable(
                        node_.name.c_str(),
                        ev_.attachNodeName == node_.name))
                    {
                        ev_.attachNodeName = node_.name;
                        effectPreviewChanged_ = true;
                    }
                }
            }

            ImGui::EndCombo();
        }

        ImGui::SameLine();
        effectPreviewChanged_ |= ImGui::Checkbox("Attach To Owner", &ev_.attachToOwner);

        ImGui::SameLine();
        effectPreviewChanged_ |= ImGui::Checkbox("Mirror", &ev_.mirrorMeshVFX);

        effectPreviewChanged_ |= ImGui::DragFloat3("Local Position", &ev_.localPosition.x, 0.01f);
        effectPreviewChanged_ |= ImGui::DragFloat3("Local Rotation", &ev_.localRotation.x, 0.5f);
        effectPreviewChanged_ |= ImGui::DragFloat3("Local Scale", &ev_.localScale.x, 0.01f, 0.001f, 100.0f);
        effectPreviewChanged_ |= ImGui::DragFloat("Life Time", &ev_.meshVFXLifeTime, 0.01f, 0.01f, 60.0f);
        effectPreviewChanged_ |= ImGui::DragFloat(
            "Playback Speed",
            &ev_.meshVFXPlaybackSpeed,
            0.01f,
            0.0f,
            10.0f,
            "%.2fx");
        if (loopSelectedEffectPreview && effectPreviewChanged_)
        {
            PlaySelectedEffectPreview(ev_);
        }

    }

    ImGui::Separator();

    auto selectedObject_ =
        GetPreviewManager()
        ? GetPreviewManager()->GetSelectedObject()
        : nullptr;

    ActionRegistry& globalReg_ = ActionRegistry::Instance();

    struct CategoryEntry
    {
        std::string category;
        std::string menuLabel;
        ActionRegistry* registry = nullptr;
    };

    std::vector<CategoryEntry> allCategories_;

    auto addRegistryCategories_ =
        [&](ActionRegistry& registry_, const std::string& sourceLabel_)
        {
            for (const auto& category_ : registry_.GetCategories())
            {
                allCategories_.push_back(
                    {
                        category_,
                        sourceLabel_ + " " + category_,
                        &registry_
                    });
            }
        };

    if (selectedObject_)
    {
        addRegistryCategories_(selectedObject_->localRegistry, "[Self]");

        auto parentTransform_ =
            selectedObject_->transform
            ? selectedObject_->transform->parent.lock()
            : nullptr;

        while (parentTransform_)
        {
            auto parentObject_ = parentTransform_->gameObject.lock();
            if (!parentObject_)
            {
                break;
            }

            addRegistryCategories_(
                parentObject_->localRegistry,
                "[Parent:" + parentObject_->GetName() + "]");

            parentTransform_ =
                parentObject_->transform
                ? parentObject_->transform->parent.lock()
                : nullptr;
        }

        auto collectChildActions_ =
            [&](auto&& self_, const std::shared_ptr<GameObject>& object_) -> void
            {
                if (!object_ || !object_->transform)
                {
                    return;
                }

                for (const auto& childTransform_ : object_->transform->children)
                {
                    if (!childTransform_)
                    {
                        continue;
                    }

                    auto childObject_ =
                        childTransform_->gameObject.lock();

                    if (!childObject_)
                    {
                        continue;
                    }

                    addRegistryCategories_(
                        childObject_->localRegistry,
                        "[Child:" + childObject_->GetName() + "]");

                    self_(self_, childObject_);
                }
            };

        collectChildActions_(collectChildActions_, selectedObject_);
    }

    addRegistryCategories_(globalReg_, "[Global]");

    auto actionField_ =
        [&](const char* label_, std::string& targetStr_)
        {
            ImGui::SetNextItemWidth(260.0f);
            ImGui::InputText(label_, &targetStr_);

            ImGui::SameLine();

            std::string popupName_ =
                std::string("ActionSelect##Inline") + label_;

            if (ImGui::Button(
                (std::string("Select##Inline") + label_).c_str()))
            {
                ImGui::OpenPopup(popupName_.c_str());
            }

            if (ImGui::BeginPopup(popupName_.c_str()))
            {
                if (ImGui::MenuItem("Clear"))
                {
                    targetStr_.clear();
                }

                ImGui::Separator();

                for (const CategoryEntry& entry_ : allCategories_)
                {
                    if (ImGui::BeginMenu(entry_.menuLabel.c_str()))
                    {
                        std::vector<std::string> actionNames_;

                        if (entry_.registry)
                        {
                            actionNames_ =
                                entry_.registry->GetActionNames(entry_.category);
                        }

                        for (const std::string& actionName_ : actionNames_)
                        {
                            if (ImGui::MenuItem(actionName_.c_str()))
                            {
                                targetStr_ = actionName_;
                            }
                        }

                        ImGui::EndMenu();
                    }
                }

                ImGui::EndPopup();
            }
        };

    if (!isVFXEvent_)
    {
        ImGui::SeparatorText("Actions");

        actionField_("Start Action", ev_.startAction);
        actionField_("Update Action", ev_.updateAction);
        actionField_("End Action", ev_.endAction);
    }

    const bool showBlackboardWrites_ =
        (!isAttackTemplateEvent_ && !isVFXEvent_) ||
        ImGui::CollapsingHeader("Advanced Blackboard Writes");

    if (showBlackboardWrites_)
    {
        ImGui::SeparatorText("Blackboard Writes");

        int removeWriteIndex_ = -1;

        for (int writeIndex_ = 0;
            writeIndex_ < static_cast<int>(ev_.blackboardWrites.size());
            ++writeIndex_)
        {
            AnimBlackboardWrite& write_ =
                ev_.blackboardWrites[writeIndex_];

            ImGui::PushID(writeIndex_);

            ImGui::InputText("Category", &write_.category);
            ImGui::InputText("Key", &write_.key);

            int typeIndex_ = static_cast<int>(write_.type);
            const char* typeNames_[] =
            {
                "Float",
                "Int",
                "Bool",
                "String"
            };

            if (ImGui::Combo(
                "Type",
                &typeIndex_,
                typeNames_,
                IM_ARRAYSIZE(typeNames_)))
            {
                write_.type =
                    static_cast<ANIM_BLACKBOARD_VALUE_TYPE>(typeIndex_);
            }

            switch (write_.type)
            {
            case ANIM_BLACKBOARD_VALUE_TYPE::FLOAT:
                ImGui::DragFloat("Value", &write_.floatValue, 0.1f);
                break;

            case ANIM_BLACKBOARD_VALUE_TYPE::INT:
                ImGui::DragInt("Value", &write_.intValue);
                break;

            case ANIM_BLACKBOARD_VALUE_TYPE::BOOL:
                ImGui::Checkbox("Value", &write_.boolValue);
                break;

            case ANIM_BLACKBOARD_VALUE_TYPE::STRING:
            {
                const bool isParticleAsset_ =
                    write_.key == "HitParticleAssetPath";

                const bool isMeshVFXAsset_ =
                    write_.key == "HitMeshVFXAssetPath";

                if ((isParticleAsset_ || isMeshVFXAsset_) &&
                    gameObjectManager)
                {
                    std::vector<std::string> assets_ = isParticleAsset_
                        ? gameObjectManager->GetEffectManager().GetParticleAssetKeys()
                        : gameObjectManager->GetEffectManager().GetMeshVFXAssetKeys();
                    if (isMeshVFXAsset_)
                    {
                        const std::vector<std::string>& sequenceAssets_ =
                            gameObjectManager->GetEffectManager().GetVFXSequenceAssetKeys();
                        assets_.insert(
                            assets_.end(),
                            sequenceAssets_.begin(),
                            sequenceAssets_.end());
                    }
                    std::string previewValue_ = write_.stringValue;
                    const std::string resolvedValue_ = isParticleAsset_
                        ? gameObjectManager->GetEffectManager().ResolveParticlePath(write_.stringValue)
                        : gameObjectManager->GetEffectManager().ResolveMeshVFXPath(write_.stringValue);
                    for (const std::string& assetKey_ : assets_)
                    {
                        const std::string resolvedKey_ = isParticleAsset_
                            ? gameObjectManager->GetEffectManager().ResolveParticlePath(assetKey_)
                            : gameObjectManager->GetEffectManager().ResolveMeshVFXPath(assetKey_);
                        if (!resolvedValue_.empty() && resolvedValue_ == resolvedKey_)
                        {
                            previewValue_ = assetKey_;
                            break;
                        }
                    }

                    if (ImGui::BeginCombo(
                        "Value",
                        previewValue_.empty()
                        ? "(None)"
                        : previewValue_.c_str()))
                    {
                        if (ImGui::Selectable(
                            "(None)",
                            write_.stringValue.empty()))
                        {
                            write_.stringValue.clear();
                        }

                        for (const std::string& assetKey_ : assets_)
                        {
                            const bool selected_ = previewValue_ == assetKey_;
                            if (ImGui::Selectable(
                                assetKey_.c_str(),
                                selected_))
                            {
                                write_.stringValue = assetKey_;
                            }
                            if (selected_) ImGui::SetItemDefaultFocus();
                        }

                        ImGui::EndCombo();
                    }
                }
                else
                {
                    ImGui::InputText("Value", &write_.stringValue);
                }

                break;
            }
            }

            if (ImGui::Button("Remove Write"))
            {
                removeWriteIndex_ = writeIndex_;
            }

            ImGui::Separator();

            ImGui::PopID();
        }

        if (removeWriteIndex_ >= 0)
        {
            ev_.blackboardWrites.erase(
                ev_.blackboardWrites.begin() + removeWriteIndex_);
        }

        auto addAttackWrite_ =
            [&ev_](const char* key_, ANIM_BLACKBOARD_VALUE_TYPE type_)
            {
                AnimBlackboardWrite write_{};
                write_.category = "Attack";
                write_.key = key_;
                write_.type = type_;

                ev_.blackboardWrites.push_back(write_);
            };

        if (!isAttackTemplateEvent_ && !isVFXEvent_)
        {
            if (ImGui::Button("+ Damage"))
            {
                addAttackWrite_(
                    "Damage",
                    ANIM_BLACKBOARD_VALUE_TYPE::FLOAT);
            }

            ImGui::SameLine();

            if (ImGui::Button("+ HitStopDuration"))
            {
                addAttackWrite_(
                    "HitStopDuration",
                    ANIM_BLACKBOARD_VALUE_TYPE::FLOAT);
            }

            ImGui::SameLine();

            if (ImGui::Button("+ HitStopScale"))
            {
                addAttackWrite_(
                    "HitStopScale",
                    ANIM_BLACKBOARD_VALUE_TYPE::FLOAT);
            }

            ImGui::SameLine();

            if (ImGui::Button("+ HitStopLayer"))
            {
                addAttackWrite_(
                    "HitStopLayerMask",
                    ANIM_BLACKBOARD_VALUE_TYPE::INT);
            }

            ImGui::SameLine();

            if (ImGui::Button("+ KnockBack"))
            {
                addAttackWrite_(
                    "KnockBackPower",
                    ANIM_BLACKBOARD_VALUE_TYPE::FLOAT);
            }

            ImGui::SameLine();

            if (ImGui::Button("+ HitParticle"))
            {
                addAttackWrite_(
                    "HitParticleAssetPath",
                    ANIM_BLACKBOARD_VALUE_TYPE::STRING);
            }

            ImGui::SameLine();

            if (ImGui::Button("+ HitMeshVFX"))
            {
                addAttackWrite_(
                    "HitMeshVFXAssetPath",
                    ANIM_BLACKBOARD_VALUE_TYPE::STRING);
            }
        }

        if (ImGui::Button("Add Blackboard Write"))
        {
            ev_.blackboardWrites.push_back(AnimBlackboardWrite{});
        }
    }

    ImGui::Checkbox("Call End On State Exit", &ev_.callEndOnStateExit);

    ImGui::Separator();

    ImGui::PushItemWidth(130.0f);

    if (ImGui::DragFloat(
        "Start (s)",
        &ev_.startTime,
        0.01f,
        0.0f,
        ev_.endTime))
    {
        ev_.frameStart = adapter_.ToFrame(ev_.startTime);
    }

    ImGui::SameLine();

    if (ImGui::DragFloat(
        "End (s)",
        &ev_.endTime,
        0.01f,
        ev_.startTime,
        adapter_.duration))
    {
        ev_.frameEnd = adapter_.ToFrame(ev_.endTime);
    }

    ImGui::PopItemWidth();

    ImGui::EndChild();
}
