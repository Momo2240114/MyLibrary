#include "AnimationEditorWindow.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Camera/CameraManager.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Graphics/Resource/Animation/AnimSequenceAdapter.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/Graphics/Resource/Animation/PoseLibraryResourceManager.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "ImSequencer.h"
#include "ImGuizmo.h"
#include "imgui_internal.h"
#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "SimpleMath.h"
#include <string>
#include <vector>

namespace
{
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
        int selectedPose = -1;
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
            return "Missing Pose";
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
            if (!anim || !poseLibrary || poseLibrary->poseLibrary.empty()) return;

            PoseKeyframe key;
            key.poseIndex = (selectedPose >= 0 && selectedPose < (int)poseLibrary->poseLibrary.size()) ? selectedPose : 0;
            key.sourceAnimIdx = poseLibrary->poseLibrary[key.poseIndex].sourceAnimIndex;
            key.sourceTime = poseLibrary->poseLibrary[key.poseIndex].sourceTime;
            key.time = anim->keyframes.empty() ? 0.0f : (std::min)(anim->duration, anim->keyframes.back().time + 0.2f);
            anim->keyframes.push_back(key);
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

bool AnimationEditorWindow::ResolveTarget(GltfModelCommponennt*& modelComp, AnimationControllerComponent*& animController) const
{
    modelComp = nullptr;
    animController = nullptr;

    if (!gameObjectManager) return false;

    auto _SelectedObject = gameObjectManager->GetSelectedObject();
    if (!_SelectedObject) return false;

    auto model = _SelectedObject->GetComponent<GltfModelCommponennt>();
    auto animation = _SelectedObject->GetComponent<AnimationControllerComponent>();
    if (!model || !animation) return false;

    modelComp = model.get();
    animController = animation.get();
    return true;
}

void AnimationEditorWindow::OnVisibilityChanged(bool visible)
{
    if (visible || !poseEditEnabled) return;

    GltfModelCommponennt* modelComp = nullptr;
    AnimationControllerComponent* animController = nullptr;
    if (ResolveTarget(modelComp, animController))
    {
        animController->SetEditorPoseOverride(false);
    }
    poseEditEnabled = false;
}

void AnimationEditorWindow::Draw()
{
    GltfModelCommponennt* modelComp = nullptr;
    AnimationControllerComponent* animController = nullptr;

    ImGui::SetNextWindowSize(ImVec2(760, 500), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(520, 340), ImVec2(1100, 760));
    if (!BeginEditorWindow())
    {
        EndEditorWindow();
        return;
    }

    DrawStylePopup();

    if (!ResolveTarget(modelComp, animController))
    {
        ImGui::TextWrapped("Select a GameObject that has both GltfModel and AnimationControllerComponent.");
        EndEditorWindow();
        return;
    }

    bool poseEditTabActive = false;
    if (ImGui::BeginTabBar("AnimationEditorTabs"))
    {
        if (ImGui::BeginTabItem("Pose Creator"))
        {
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

        if (ImGui::BeginTabItem("Current Pose Preview"))
        {
            if (poseEditEnabled)
            {
                poseEditEnabled = false;
                animController->SetEditorPoseOverride(false);
            }
            DrawPosePreview(modelComp, animController);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    if (!poseEditTabActive && poseEditEnabled)
    {
        poseEditEnabled = false;
        animController->SetEditorPoseOverride(false);
    }

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

    if (ImGui::BeginChild("PoseLibraryList", ImVec2(0, 115), true))
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
        ImGui::EndChild();
    }

    ImGui::SeparatorText("Custom Animation");
    ImGui::SetNextItemWidth(180.0f);
    ImGui::InputText("Animation Name", &newCustomName);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::DragFloat("Duration", &newCustomDuration, 0.05f, 0.05f, 60.0f);
    ImGui::SameLine();
    if (ImGui::Button("Create Custom Animation"))
    {
        CustomAnimation anim;
        anim.name = newCustomName.empty() ? ("CustomAnim_" + std::to_string(poseLibrary->customAnims.size())) : newCustomName;
        anim.duration = (std::max)(0.05f, newCustomDuration);
        poseLibrary->customAnims.push_back(anim);
        selectedCustom = (int)poseLibrary->customAnims.size() - 1;
        selectedKeyframe = -1;
        newCustomName = "CustomAnim_" + std::to_string(poseLibrary->customAnims.size());
    }

    if (poseLibrary->customAnims.empty())
    {
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
    }

    CustomAnimation& anim = poseLibrary->customAnims[selectedCustom];
    ImGui::InputText("Name##CustomAnim", &anim.name);
    ImGui::DragFloat("Duration##CustomAnim", &anim.duration, 0.05f, 0.05f, 60.0f);

    if (ImGui::Button("Create/Update State"))
    {
        if (!animController->GetStates().count(anim.name))
        {
            animController->AddState(anim.name, 0);
        }

        auto it = animController->GetStates().find(anim.name);
        if (it != animController->GetStates().end())
        {
            it->second->type = AnimStateType::Custom;
            it->second->customAnimIndex = selectedCustom;
            it->second->isLoop = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Play State"))
    {
        animController->ChangeState(anim.name, 0.2f);
    }

    DrawPoseKeyframeSequencer(anim, *poseLibrary, animController->GetAnimatorTime());

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

void AnimationEditorWindow::DrawPoseKeyframeSequencer(CustomAnimation& anim, PoseLibraryResource& poseLibrary, float currentTime)
{
    ImGui::SeparatorText("Pose Keyframe Sequencer");

    PoseKeyframeSequenceAdapter adapter;
    adapter.anim = &anim;
    adapter.poseLibrary = &poseLibrary;
    adapter.selectedPose = selectedPose;
    adapter.SyncFrames();

    int currentFrame = static_cast<int>(currentTime * 60.0f);
    if (ImGui::BeginChild("PoseKeyframeSequencerRegion", ImVec2(0, 230), true))
    {
        if (ImSequencer::Sequencer(&adapter, &currentFrame, &poseSequencerExpanded, &selectedKeyframe, &firstPoseFrame,
            ImSequencer::SEQUENCER_EDIT_ALL | ImSequencer::SEQUENCER_ADD | ImSequencer::SEQUENCER_DEL))
        {
            adapter.EndEdit();
        }
        ImGui::EndChild();
    }

    if (ImGui::Button("Sort Keyframes"))
    {
        std::sort(anim.keyframes.begin(), anim.keyframes.end(), [](const PoseKeyframe& a, const PoseKeyframe& b) { return a.time < b.time; });
    }

    if (selectedKeyframe < 0 || selectedKeyframe >= (int)anim.keyframes.size())
    {
        ImGui::TextDisabled("Select a keyframe on the sequencer to edit its pose and interpolation.");
        return;
    }

    PoseKeyframe& key = anim.keyframes[selectedKeyframe];
    ImGui::SeparatorText("Selected Keyframe");
    ImGui::DragFloat("Time", &key.time, 0.01f, 0.0f, anim.duration);

    std::string posePreview = "None";
    if (key.poseIndex >= 0 && key.poseIndex < (int)poseLibrary.poseLibrary.size())
    {
        posePreview = poseLibrary.poseLibrary[key.poseIndex].name;
    }

    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::BeginCombo("Pose", posePreview.c_str()))
    {
        for (int p = 0; p < (int)poseLibrary.poseLibrary.size(); ++p)
        {
            if (ImGui::Selectable(poseLibrary.poseLibrary[p].name.c_str(), key.poseIndex == p))
            {
                key.poseIndex = p;
                key.sourceAnimIdx = poseLibrary.poseLibrary[p].sourceAnimIndex;
                key.sourceTime = poseLibrary.poseLibrary[p].sourceTime;
            }
        }
        ImGui::EndCombo();
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
    if (!gameObjectManager || selectedEditNode < 0 || selectedEditNode >= (int)modelComp->GetNodes().size())
    {
        return;
    }

    auto* cameraManager = gameObjectManager->GetCameraManager();
    if (!cameraManager) return;

    auto camera = cameraManager->GetActiveCamera();
    if (!camera) return;

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
    if (ImGui::BeginChild("PoseEditAnimationScrubSequencer", ImVec2(0, 62), true))
    {
        ImSequencer::Sequencer(&scrubAdapter, &scrubFrame, &editScrubExpanded, &selectedScrubEntry, &firstEditFrame,
            ImSequencer::SEQUENCER_CHANGE_FRAME);
        ImGui::EndChild();
    }
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
    const float nodeListWidth = availableWidth * 0.5f; // ç∂âEÇ≈50%Ç∏Ç¬ï™ÇØçáÇ§ÇÃÇæ



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

void AnimationEditorWindow::DrawEventSequencer(GltfModelCommponennt* modelComp, AnimationControllerComponent* animController)
{
    auto& states = animController->GetStates();
    if (states.empty())
    {
        ImGui::TextDisabled("No animation states are registered.");
        return;
    }

    if (selectedEventStateName.empty() || !states.count(selectedEventStateName))
    {
        if (const AnimState* current = animController->GetCurrentState())
        {
            selectedEventStateName = current->name;
        }
        if (selectedEventStateName.empty() || !states.count(selectedEventStateName))
        {
            selectedEventStateName = states.begin()->first;
        }
        selectedEvent = -1;
    }

    std::vector<const char*> stateNames;
    std::vector<std::string> stateNameStorage;
    stateNameStorage.reserve(states.size());
    int selectedStateIndex = 0;
    int index = 0;
    for (auto& [name, state] : states)
    {
        stateNameStorage.push_back(name);
        if (name == selectedEventStateName) selectedStateIndex = index;
        ++index;
    }
    for (auto& name : stateNameStorage)
    {
        stateNames.push_back(name.c_str());
    }

    ImGui::SetNextItemWidth(240.0f);
    if (ImGui::Combo("Editing State", &selectedStateIndex, stateNames.data(), (int)stateNames.size()))
    {
        selectedEventStateName = stateNameStorage[selectedStateIndex];
        selectedEvent = -1;
    }

    AnimState* targetState = states[selectedEventStateName].get();
    float duration = 1.0f;
    if (targetState->type == AnimStateType::Custom)
    {
        auto poseLibrary = animController->GetPoseLibrary();
        if (poseLibrary &&
            targetState->customAnimIndex >= 0 &&
            targetState->customAnimIndex < (int)poseLibrary->customAnims.size())
        {
            duration = poseLibrary->customAnims[targetState->customAnimIndex].duration;
        }
    }
    else if (targetState->type == AnimStateType::Blend1D && targetState->animIndexA >= 0)
    {
        duration = modelComp->GetAnimationDuration(targetState->animIndexA);
    }
    else if (targetState->animationIndex >= 0)
    {
        duration = modelComp->GetAnimationDuration(targetState->animationIndex);
    }

    AnimSequenceAdapter adapter;
    adapter.targetState = targetState;
    adapter.duration = (std::max)(0.05f, duration);
    adapter.fps = 60.0f;

    int currentFrame = (int)(animController->GetAnimatorTime() * adapter.fps);

    ImGui::PushItemWidth(-1);
    if (ImGui::BeginChild("InlineEventSequencerRegion", ImVec2(0, 300), true))
    {
        if (ImSequencer::Sequencer(&adapter, &currentFrame, &eventSequencerExpanded, &selectedEvent, &firstEventFrame,
            ImSequencer::SEQUENCER_EDIT_ALL | ImSequencer::SEQUENCER_ADD | ImSequencer::SEQUENCER_DEL))
        {
            adapter.EndEdit();
        }
        ImGui::EndChild();
    }
    ImGui::PopItemWidth();

    if (selectedEvent < 0 || selectedEvent >= (int)targetState->events.size())
    {
        ImGui::TextDisabled("Click an event bar or name to edit details.");
        return;
    }

    if (!ImGui::BeginChild("InlineEventDetails", ImVec2(0, 0), true))
    {
        ImGui::EndChild();
        return;
    }

    auto& ev = targetState->events[selectedEvent];
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Editing Event #%d", selectedEvent);
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
    if (ImGui::Button("Delete Event"))
    {
        adapter.Del(selectedEvent);
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
    ImGui::InputText("Display Label", &ev.eventLabel);

    ImGui::Separator();

    auto _SelectedObject = gameObjectManager ? gameObjectManager->GetSelectedObject() : nullptr;
    auto& globalReg = ActionRegistry::Instance();

    struct CategoryEntry { std::string name; bool isLocal; };
    std::vector<CategoryEntry> allCategories;

    if (_SelectedObject)
    {
        for (const auto& cat : _SelectedObject->localRegistry.GetCategories())
        {
            allCategories.push_back({ cat, true });
        }
    }
    for (const auto& cat : globalReg.GetCategories())
    {
        allCategories.push_back({ cat, false });
    }

    auto actionField = [&](const char* label, std::string& targetStr)
        {
            ImGui::SetNextItemWidth(260.0f);
            ImGui::InputText(label, &targetStr);

            ImGui::SameLine();
            std::string popupName = std::string("ActionSelect##Inline") + label;
            if (ImGui::Button((std::string("Select##Inline") + label).c_str()))
            {
                ImGui::OpenPopup(popupName.c_str());
            }

            if (ImGui::BeginPopup(popupName.c_str()))
            {
                if (ImGui::MenuItem("Clear"))
                {
                    targetStr.clear();
                }
                ImGui::Separator();

                for (const auto& entry : allCategories)
                {
                    std::string menuLabel = (entry.isLocal ? "[L] " : "[G] ") + entry.name;
                    if (ImGui::BeginMenu(menuLabel.c_str()))
                    {
                        std::vector<std::string> actionNames;
                        if (entry.isLocal && _SelectedObject)
                        {
                            actionNames = _SelectedObject->localRegistry.GetActionNames(entry.name);
                        }
                        else
                        {
                            actionNames = globalReg.GetActionNames(entry.name);
                        }

                        for (const auto& actionName : actionNames)
                        {
                            if (ImGui::MenuItem(actionName.c_str()))
                            {
                                targetStr = actionName;
                            }
                        }
                        ImGui::EndMenu();
                    }
                }
                ImGui::EndPopup();
            }
        };

    actionField("Start Action", ev.startAction);
    actionField("Update Action", ev.updateAction);
    actionField("End Action", ev.endAction);

    ImGui::Separator();
    ImGui::PushItemWidth(130.0f);
    if (ImGui::DragFloat("Start (s)", &ev.startTime, 0.01f, 0.0f, ev.endTime))
    {
        ev.frameStart = adapter.ToFrame(ev.startTime);
    }
    ImGui::SameLine();
    if (ImGui::DragFloat("End (s)", &ev.endTime, 0.01f, ev.startTime, adapter.duration))
    {
        ev.frameEnd = adapter.ToFrame(ev.endTime);
    }
    ImGui::PopItemWidth();

    ImGui::EndChild();
}

void AnimationEditorWindow::DrawPosePreview(GltfModelCommponennt* modelComp, AnimationControllerComponent* animController)
{
    ImGui::Text("Model: %s", modelComp->GetModelName().c_str());
    if (const AnimState* state = animController->GetCurrentState())
    {
        ImGui::Text("State: %s", state->name.c_str());
        ImGui::Text("Time: %.3f", animController->GetAnimatorTime());
    }
    else
    {
        ImGui::TextDisabled("No animation state is playing.");
    }

    ID3D11ShaderResourceView* sceneSRV = GraphicsDeviceManager::Instance().GetSceneSRV();
    if (!sceneSRV)
    {
        ImGui::TextDisabled("Scene texture is not ready.");
        return;
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    const float width = (std::max)(160.0f, avail.x);
    const float height = (std::max)(120.0f, width * 0.5625f);
    ImGui::Image((void*)sceneSRV, ImVec2(width, height));
}
