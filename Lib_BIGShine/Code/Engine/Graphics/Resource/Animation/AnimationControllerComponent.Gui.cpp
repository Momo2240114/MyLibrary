// 通常 Inspector GUI / Sequencer GUI

#include "AnimationControllerComponent.h"
#include "AnimSequenceAdapter.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "AnimationDataAsset.h"
#include "PoseLibraryResourceManager.h"
#include "IKMapComponent.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <unordered_map>

namespace ed = ax::NodeEditor;

void AnimationControllerComponent::Gui()
{
    if (!modelComp) return;

    ImGui::Text("Model: %s", modelComp->GetModelName().c_str());
    ImGui::TextDisabled("Animation editors are opened from the main Window menu.");


    ImGui::SeparatorText("Animation Asset");

    ImGui::InputText("Path", &m_assetFilePath);
    if (ImGui::Button("Save to File")) {SaveAsset(m_assetFilePath);}
    ImGui::SameLine();
    if (ImGui::Button("Load from File")) { LoadAsset(m_assetFilePath);}

    ImGui::Separator();

    ImGui::Checkbox("usedTransitionGraph", &usedTransitionGraph);

    ImGui::Separator();


    std::vector<std::string> curveNames;
    if (curvecomp) {
        curveNames = curvecomp->GetTrackNames();
    }


    if (ImGui::CollapsingHeader("Add New State", ImGuiTreeNodeFlags_None))
    {
        static char newStateName[64] = "CustomState";
        static int selectedAnimIdx = 0;
        ImGui::InputText("State Name", newStateName, 64);

        const auto& anims = modelComp->GetAnimations();
        if (ImGui::BeginCombo("Base Animation", anims[selectedAnimIdx].name.c_str()))
        {
            for (int i = 0; i < (int)anims.size(); i++)
            {
                if (ImGui::Selectable(anims[i].name.c_str(), selectedAnimIdx == i)) selectedAnimIdx = i;
            }
            ImGui::EndCombo();
        }
        if (ImGui::Button("Add State", ImVec2(-1, 0))) { AddState(newStateName, selectedAnimIdx); }
    }

    ImGui::Separator();


    ImGui::SeparatorText("State Asset Import");
    static std::string loadStateAssetName_;
    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputText("Load State", &loadStateAssetName_);
    ImGui::SameLine();
    if (ImGui::Button("Load State Asset") && !loadStateAssetName_.empty())
    {
        LoadStateAsset(loadStateAssetName_);
    }
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Registered Animation States");

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));

    if (ImGui::BeginChild("StatesRegion", ImVec2(0, 200), true))
    {
        std::string targetToDelete = "";

        for (auto& [name, statePtr] : states)
        {
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


            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 65);
            if (ImGui::Button("Play")) {
                ChangeState(name, 0.2f);

                usedTransitionGraph = false;
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.1f, 0.1f, 1.0f));
            if (ImGui::Button("X")) { targetToDelete = name; }
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
        ImGui::SameLine();
        if (ImGui::Button("Save Editing State"))
        {
            SaveStateAsset(editingState->name);
        }


        const char* typeNames[] = { "Single", "Blend1D", "Custom" };
        int currentType = (int)editingState->type;
        if (ImGui::Combo("Type", &currentType, typeNames, IM_ARRAYSIZE(typeNames))) {
            editingState->type = (AnimStateType)currentType;
        }

        if (editingState->type == AnimStateType::Single) {
            const auto& anims = modelComp->GetAnimations();
            if (ImGui::BeginCombo("Animation", anims[editingState->animationIndex].name.c_str())) {
                for (int i = 0; i < (int)anims.size(); i++) {
                    if (ImGui::Selectable(anims[i].name.c_str(), editingState->animationIndex == i))
                        editingState->animationIndex = i;
                }
                ImGui::EndCombo();
            }
        }
        else if (editingState->type == AnimStateType::Blend1D) {
            const auto& anims = modelComp->GetAnimations();
            auto ComboAnim = [&](const char* label, int& targetIdx) {
                std::string cName = (targetIdx >= 0) ? anims[targetIdx].name : "None";
                if (ImGui::BeginCombo(label, cName.c_str())) {
                    for (int i = 0; i < (int)anims.size(); i++) {
                        if (ImGui::Selectable(anims[i].name.c_str(), targetIdx == i)) targetIdx = i;
                    }
                    ImGui::EndCombo();
                }
                };
            ComboAnim("Anim A", editingState->animIndexA);
            ComboAnim("Anim B", editingState->animIndexB);
            ImGui::InputText("Blend Param", &editingState->blendParamName);
        }
        else if (editingState->type == AnimStateType::Custom) {
            if (m_poseLibrary && !m_poseLibrary->customAnims.empty()) {
                int selectedCustom = editingState->customAnimIndex;
                if (selectedCustom < 0 || selectedCustom >= (int)m_poseLibrary->customAnims.size()) selectedCustom = 0;
                const std::string& previewName = m_poseLibrary->customAnims[selectedCustom].name;
                if (ImGui::BeginCombo("Custom Animation", previewName.c_str())) {
                    for (int i = 0; i < (int)m_poseLibrary->customAnims.size(); ++i) {
                        if (ImGui::Selectable(m_poseLibrary->customAnims[i].name.c_str(), selectedCustom == i)) {
                            editingState->customAnimIndex = i;
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            else {
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

                ImGui::SetNextItemWidth(220.0f);
                ImGui::InputText("Root Motion Scale Curve", &editingState->rootMotionScaleCurveName);

                ImGui::TextDisabled("Final RootMotion = Delta * Scale * CurveValue");

                const std::string sourcePreview_ = editingState->rootMotion.sourceNodeName.empty()
                    ? std::string("Model Default (") + modelComp->GetRootMotionName() + ")"
                    : editingState->rootMotion.sourceNodeName;
                if (ImGui::BeginCombo("Motion Source Node", sourcePreview_.c_str()))
                {
                    const bool useModelDefault_ = editingState->rootMotion.sourceNodeName.empty();
                    if (ImGui::Selectable("Model Default", useModelDefault_))
                    {
                        editingState->rootMotion.sourceNodeName.clear();
                    }

                    for (const auto& node_ : modelComp->GetNodes())
                    {
                        const bool selected_ = editingState->rootMotion.sourceNodeName == node_.name;
                        if (ImGui::Selectable(node_.name.c_str(), selected_))
                        {
                            editingState->rootMotion.sourceNodeName = node_.name;
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::TextDisabled("Source-node movement is always removed while Root Motion is enabled.");

                ImGui::Text("Apply Position to GameObject:");
                ImGui::Checkbox("X##Pos", &editingState->rootMotion.applyPositionX); ImGui::SameLine();
                ImGui::Checkbox("Y##Pos", &editingState->rootMotion.applyPositionY); ImGui::SameLine();
                ImGui::Checkbox("Z##Pos", &editingState->rootMotion.applyPositionZ);


                ImGui::TextDisabled("Rotation is not modified by Root Motion.");

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
        adapter.duration = modelComp->GetAnimationDuration(editingState->animationIndex);
        adapter.fps = 60.0f;

        static int selectedEntry = -1;
        static int firstFrame = 0;
        static bool expanded = true;


        int currentFrame = (int)(animator.GetCurrentFadeTime() * adapter.fps);



        ImGui::PushItemWidth(-1);



        if (ImGui::BeginChild("SequencerRegion", ImVec2(0, 300), true))
        {
            if (ImSequencer::Sequencer(&adapter, &currentFrame, &expanded, &selectedEntry, &firstFrame,
                ImSequencer::SEQUENCER_EDIT_ALL | ImSequencer::SEQUENCER_ADD | ImSequencer::SEQUENCER_DEL))
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

                struct CategoryEntry { std::string name; bool isLocal; };
                std::vector<CategoryEntry> allCategories;

                if (owner) {
                    for (const auto& cat : owner->localRegistry.GetCategories()) {
                        allCategories.push_back({ cat, true });
                    }
                }
                for (const auto& cat : globalReg.GetCategories()) {
                    allCategories.push_back({ cat, false });
                }

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
                            std::string menuLabel = (entry.isLocal ? "[L] " : "[G] ") + entry.name;
                            if (ImGui::BeginMenu(menuLabel.c_str())) {
                                std::vector<std::string> actionNames;
                                if (entry.isLocal && owner) {
                                    actionNames = owner->localRegistry.GetActionNames(entry.name);
                                }
                                else {
                                    actionNames = globalReg.GetActionNames(entry.name);
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

