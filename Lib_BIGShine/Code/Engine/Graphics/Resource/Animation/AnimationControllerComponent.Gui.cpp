// í èÌ Inspector GUI / Sequencer GUI

#include "AnimationControllerComponent.h"
#include "AnimSequenceAdapter.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "AnimationDataAsset.h"
#include "PoseLibraryResourceManager.h"
#include "IKMapComponent.h"
#include "Editor/AssetSlotGui.h"
#include "Engine/Platform/WindowsDialogHelper.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <cctype>
#include <unordered_map>

namespace ed = ax::NodeEditor;

namespace
{
    bool ContainsCaseInsensitive(const std::string& text_, const std::string& query_)
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
}

void AnimationControllerComponent::Gui()
{
    if (!modelComponent) return;

    ImGui::Text("Model: %s", modelComponent->GetModelName().c_str());
    ImGui::TextDisabled("Animation editors are opened from the main Window menu.");


    ImGui::Text(
        "RootMotion %.3f %.3f %.3f",
        rootMotionDelta.x,
        rootMotionDelta.y,
        rootMotionDelta.z);

    ImGui::SeparatorText("State Assets");
    if (AssetSlotGui::Draw(
        "Import State",
        AssetSlotType::JSON,
        stateAssetImportPath,
        nullptr,
        "Data/Animation/StateData"))
    {
        if (!stateAssetImportPath.empty())
        {
            const bool loaded_ = LoadStateAsset(stateAssetImportPath);
            stateAssetStatus = loaded_
                ? "Loaded: " + stateAssetImportPath
                : "Not an AnimationState asset: " + stateAssetImportPath;
        }
    }
    if (!stateAssetStatus.empty())
    {
        ImGui::TextWrapped("%s", stateAssetStatus.c_str());
    }

    ImGui::Checkbox("useTransitionGraph", &useTransitionGraph);

    ImGui::Separator();


    std::vector<std::string> curveNames;
    if (curveComponent) {
        curveNames = curveComponent->GetTrackNames();
    }


    if (ImGui::CollapsingHeader("Add State From Model Clip", ImGuiTreeNodeFlags_None))
    {
        static int selectedAnimIndex_ = -1;
        const auto& animations_ = modelComponent->GetAnimations();

        ImGui::BeginChild("ModelAnimationClips", ImVec2(0.0f, 140.0f), true);
        for (int index_ = 0; index_ < static_cast<int>(animations_.size()); ++index_)
        {
            if (ImGui::Selectable(
                animations_[index_].name.c_str(),
                selectedAnimIndex_ == index_))
            {
                selectedAnimIndex_ = index_;
            }
        }
        ImGui::EndChild();

        if (selectedAnimIndex_ >= 0 &&
            selectedAnimIndex_ < static_cast<int>(animations_.size()) &&
            ImGui::Button("Create State From Selected Clip", ImVec2(-1.0f, 0.0f)))
        {
            std::string stateName_ = animations_[selectedAnimIndex_].name;
            const std::string baseName_ = stateName_;
            int suffix_ = 2;
            while (states.find(stateName_) != states.end())
            {
                stateName_ = baseName_ + "_" + std::to_string(suffix_++);
            }
            AddState(stateName_, selectedAnimIndex_);
            editingState = states[stateName_].get();
        }
    }

    ImGui::Separator();


    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Registered Animation States");

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));

    if (ImGui::BeginChild("StatesRegion", ImVec2(0, 200), true))
    {
        std::string targetToDelete = "";

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##StateSearch", "Search states...", &stateSearchText);
        ImGui::Separator();

        for (auto& [name, statePtr] : states)
        {
            if (!ContainsCaseInsensitive(name, stateSearchText)) continue;

            ImGui::PushID(name.c_str());

            bool isCurrent = (animator.GetCurrentState() == statePtr.get());
            bool isEditing = (editingState == statePtr.get());


            if (isCurrent) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.4f, 0.2f, 1.0f));
            }



            if (ImGui::Selectable(name.c_str(), isEditing, ImGuiSelectableFlags_AllowOverlap)) {
                editingState = statePtr.get();
            }

            if (isCurrent) ImGui::PopStyleColor();


            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 150.0f);
            if (ImGui::SmallButton("Save"))
            {
                const auto pathIt_ = stateAssetPaths.find(name);
                const std::string path_ = pathIt_ != stateAssetPaths.end()
                    ? pathIt_->second
                    : std::string();
                stateAssetStatus = SaveStateAsset(name, path_)
                    ? "Saved state: " + name
                    : "Failed to save state: " + name;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Play")) {
                ChangeState(name, 0.2f);

                useTransitionGraph = false;
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.1f, 0.1f, 1.0f));
            if (ImGui::SmallButton("X")) { targetToDelete = name; }
            ImGui::PopStyleColor();

            ImGui::PopID();
        }

        if (!targetToDelete.empty()) {
            if (editingState && editingState->name == targetToDelete) editingState = nullptr;
            RemoveState(targetToDelete);
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();


    ImGui::SeparatorText("State Properties");
    if (editingState)
    {

        ImGui::Text("Editing: %s", editingState->name.c_str());

        if (stateRenameSourceName != editingState->name)
        {
            stateRenameSourceName = editingState->name;
            stateRenameText = editingState->name;
        }
        ImGui::SetNextItemWidth(-90.0f);
        const bool renameSubmitted_ = ImGui::InputText(
            "##StateName",
            &stateRenameText,
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        const bool renameClicked_ = ImGui::Button("Rename");
        if (renameSubmitted_ || renameClicked_)
        {
            const std::string oldName_ = editingState->name;
            if (RenameState(oldName_, stateRenameText))
            {
                stateRenameSourceName = stateRenameText;
                stateAssetStatus = "Renamed state: " + oldName_ + " -> " + stateRenameText;
            }
            else
            {
                stateAssetStatus = "Failed to rename state. Name is empty or already in use.";
                stateRenameText = oldName_;
            }
        }

        const auto statePathIt_ = stateAssetPaths.find(editingState->name);
        const std::string currentStatePath_ = statePathIt_ != stateAssetPaths.end()
            ? statePathIt_->second
            : std::string();
        ImGui::TextDisabled(
            "%s",
            currentStatePath_.empty() ? "Unsaved state asset" : currentStatePath_.c_str());

        if (ImGui::Button("Save State"))
        {
            stateAssetStatus = SaveStateAsset(editingState->name, currentStatePath_)
                ? "Saved state: " + editingState->name
                : "Failed to save state: " + editingState->name;
        }
        ImGui::SameLine();
        if (ImGui::Button("Save State As..."))
        {
            const std::string initialPath_ = currentStatePath_.empty()
                ? BuildDefaultStateAssetPath(editingState->name)
                : currentStatePath_;
            const std::string selectedPath_ = WindowsDialogHelper::SaveJsonDialog(
                L"Save Animation State",
                initialPath_);
            if (!selectedPath_.empty())
            {
                stateAssetStatus = SaveStateAsset(editingState->name, selectedPath_)
                    ? "Saved state: " + selectedPath_
                    : "Failed to save state: " + selectedPath_;
            }
        }
        if (!currentStatePath_.empty())
        {
            ImGui::SameLine();
            if (ImGui::Button("Reload State"))
            {
                const std::string stateName_ = editingState->name;
                stateAssetStatus = LoadStateAsset(currentStatePath_)
                    ? "Reloaded state: " + stateName_
                    : "Failed to reload state: " + currentStatePath_;
            }
        }

        ImGui::TextUnformatted("State Type");
        int currentType_ = static_cast<int>(editingState->type);
        if (ImGui::RadioButton("Single", currentType_ == static_cast<int>(AnimStateType::Single)))
        {
            editingState->type = AnimStateType::Single;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Blend 1D", currentType_ == static_cast<int>(AnimStateType::Blend1D)))
        {
            editingState->type = AnimStateType::Blend1D;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Blend 2D", currentType_ == static_cast<int>(AnimStateType::BLEND_2D)))
        {
            editingState->type = AnimStateType::BLEND_2D;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Custom", currentType_ == static_cast<int>(AnimStateType::Custom)))
        {
            editingState->type = AnimStateType::Custom;
        }

        const auto& animations_ = modelComponent->GetAnimations();
        auto DrawAnimationIndex_ = [&](const char* label_, int& targetIndex_)
            {
                ImGui::PushID(label_);
                ImGui::TextUnformatted(label_);
                const char* selectedName_ =
                    targetIndex_ >= 0 && targetIndex_ < static_cast<int>(animations_.size())
                    ? animations_[targetIndex_].name.c_str()
                    : "None";
                ImGui::TextDisabled("%s", selectedName_);
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputTextWithHint(
                    "##AnimationClipSearch",
                    "Search animation clips...",
                    &animationClipSearchText);
                ImGui::BeginChild("AnimationClipList", ImVec2(0.0f, 110.0f), true);
                for (int index_ = 0; index_ < static_cast<int>(animations_.size()); ++index_)
                {
                    if (!ContainsCaseInsensitive(animations_[index_].name, animationClipSearchText)) continue;

                    if (ImGui::Selectable(
                        animations_[index_].name.c_str(),
                        targetIndex_ == index_))
                    {
                        targetIndex_ = index_;
                    }
                }
                ImGui::EndChild();
                ImGui::PopID();
            };

        if (editingState->type == AnimStateType::Single)
        {
            DrawAnimationIndex_("Animation Clip", editingState->animationIndex);
        }
        else if (editingState->type == AnimStateType::Blend1D)
        {
            DrawAnimationIndex_("Animation A", editingState->animIndexA);
            DrawAnimationIndex_("Animation B", editingState->animIndexB);

            ImGui::TextUnformatted("Blend Parameter");
            if (ImGui::Button(
                editingState->blendParamName.empty()
                ? "Select Blackboard Parameter"
                : editingState->blendParamName.c_str(),
                ImVec2(-1.0f, 0.0f)))
            {
                ImGui::OpenPopup("BlendParameterPopup");
            }
            if (ImGui::BeginPopup("BlendParameterPopup"))
            {
                for (const auto& [parameterName_, value_] : GetEditableBlackboard())
                {
                    if (std::holds_alternative<float>(value_) &&
                        ImGui::MenuItem(parameterName_.c_str()))
                    {
                        editingState->blendParamName = parameterName_;
                    }
                }
                ImGui::EndPopup();
            }

            ImGui::SeparatorText("Blend Timing");
            ImGui::SliderFloat(
                "Default Weight",
                &editingState->blendDefaultWeight,
                0.0f,
                1.0f);
            ImGui::DragFloat(
                "Parameter Min",
                &editingState->blendParameterMin,
                0.01f);
            ImGui::DragFloat(
                "Parameter Max",
                &editingState->blendParameterMax,
                0.01f);
            ImGui::Checkbox(
                "Synchronize Normalized Time",
                &editingState->blendSyncNormalizedTime);
            ImGui::SliderFloat(
                "Animation A Phase",
                &editingState->blendPhaseOffsetA,
                -1.0f,
                1.0f);
            ImGui::SliderFloat(
                "Animation B Phase",
                &editingState->blendPhaseOffsetB,
                -1.0f,
                1.0f);
            ImGui::TextDisabled(
                "Phase uses normalized cycles. 0.5 shifts an animation by half a cycle.");

            ImGui::SeparatorText("Animation Mask");
            ImGui::Checkbox("Use Animation Mask", &editingState->blendUseMask);
            if (editingState->blendUseMask)
            {
                ImGui::TextDisabled(
                    "Animation B is blended only on selected nodes and their descendants.");
                if (ImGui::SmallButton("Clear Mask"))
                {
                    editingState->blendMaskRootNodes.clear();
                }

                const auto& nodes_ = modelComponent->GetNodes();
                ImGui::BeginChild("AnimationMaskNodeList", ImVec2(0.0f, 190.0f), true);
                for (int nodeIndex_ = 0;
                    nodeIndex_ < static_cast<int>(nodes_.size());
                    ++nodeIndex_)
                {
                    const auto& node_ = nodes_[nodeIndex_];
                    int depth_ = 0;
                    int parentIndex_ = node_.parentIndex;
                    bool inherited_ = false;
                    while (parentIndex_ >= 0 &&
                        parentIndex_ < static_cast<int>(nodes_.size()))
                    {
                        ++depth_;
                        const std::string& parentName_ = nodes_[parentIndex_].name;
                        if (std::find(
                            editingState->blendMaskRootNodes.begin(),
                            editingState->blendMaskRootNodes.end(),
                            parentName_) != editingState->blendMaskRootNodes.end())
                        {
                            inherited_ = true;
                        }
                        parentIndex_ = nodes_[parentIndex_].parentIndex;
                    }

                    auto selectedIt_ = std::find(
                        editingState->blendMaskRootNodes.begin(),
                        editingState->blendMaskRootNodes.end(),
                        node_.name);
                    bool selected_ = selectedIt_ != editingState->blendMaskRootNodes.end();

                    ImGui::PushID(nodeIndex_);
                    ImGui::Indent(static_cast<float>(depth_) * 12.0f);
                    if (ImGui::Checkbox("##MaskNode", &selected_))
                    {
                        if (selected_)
                        {
                            editingState->blendMaskRootNodes.push_back(node_.name);
                        }
                        else
                        {
                            editingState->blendMaskRootNodes.erase(
                                std::remove(
                                    editingState->blendMaskRootNodes.begin(),
                                    editingState->blendMaskRootNodes.end(),
                                    node_.name),
                                editingState->blendMaskRootNodes.end());
                        }
                    }
                    ImGui::SameLine();
                    if (inherited_ && !selected_)
                    {
                        ImGui::TextDisabled("%s (Inherited)", node_.name.c_str());
                    }
                    else
                    {
                        ImGui::TextUnformatted(node_.name.c_str());
                    }
                    ImGui::Unindent(static_cast<float>(depth_) * 12.0f);
                    ImGui::PopID();
                }
                ImGui::EndChild();
            }
        }
        else if (editingState->type == AnimStateType::BLEND_2D)
        {
            ImGui::SeparatorText("Directional Animations");
            if (ImGui::TreeNode("Forward (0, +1)"))
            {
                DrawAnimationIndex_(
                    "Forward Animation", editingState->blend2DForwardAnimationIndex);
                ImGui::SliderFloat(
                    "Forward Phase",
                    &editingState->blend2DForwardPhaseOffset,
                    -1.0f,
                    1.0f);
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Backward (0, -1)"))
            {
                DrawAnimationIndex_(
                    "Backward Animation", editingState->blend2DBackwardAnimationIndex);
                ImGui::SliderFloat(
                    "Backward Phase",
                    &editingState->blend2DBackwardPhaseOffset,
                    -1.0f,
                    1.0f);
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Left (-1, 0)"))
            {
                DrawAnimationIndex_(
                    "Left Animation", editingState->blend2DLeftAnimationIndex);
                ImGui::SliderFloat(
                    "Left Phase",
                    &editingState->blend2DLeftPhaseOffset,
                    -1.0f,
                    1.0f);
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Right (+1, 0)"))
            {
                DrawAnimationIndex_(
                    "Right Animation", editingState->blend2DRightAnimationIndex);
                ImGui::SliderFloat(
                    "Right Phase",
                    &editingState->blend2DRightPhaseOffset,
                    -1.0f,
                    1.0f);
                ImGui::TreePop();
            }

            ImGui::SeparatorText("Blend Input");
            if (ImGui::Button(
                editingState->blend2DParamName.empty()
                ? "Select Vector2 Parameter"
                : editingState->blend2DParamName.c_str(),
                ImVec2(-1.0f, 0.0f)))
            {
                ImGui::OpenPopup("Blend2DParameterPopup");
            }
            if (ImGui::BeginPopup("Blend2DParameterPopup"))
            {
                for (const auto& [parameterName_, value_] : GetEditableBlackboard())
                {
                    if (std::holds_alternative<DirectX::SimpleMath::Vector2>(value_) &&
                        ImGui::MenuItem(parameterName_.c_str()))
                    {
                        editingState->blend2DParamName = parameterName_;
                    }
                }
                ImGui::EndPopup();
            }

            ImGui::DragFloat2(
                "Default Input",
                &editingState->blend2DDefaultInput.x,
                0.01f,
                -1.0f,
                1.0f);
            ImGui::Checkbox(
                "Synchronize Normalized Time##Blend2D",
                &editingState->blend2DSyncNormalizedTime);
            ImGui::TextDisabled(
                "X blends Left/Right. Y blends Backward/Forward.");
            ImGui::TextDisabled(
                "Diagonal input automatically blends the two matching directions.");
        }
        else if (editingState->type == AnimStateType::Custom)
        {
            if (poseLibrary && !poseLibrary->customAnims.empty())
            {
                ImGui::TextUnformatted("Custom Animation");
                ImGui::BeginChild("CustomAnimationList", ImVec2(0.0f, 110.0f), true);
                for (int index_ = 0; index_ < static_cast<int>(poseLibrary->customAnims.size()); ++index_)
                {
                    if (ImGui::Selectable(
                        poseLibrary->customAnims[index_].name.c_str(),
                        editingState->customAnimIndex == index_))
                    {
                        editingState->customAnimIndex = index_;
                    }
                }
                ImGui::EndChild();
            }
            else
            {
                ImGui::TextDisabled("Create a custom animation in Pose Animation Creator.");
            }
        }
        ImGui::DragFloat("Speed", &editingState->playbackSpeed, 0.05f);
        ImGui::Checkbox("Loop", &editingState->isLoop);


        ImGui::Spacing();
        if (ImGui::TreeNode("Root Motion Settings"))
        {

            ImGui::Checkbox("Enable Root Motion", &editingState->rootMotion.enable);

            if (editingState->rootMotion.enable)
            {
                ImGui::Indent();

                ImGui::DragFloat("Root Motion Scale", &editingState->rootMotionScale, 0.01f);

                ImGui::TextUnformatted("Root Motion Scale Curve");
                if (ImGui::Button(
                    editingState->rootMotionScaleCurveName.empty()
                    ? "No Curve"
                    : editingState->rootMotionScaleCurveName.c_str(),
                    ImVec2(-1.0f, 0.0f)))
                {
                    ImGui::OpenPopup("RootMotionCurvePopup");
                }
                if (ImGui::BeginPopup("RootMotionCurvePopup"))
                {
                    if (ImGui::MenuItem("No Curve"))
                    {
                        editingState->rootMotionScaleCurveName.clear();
                    }
                    for (const std::string& curveName_ : curveNames)
                    {
                        if (ImGui::MenuItem(curveName_.c_str()))
                        {
                            editingState->rootMotionScaleCurveName = curveName_;
                        }
                    }
                    ImGui::EndPopup();
                }

                ImGui::TextDisabled("Final RootMotion = Delta * Scale * CurveValue");

                const std::string sourcePreview_ = editingState->rootMotion.sourceNodeName.empty()
                    ? std::string("Model Default (") + modelComponent->GetRootMotionName() + ")"
                    : editingState->rootMotion.sourceNodeName;
                ImGui::TextUnformatted("Motion Source Node");
                if (ImGui::Button(sourcePreview_.c_str(), ImVec2(-1.0f, 0.0f)))
                {
                    ImGui::OpenPopup("RootMotionNodePopup");
                }
                if (ImGui::BeginPopup("RootMotionNodePopup"))
                {
                    if (ImGui::MenuItem("Model Default"))
                    {
                        editingState->rootMotion.sourceNodeName.clear();
                    }

                    for (const auto& node_ : modelComponent->GetNodes())
                    {
                        if (ImGui::MenuItem(node_.name.c_str()))
                        {
                            editingState->rootMotion.sourceNodeName = node_.name;
                        }
                    }
                    ImGui::EndPopup();
                }
                ImGui::TextDisabled("Source-node movement is always removed while Root Motion is enabled.");

                ImGui::Text("Apply Position to GameObject:");
                ImGui::Checkbox("X##Pos", &editingState->rootMotion.applyPositionX); ImGui::SameLine();
                ImGui::Checkbox("Y##Pos", &editingState->rootMotion.applyPositionY); ImGui::SameLine();
                ImGui::Checkbox("Z##Pos", &editingState->rootMotion.applyPositionZ);



                ImGui::Unindent();
            }

            ImGui::TreePop();
        }
        ImGui::TextDisabled("Use Window > Animation Sequence to edit events in the sequencer.");
    }
    else
    {
        ImGui::TextDisabled("Select a state from the list or node editor to edit.");
    }



    if (ImGui::CollapsingHeader("Blackboard (Params)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        static char newVarName[64] = "";
        static int selectedType = 0; // 0:Float, 1:Int, 2:Bool, 3:Vector2
        const char* typeNames[] = { "Float", "Int", "Bool", "Vector2" };


        ImGui::SeparatorText("Add New Parameter");
        ImGui::SetNextItemWidth(150.0f);
        ImGui::InputText("Name##BB", newVarName, IM_ARRAYSIZE(newVarName));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        ImGui::Combo("Type##BB", &selectedType, typeNames, IM_ARRAYSIZE(typeNames));
        ImGui::SameLine();
        if (ImGui::Button("Add##BB"))
        {
            std::string name(newVarName);
            if (!name.empty() && GetEditableBlackboard().find(name) == GetEditableBlackboard().end())
            {
                switch (selectedType)
                {
                case 0: GetEditableBlackboard()[name] = 0.0f;                                   break;
                case 1: GetEditableBlackboard()[name] = 0;                                      break;
                case 2: GetEditableBlackboard()[name] = false;                                  break;
                case 3: GetEditableBlackboard()[name] = DirectX::SimpleMath::Vector2(0.0f, 0.0f); break;
                }
                newVarName[0] = '\0';
            }
        }


        ImGui::SeparatorText("Parameters");

        if (GetEditableBlackboard().empty())
        {
            ImGui::TextDisabled("No parameters defined.");
        }
        else
        {
            std::string toDelete;
            for (auto& [key, val] : GetEditableBlackboard())
            {
                ImGui::PushID(key.c_str());


                ImGui::Text("%-20s", key.c_str());
                ImGui::SameLine();


                if (auto* f = std::get_if<float>(&val)) {
                    ImGui::SetNextItemWidth(100.0f);
                    ImGui::DragFloat("##v", f, 0.01f);
                }
                else if (auto* i = std::get_if<int>(&val)) {
                    ImGui::SetNextItemWidth(100.0f);
                    ImGui::DragInt("##v", i);
                }
                else if (auto* b = std::get_if<bool>(&val)) {
                    ImGui::Checkbox("##v", b);
                }
                else if (auto* v = std::get_if<DirectX::SimpleMath::Vector2>(&val)) {
                    ImGui::SetNextItemWidth(180.0f);
                    ImGui::DragFloat2("##v", &v->x, 0.01f);
                }

                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.1f, 0.1f, 1.0f));
                if (ImGui::Button("X##del")) { toDelete = key; }
                ImGui::PopStyleColor();

                ImGui::PopID();
            }
            if (!toDelete.empty()) { GetEditableBlackboard().erase(toDelete); }
        }
    }

}

void AnimationControllerComponent::ShowSequencerEditor()
{

    ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_FirstUseEver);


    if (!ImGui::Begin("Animation Sequence Editor", &showSequencerWindow))
    {
        ImGui::End();
        return;
    }

    if (!editingState)
    {
        ImGui::TextDisabled("Select a state to edit events.");
        ImGui::End();
        return;

    }
    else
    {

        AnimSequenceAdapter adapter;
        adapter.targetState = const_cast<AnimState*>(editingState);
        adapter.duration = modelComponent->GetAnimationDuration(editingState->animationIndex);
        adapter.fps = 60.0f;
        adapter.SyncFrames();

        static int selectedEntry = -1;
        static int firstFrame = 0;
        static bool expanded = true;


        int currentFrame = (int)(animator.GetCurrentFadeTime() * adapter.fps);
        const int previousFrame_ = currentFrame;



        ImGui::PushItemWidth(-1);



        if (ImGui::BeginChild("SequencerRegion", ImVec2(0, 300), true))
        {
            if (ImSequencer::Sequencer(&adapter, &currentFrame, &expanded, &selectedEntry, &firstFrame,
                ImSequencer::SEQUENCER_EDIT_ALL | ImSequencer::SEQUENCER_ADD |
                ImSequencer::SEQUENCER_DEL | ImSequencer::SEQUENCER_CHANGE_FRAME))
            {


                if (selectedEntry >= 0 && selectedEntry < (int)adapter.targetState->events.size())
                {
                    int* pStart, * pEnd;
                    adapter.Get(selectedEntry, &pStart, &pEnd, nullptr, nullptr);
                    auto& ev = adapter.targetState->events[selectedEntry];
                    ev.startTime = adapter.ToTime(*pStart);
                    ev.endTime = adapter.ToTime(*pEnd);
                }
            }
            ImGui::EndChild();
        }

        if (currentFrame != previousFrame_)
        {
            const int clampedFrame_ = (std::max)(
                adapter.GetFrameMin(),
                (std::min)(adapter.GetFrameMax(), currentFrame));
            ChangeState(editingState->name, 0.0f);
            SetAnimatorTime(adapter.ToTime(clampedFrame_));
        }


        ImGui::PopItemWidth();


        if (selectedEntry >= 0 && selectedEntry < (int)adapter.targetState->events.size())
        {

            if (ImGui::BeginChild("EventDetails", ImVec2(0, 0), true))
            {
                auto& ev = adapter.targetState->events[selectedEntry];


                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Editing Event #%d", selectedEntry);
                ImGui::SameLine();


                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
                if (ImGui::Button("Delete Event"))
                {

                    adapter.Del(selectedEntry);
                    selectedEntry = -1;
                    ImGui::PopStyleColor(2);
                    ImGui::EndChild();
                    ImGui::End();
                    return;
                }
                ImGui::PopStyleColor(2);

                ImGui::SameLine(ImGui::GetWindowWidth() - 100);
                if (ImGui::Button("Deselect")) { selectedEntry = -1; }


                ImGui::SetNextItemWidth(200.0f);
                if (ImGui::InputText("Display Label", &ev.eventLabel)) {

                }
                ImGui::SameLine();
                ImGui::TextDisabled("(Name on timeline)");

                ImGui::Separator();
                auto& globalReg = ActionRegistry::Instance();
                auto owner = GetGameObject();

                struct CategoryEntry
                {
                    std::string category;
                    std::string menuLabel;
                    ActionRegistry* registry = nullptr;
                };
                std::vector<CategoryEntry> allCategories;

                auto addRegistryCategories_ = [&](ActionRegistry& registry_, const std::string& sourceLabel_)
                    {
                        for (const auto& category_ : registry_.GetCategories())
                        {
                            allCategories.push_back({ category_, sourceLabel_ + " " + category_, &registry_ });
                        }
                    };

                if (owner)
                {
                    addRegistryCategories_(owner->localRegistry, "[Self]");

                    auto parentTransform_ = owner->transform ? owner->transform->parent.lock() : nullptr;
                    while (parentTransform_)
                    {
                        auto parentObject_ = parentTransform_->gameObject.lock();
                        if (!parentObject_) break;

                        addRegistryCategories_(parentObject_->localRegistry, "[Parent:" + parentObject_->GetName() + "]");
                        parentTransform_ = parentObject_->transform ? parentObject_->transform->parent.lock() : nullptr;
                    }

                    auto collectChildActions_ = [&](auto&& self_, const std::shared_ptr<GameObject>& object_) -> void
                        {
                            if (!object_ || !object_->transform) return;

                            for (const auto& childTransform_ : object_->transform->children)
                            {
                                if (!childTransform_) continue;

                                auto childObject_ = childTransform_->gameObject.lock();
                                if (!childObject_) continue;

                                addRegistryCategories_(childObject_->localRegistry, "[Child:" + childObject_->GetName() + "]");
                                self_(self_, childObject_);
                            }
                        };
                    collectChildActions_(collectChildActions_, owner);
                }

                addRegistryCategories_(globalReg, "[Global]");

                auto ActionField = [&](const char* label, std::string& targetStr) {
                    ImGui::SetNextItemWidth(250.0f);
                    ImGui::InputText(label, &targetStr);

                    ImGui::SameLine();
                    std::string popupName = std::string("ActionSelect##") + label;
                    if (ImGui::Button((std::string("Select##") + label).c_str())) {
                        ImGui::OpenPopup(popupName.c_str());
                    }

                    if (ImGui::BeginPopup(popupName.c_str())) {
                        if (ImGui::MenuItem("Clear")) {
                            targetStr.clear();
                        }
                        ImGui::Separator();

                        for (const auto& entry : allCategories) {
                            if (ImGui::BeginMenu(entry.menuLabel.c_str())) {
                                std::vector<std::string> actionNames;
                                if (entry.registry) {
                                    actionNames = entry.registry->GetActionNames(entry.category);
                                }

                                for (const auto& actionName : actionNames) {
                                    if (ImGui::MenuItem(actionName.c_str())) {
                                        targetStr = actionName;
                                    }
                                }
                                ImGui::EndMenu();
                            }
                        }
                        ImGui::EndPopup();
                    }
                    };

                ActionField("Start Action", ev.startAction);
                ActionField("Update Action", ev.updateAction);
                ActionField("End Action", ev.endAction);
                ImGui::SeparatorText("Blackboard Writes");
                int removeWriteIndex_ = -1;
                for (int writeIndex_ = 0;
                    writeIndex_ < static_cast<int>(ev.blackboardWrites.size());
                    ++writeIndex_)
                {
                    AnimBlackboardWrite& write_ = ev.blackboardWrites[writeIndex_];
                    ImGui::PushID(writeIndex_);
                    ImGui::InputText("Category", &write_.category);
                    ImGui::InputText("Key", &write_.key);

                    int typeIndex_ = static_cast<int>(write_.type);
                    const char* typeNames_[] = { "Float", "Int", "Bool", "String" };
                    if (ImGui::Combo("Type", &typeIndex_, typeNames_, IM_ARRAYSIZE(typeNames_)))
                    {
                        write_.type = static_cast<ANIM_BLACKBOARD_VALUE_TYPE>(typeIndex_);
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
                        const bool isParticleAsset_ = write_.key == "HitParticleAssetPath";
                        const bool isMeshVFXAsset_ = write_.key == "HitMeshVFXAssetPath";
                        std::shared_ptr<GameObject> owner_ = GetGameObject();
                        if ((isParticleAsset_ || isMeshVFXAsset_) && owner_)
                        {
                            std::vector<std::string> assets_ = isParticleAsset_
                        ? owner_->GetManager().GetEffectManager().GetParticleAssetKeys()
                        : owner_->GetManager().GetEffectManager().GetMeshVFXAssetKeys();
                    if (isMeshVFXAsset_)
                    {
                        const std::vector<std::string>& sequenceAssets_ =
                            owner_->GetManager().GetEffectManager().GetVFXSequenceAssetKeys();
                        assets_.insert(
                            assets_.end(),
                            sequenceAssets_.begin(),
                            sequenceAssets_.end());
                    }
                            std::string previewValue_ = write_.stringValue;
                            const std::string resolvedValue_ = isParticleAsset_
                                ? owner_->GetManager().GetEffectManager().ResolveParticlePath(write_.stringValue)
                                : owner_->GetManager().GetEffectManager().ResolveMeshVFXPath(write_.stringValue);
                            for (const std::string& assetKey_ : assets_)
                            {
                                const std::string resolvedKey_ = isParticleAsset_
                                    ? owner_->GetManager().GetEffectManager().ResolveParticlePath(assetKey_)
                                    : owner_->GetManager().GetEffectManager().ResolveMeshVFXPath(assetKey_);
                                if (!resolvedValue_.empty() && resolvedValue_ == resolvedKey_)
                                {
                                    previewValue_ = assetKey_;
                                    break;
                                }
                            }
                            if (ImGui::BeginCombo("Value", previewValue_.empty() ? "(None)" : previewValue_.c_str()))
                            {
                                if (ImGui::Selectable("(None)", write_.stringValue.empty())) write_.stringValue.clear();
                                for (const std::string& assetKey_ : assets_)
                                {
                                    const bool selected_ = previewValue_ == assetKey_;
                                    if (ImGui::Selectable(assetKey_.c_str(), selected_)) write_.stringValue = assetKey_;
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
                    if (ImGui::Button("Remove Write")) removeWriteIndex_ = writeIndex_;
                    ImGui::Separator();
                    ImGui::PopID();
                }
                if (removeWriteIndex_ >= 0)
                {
                    ev.blackboardWrites.erase(ev.blackboardWrites.begin() + removeWriteIndex_);
                }
                auto addAttackWrite_ = [&ev](const char* key_, ANIM_BLACKBOARD_VALUE_TYPE type_)
                {
                    AnimBlackboardWrite write_{};
                    write_.category = "Attack";
                    write_.key = key_;
                    write_.type = type_;
                    ev.blackboardWrites.push_back(write_);
                };
                if (ImGui::Button("+ Damage")) addAttackWrite_("Damage", ANIM_BLACKBOARD_VALUE_TYPE::FLOAT);
                ImGui::SameLine();
                if (ImGui::Button("+ HitStopDuration")) addAttackWrite_("HitStopDuration", ANIM_BLACKBOARD_VALUE_TYPE::FLOAT);
                ImGui::SameLine();
                if (ImGui::Button("+ HitStopScale")) addAttackWrite_("HitStopScale", ANIM_BLACKBOARD_VALUE_TYPE::FLOAT);
                ImGui::SameLine();
                if (ImGui::Button("+ HitStopLayer")) addAttackWrite_("HitStopLayerMask", ANIM_BLACKBOARD_VALUE_TYPE::INT);
                ImGui::SameLine();
                if (ImGui::Button("+ KnockBack")) addAttackWrite_("KnockBackPower", ANIM_BLACKBOARD_VALUE_TYPE::FLOAT);
                ImGui::SameLine();
                if (ImGui::Button("+ HitParticle")) addAttackWrite_("HitParticleAssetPath", ANIM_BLACKBOARD_VALUE_TYPE::STRING);
                ImGui::SameLine();
                if (ImGui::Button("+ HitMeshVFX")) addAttackWrite_("HitMeshVFXAssetPath", ANIM_BLACKBOARD_VALUE_TYPE::STRING);
                if (ImGui::Button("Add Blackboard Write"))
                {
                    ev.blackboardWrites.push_back(AnimBlackboardWrite{});
                }

                ImGui::Checkbox("Call End On State Exit", &ev.callEndOnStateExit);
                ImGui::Checkbox("Override Playback Speed", &ev.overridePlaybackSpeed);
                if (ev.overridePlaybackSpeed)
                {
                    ImGui::SetNextItemWidth(120.0f);
                    ImGui::DragFloat("Playback Speed", &ev.playbackSpeed, 0.05f, 0.01f, 10.0f, "%.2fx");
                }

                ImGui::Separator();


                ImGui::PushItemWidth(120.0f);
                if (ImGui::DragFloat("Start (s)", &ev.startTime, 0.01f, 0.0f, ev.endTime)) {
                    ev.frameStart = adapter.ToFrame(ev.startTime);
                }
                ImGui::SameLine();
                if (ImGui::DragFloat("End (s)", &ev.endTime, 0.01f, ev.startTime, adapter.duration)) {
                    ev.frameEnd = adapter.ToFrame(ev.endTime);
                }
                ImGui::PopItemWidth();

                ImGui::EndChild();
            }
        }
        else
        {
            ImGui::TextDisabled("Click an event bar or name to edit details.");
        }

        ImGui::End();
    }

}

