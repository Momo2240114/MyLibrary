#include "PrecompiledHeader.h"
#include "SkillWorkspaceWindow.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/AI/AIActionComponent.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/EventSystem/ActionSequence.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Core/LightManager.h"
#include "Engine/Graphics/Core/RenderContext.h"
#include "Engine/Graphics/Decal/DecalManager.h"
#include "Engine/Graphics/Decal/DecalComponent.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"
#include "Game/Charcter/Player/PlayerControllerComponent.h"
#include "Game/AI/Dragon/DragonAIComponent.h"
#include "ImSequencer.h"
#include <imgui_stdlib.h>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>
#include <stdexcept>
#include <utility>
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/Math/Curve/CurveComponent.h"

namespace
{
    constexpr float SKILL_TIMELINE_FPS = 60.0f;

    // 新しいSlot形式と、旧Action形式の入力JSONを同じスロット番号へ解決する。
    int GetInputSlotIndex(const nlohmann::json& input_)
    {
        if (input_.contains("Slot"))
        {
            if (!input_.at("Slot").is_number_integer()) throw std::runtime_error("Input Slot must be an integer.");
            return input_.at("Slot").get<int>();
        }
        const std::string action_ = input_.value("Action", std::string());
        return action_ == "SkillB" ? 0 : action_ == "SkillA" ? 1 : action_ == "SkillC" ? 2 : -1;
    }

    // 装備名はSceneへ分離し、ライブラリ側では入力のスロット番号だけを検証する。
    bool ValidateCallerSettings(const nlohmann::json& document_, std::string& error_)
    {
        try
        {
            if (!document_.at("Inputs").is_array()) throw std::runtime_error("Inputs must be an array.");
            for (const auto& input_ : document_.at("Inputs"))
            {
                const int slotIndex_ = GetInputSlotIndex(input_);
                if (slotIndex_ < 0 || slotIndex_ >= static_cast<int>(PlayerSkillLoadout::SLOT_COUNT))
                    throw std::runtime_error("Input Slot must be 0, 1 or 2.");
                if (input_.at("ModifierAction").get<std::string>().empty() || input_.at("TriggerAction").get<std::string>().empty())
                    throw std::runtime_error("Input Action names must not be empty.");
                const int priority_ = input_.value("Priority", 100);
                (void)priority_;
            }
            return true;
        }
        catch (const std::exception& exception_)
        {
            error_ = exception_.what();
            return false;
        }
    }
    // 手入力も維持し、選択Actorに登録された既存Actionから設定できるようにする。
    bool DrawActionField(const char* label_, std::string& value_, const std::shared_ptr<GameObject>& owner_, const char* category_ = nullptr)
    {
        ImGui::PushID(label_);
        bool changed_ = ImGui::InputText(label_, &value_);
        if (owner_ && ImGui::BeginCombo("Registered Actions", value_.c_str()))
        {
            const auto drawRegistry_ = [&](ActionRegistry& registry_, const char* source_)
            {
                ImGui::PushID(source_);
                ImGui::TextDisabled("%s", source_);
                const auto categories_ = category_ ? std::vector<std::string>{ category_ } : registry_.GetCategories();
                for (const auto& categoryName_ : categories_)
                {
                    ImGui::PushID(categoryName_.c_str());
                    for (const auto& name_ : registry_.GetActionNames(categoryName_))
                        if (ImGui::Selectable(name_.c_str(), value_ == name_)) { value_ = name_; changed_ = true; }
                    ImGui::PopID();
                }
                ImGui::PopID();
            };
            drawRegistry_(owner_->GetLocalRegistry(), "Owner");
            if (!category_) drawRegistry_(ActionRegistry::Instance(), "Global");
            ImGui::EndCombo();
        }
        ImGui::PopID();
        return changed_;
    }
}

// Phaseを既存ImSequencerへ渡す。横移動は順序変更、幅変更は時間終了へ変換する。
struct SkillWorkspaceWindow::PhaseTimelineAdapter : ImSequencer::SequenceInterface
{
    SkillWorkspaceWindow& workspace;
    explicit PhaseTimelineAdapter(SkillWorkspaceWindow& workspace_) : workspace(workspace_) {}
    int GetFrameMin() const override { return 0; }
    int GetFrameMax() const override
    {
        int maximumFrame_ = 60;
        for (const auto& frame_ : workspace.phaseFrames) maximumFrame_ = (std::max)(maximumFrame_, frame_.end + 30);
        return maximumFrame_;
    }
    int GetItemCount() const override { return static_cast<int>(workspace.phaseFrames.size()); }
    int GetItemTypeCount() const override { return 1; }
    const char* GetItemTypeName(int typeIndex_) const override { return "Phase"; }
    const char* GetItemLabel(int index_) const override { return workspace.draft.phases[index_].animationState.c_str(); }
    void Get(int index_, int** start_, int** end_, int* type_, unsigned int* color_) override
    {
        auto& frame_ = workspace.phaseFrames[index_];
        if (start_) *start_ = &frame_.start;
        if (end_) *end_ = &frame_.end;
        if (type_) *type_ = 0;
        if (color_) *color_ = 0xFFBB8844;
    }
    void BeginEdit(int index_) override { workspace.timelineEditing = true; }
    void EndEdit() override
    {
        workspace.CommitDraft();
        std::vector<std::size_t> order_(workspace.phaseFrames.size());
        std::iota(order_.begin(), order_.end(), 0);
        std::stable_sort(order_.begin(), order_.end(), [this](std::size_t left_, std::size_t right_)
        {
            return workspace.phaseFrames[left_].start < workspace.phaseFrames[right_].start;
        });
        auto phases_ = workspace.draft.phases;
        const auto rawPhases_ = workspace.document["Skills"][workspace.selectedSkill]["Phases"];
        auto reorderedJson_ = nlohmann::json::array();
        for (std::size_t index_ = 0; index_ < order_.size(); ++index_)
        {
            const std::size_t source_ = order_[index_];
            auto phase_ = phases_[source_];
            const auto& frame_ = workspace.phaseFrames[source_];
            const int width_ = (std::max)(1, frame_.end - frame_.start);
            if (width_ != frame_.originalEnd - frame_.originalStart)
            {
                phase_.duration = static_cast<float>(width_) / SKILL_TIMELINE_FPS;
                phase_.endCondition = SkillPhaseEnd::DURATION;
                phase_.maximumDuration = (std::max)(phase_.maximumDuration, phase_.duration + 0.1f);
            }
            workspace.draft.phases[index_] = phase_;
            reorderedJson_.push_back(rawPhases_[source_]);
        }
        workspace.document["Skills"][workspace.selectedSkill]["Phases"] = std::move(reorderedJson_);
        workspace.timelineEditing = false;
        workspace.dirty = true;
        workspace.selectedPhase = -1;
    }
};

SkillWorkspaceWindow::SkillWorkspaceWindow(GameObjectManager* manager_)
    : EditorWorkspaceWindow("Skill Workspace", manager_)
{
    isVisible = false;
    LoadLibrary();
}

void SkillWorkspaceWindow::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("SkillWorkspaceWindow::Update");
    if (gameObjectManager) targetObject = gameObjectManager->GetSelectedObject();
}

void SkillWorkspaceWindow::OnVisibilityChanged(bool visible_)
{
    if (!visible_) simulateScene = false;
}

void SkillWorkspaceWindow::LoadLibrary()
{
    std::vector<SkillDefinition> definitions_;
    nlohmann::json loaded_;
    if (!LoadSkillDefinitions(libraryPath, definitions_, loaded_, statusText)) return;
    if (!loaded_.contains("Inputs")) loaded_["Inputs"] = nlohmann::json::array();
    if (!ValidateCallerSettings(loaded_, statusText)) return;
    for (auto& input_ : loaded_["Inputs"])
    {
        input_["Slot"] = GetInputSlotIndex(input_);
        input_.erase("Action");
    }
    document = std::move(loaded_);
    selectedSkill = -1;
    selectedPhase = -1;
    dirty = false;
    timelineEditing = false;
    if (!definitions_.empty()) SelectSkill(0);
    statusText = "Loaded: " + libraryPath;
}

void SkillWorkspaceWindow::CommitDraft()
{
    if (selectedSkill >= 0 && selectedSkill < static_cast<int>(document["Skills"].size()))
        SerializeSkillDefinition(draft, document["Skills"][selectedSkill]);
}

bool SkillWorkspaceWindow::SaveLibrary()
{
    CommitDraft();
    if (!ValidateCallerSettings(document, statusText)) return false;
    if (!SaveSkillDefinitions(libraryPath, document, statusText)) return false;
    dirty = false;
    statusText = "Saved: " + libraryPath;
    return true;
}

void SkillWorkspaceWindow::SelectSkill(int index_)
{
    CommitDraft();
    if (index_ < 0 || index_ >= static_cast<int>(document["Skills"].size())) return;
    SkillDefinition loaded_;
    if (!DeserializeSkillDefinition(document["Skills"][index_], loaded_, statusText)) return;
    selectedSkill = index_;
    draft = std::move(loaded_);
    selectedPhase = draft.phases.empty() ? -1 : 0;
    timelineEditing = false;
}

void SkillWorkspaceWindow::AddSkill(bool duplicate_)
{
    CommitDraft();
    SkillDefinition definition_ = duplicate_ && selectedSkill >= 0 ? draft : SkillDefinition();
    if (definition_.phases.empty()) definition_.phases.push_back(SkillPhase());
    const std::string base_ = duplicate_ ? definition_.name + "_Copy" : "NewSkill";
    definition_.name = base_;
    int suffix_ = 1;
    while (std::any_of(document["Skills"].begin(), document["Skills"].end(), [&definition_](const nlohmann::json& json_)
        { return json_.value("Name", std::string()) == definition_.name; }))
        definition_.name = base_ + std::to_string(suffix_++);
    nlohmann::json json_ = duplicate_ && selectedSkill >= 0 ? document["Skills"][selectedSkill] : nlohmann::json::object();
    json_.erase("Action");
    SerializeSkillDefinition(definition_, json_);
    document["Skills"].push_back(std::move(json_));
    SelectSkill(static_cast<int>(document["Skills"].size()) - 1);
    dirty = true;
}

void SkillWorkspaceWindow::Draw()
{
    BS_PROFILE_SCOPE("SkillWorkspaceWindow::Draw");
    EnsurePreviewResources();
    if (!BeginWorkspaceWindow()) { EndEditorWindow(); return; }
    DrawToolbar();
    const ImVec2 available_ = ImGui::GetContentRegionAvail();
    const float topHeight_ = (std::max)(200.0f, available_.y * 0.57f);
    ImGui::BeginChild("SkillLibrary", ImVec2(220.0f, topHeight_), true);
    DrawLibraryPanel();
    ImGui::EndChild();
    ImGui::SameLine();
    const float previewWidth_ = (std::max)(200.0f, available_.x - 650.0f);
    ImGui::BeginChild("SkillPreview", ImVec2(previewWidth_, topHeight_), true);
    DrawPreviewPanel();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("SkillInspector", ImVec2(0.0f, topHeight_), true);
    DrawInspector();
    ImGui::EndChild();
    ImGui::BeginChild("SkillTimelineAndEvents", ImVec2(0.0f, 0.0f), true);
    if (ImGui::BeginTabBar("SkillEditingTabs"))
    {
        if (ImGui::BeginTabItem("Phases")) { DrawTimeline(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Action Sequence"))
        {
            const auto target_ = targetObject.lock();
            const auto sequence_ = target_ ? target_->GetComponent<ActionSequence>() : nullptr;
            if (sequence_) sequence_->DrawEditorGui();
            else if (target_ && ImGui::Button("Add ActionSequence")) target_->GetOrAddComponent<ActionSequence>();
            else ImGui::TextDisabled("Select an actor to edit timed actions.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Input Bindings")) { DrawInputBindings(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Player / AI")) { DrawExecutionPanel(); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();
    EndEditorWindow();
    const auto target_ = targetObject.lock();
    const auto animation_ = target_ ? target_->GetComponent<AnimationControllerComponent>() : nullptr;
    if (animation_ && animation_->IsSequencerWindowVisible()) animation_->ShowSequencerEditor();
}

void SkillWorkspaceWindow::DrawToolbar()
{
    ImGui::SetNextItemWidth(420.0f);
    ImGui::InputText("Library", &libraryPath);
    ImGui::SameLine();
    if (ImGui::Button("Load")) LoadLibrary();
    ImGui::SameLine();
    if (ImGui::Button("Save")) SaveLibrary();
    ImGui::SameLine();
    ImGui::Checkbox("Simulate Scene", &simulateScene);
    ImGui::SameLine();
    if (ImGui::Button("Save & Play")) PlaySelected();
    ImGui::SameLine();
    if (ImGui::Button("Stop")) StopSelected();
    if (dirty) { ImGui::SameLine(); ImGui::TextUnformatted("Unsaved"); }
    ImGui::Checkbox("Grid", &showGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Free Camera", &editorCamera.enablePreviewFreeCamera);
    ImGui::SameLine();
    if (ImGui::Button("Reset Camera")) editorCamera.ResetPreviewCamera(previewCamera);
    ImGui::TextWrapped("%s", statusText.c_str());
}

void SkillWorkspaceWindow::DrawLibraryPanel()
{
    if (ImGui::Button("New Skill")) AddSkill(false);
    ImGui::SameLine();
    if (selectedSkill >= 0 && ImGui::Button("Copy")) AddSkill(true);
    for (int index_ = 0; index_ < static_cast<int>(document["Skills"].size()); ++index_)
    {
        const std::string name_ = index_ == selectedSkill ? draft.name : document["Skills"][index_].value("Name", std::string());
        ImGui::PushID(index_);
        if (ImGui::Selectable(name_.c_str(), selectedSkill == index_)) SelectSkill(index_);
        ImGui::PopID();
    }
    if (selectedSkill >= 0 && ImGui::Button("Remove Skill"))
    {
        document["Skills"].erase(document["Skills"].begin() + selectedSkill);
        selectedSkill = -1;
        selectedPhase = -1;
        timelineEditing = false;
        dirty = true;
        if (!document["Skills"].empty()) SelectSkill(0);
    }
    ImGui::SeparatorText("Target Actor");
    const auto target_ = targetObject.lock();
    if (gameObjectManager && ImGui::BeginCombo("Actor", target_ ? target_->GetName().c_str() : "Select"))
    {
        for (const auto& object_ : gameObjectManager->GetObjects())
        {
            if (!object_ || !object_->GetComponent<AnimationControllerComponent>()) continue;
            ImGui::PushID(object_.get());
            if (ImGui::Selectable(object_->GetName().c_str(), object_ == target_))
            {
                gameObjectManager->SetSelectedObject(object_);
                targetObject = object_;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
}

void SkillWorkspaceWindow::DrawInspector()
{
    if (selectedSkill < 0) { ImGui::TextDisabled("Create or select a skill."); return; }
    dirty |= ImGui::InputText("Name", &draft.name);
    dirty |= ImGui::Checkbox("Enabled", &draft.enabled);
    dirty |= ImGui::DragFloat("Cooldown", &draft.cooldown, 0.05f, 0.0f, 300.0f);
    dirty |= DrawActionField("Cleanup Action", draft.cleanupAction, targetObject.lock());
    if (ImGui::BeginCombo("Category", GetSkillCategoryName(draft.category)))
    {
        for (int index_ = 0; index_ <= static_cast<int>(SkillCategory::UTILITY); ++index_)
        {
            const auto category_ = static_cast<SkillCategory>(index_);
            if (ImGui::Selectable(GetSkillCategoryName(category_), draft.category == category_))
            { draft.category = category_; dirty = true; }
        }
        ImGui::EndCombo();
    }
    dirty |= ImGui::Checkbox("Can Use On Ground", &draft.canUseOnGround);
    dirty |= ImGui::Checkbox("Can Use In Air", &draft.canUseInAir);
    ImGui::SeparatorText("Phases");
    if (ImGui::Button("Add Phase"))
    {
        draft.phases.push_back(SkillPhase());
        selectedPhase = static_cast<int>(draft.phases.size()) - 1;
        dirty = true;
    }
    for (int index_ = 0; index_ < static_cast<int>(draft.phases.size()); ++index_)
    {
        ImGui::PushID(index_);
        const std::string label_ = std::to_string(index_) + ": " + draft.phases[index_].animationState;
        if (ImGui::Selectable(label_.c_str(), selectedPhase == index_)) selectedPhase = index_;
        ImGui::PopID();
    }
    DrawPhaseInspector();
}

void SkillWorkspaceWindow::DrawPhaseInspector()
{
    if (selectedPhase < 0 || selectedPhase >= static_cast<int>(draft.phases.size())) return;
    auto& phase_ = draft.phases[selectedPhase];
    const auto target_ = targetObject.lock();
    const auto animation_ = target_ ? target_->GetComponent<AnimationControllerComponent>() : nullptr;
    dirty |= ImGui::InputText("Animation", &phase_.animationState);
    dirty |= ImGui::InputText("Motion Warp Preset", &phase_.motionWarpPreset);
    if (animation_ && ImGui::BeginCombo("Actor Animations", phase_.animationState.c_str()))
    {
        for (const auto& [name_, state_] : animation_->GetStates())
            if (ImGui::Selectable(name_.c_str(), phase_.animationState == name_)) { phase_.animationState = name_; dirty = true; }
        ImGui::EndCombo();
    }
    int end_ = static_cast<int>(phase_.endCondition);
    const char* endNames_[] = { "Animation End", "Duration", "Action End", "Ground Contact" };
    if (ImGui::Combo("End", &end_, endNames_, 4)) { phase_.endCondition = static_cast<SkillPhaseEnd>(end_); dirty = true; }
    if (phase_.endCondition == SkillPhaseEnd::DURATION || phase_.endCondition == SkillPhaseEnd::GROUND_CONTACT) dirty |= ImGui::DragFloat("Duration", &phase_.duration, 0.01f, 0.0f, 120.0f);
    dirty |= ImGui::DragFloat("Maximum Duration", &phase_.maximumDuration, 0.1f, 0.1f, 300.0f);
    dirty |= ImGui::DragFloat("Blend Duration", &phase_.blendDuration, 0.01f, 0.0f, 10.0f);
    dirty |= ImGui::DragFloat("Forward Speed", &phase_.forwardSpeed, 0.1f, -100.0f, 100.0f);
    dirty |= ImGui::InputText("Forward Speed Curve", &phase_.forwardSpeedCurveName);
    const auto curve_ = target_ ? target_->GetComponent<CurveComponent>() : nullptr;
    if (curve_ && ImGui::BeginCombo("Movement Curves", phase_.forwardSpeedCurveName.c_str()))
    {
        if (ImGui::Selectable("(None)", phase_.forwardSpeedCurveName.empty()))
        { phase_.forwardSpeedCurveName.clear(); dirty = true; }
        for (const auto& name_ : curve_->GetTrackNames())
            if (ImGui::Selectable(name_.c_str(), phase_.forwardSpeedCurveName == name_))
            { phase_.forwardSpeedCurveName = name_; dirty = true; }
        ImGui::EndCombo();
    }
    dirty |= ImGui::DragFloat("Warp Height", &phase_.warpHeight, 0.1f, 0.0f, 100.0f);
    dirty |= ImGui::DragFloat("Vertical Impulse", &phase_.verticalImpulse, 0.1f, -100.0f, 100.0f);
    dirty |= ImGui::Checkbox("Suspend Gravity", &phase_.suspendGravity);
    if (phase_.suspendGravity)
        dirty |= ImGui::DragFloat("Vertical Speed", &phase_.verticalSpeed, 0.1f, -100.0f, 100.0f);
    if (phase_.suspendGravity)
        dirty |= ImGui::InputText("Vertical Speed Curve (seconds)", &phase_.verticalSpeedCurveName);
    else
    {
        phase_.verticalSpeed = 0.0f;
        phase_.verticalSpeedCurveName.clear();
    }
    dirty |= DrawActionField("Sequence", phase_.sequence, target_, "Sequence");
    dirty |= DrawActionField("Enter Action", phase_.enterAction, target_);
    dirty |= DrawActionField("Exit Action", phase_.exitAction, target_);
    dirty |= ImGui::Checkbox("Dodge Cancel", &phase_.canDodgeCancel);
    dirty |= ImGui::Checkbox("Action Branch", &phase_.canActionBranch);
    dirty |= ImGui::Checkbox("Buffer Input", &phase_.canBufferInput);
    dirty |= ImGui::Checkbox("Enable Guard", &phase_.enableGuard);
    dirty |= ImGui::Checkbox("Prevent Flinch", &phase_.preventFlinch);
    if (animation_ && ImGui::Button("Edit Animation Events"))
    {
        if (animation_->SelectStateForEditing(phase_.animationState)) animation_->SetSequencerWindowVisible(true);
        else statusText = "Animation State not found: " + phase_.animationState;
    }
    if (ImGui::Button("Remove Phase"))
    {
        CommitDraft();
        auto& rawPhases_ = document["Skills"][selectedSkill]["Phases"];
        rawPhases_.erase(rawPhases_.begin() + selectedPhase);
        draft.phases.erase(draft.phases.begin() + selectedPhase);
        selectedPhase = -1;
        timelineEditing = false;
        dirty = true;
    }
}

void SkillWorkspaceWindow::DrawInputBindings()
{
    auto& inputs_ = document["Inputs"];
    if (!inputs_.is_array()) { ImGui::TextDisabled("Inputs must be an array."); return; }
    if (ImGui::Button("Add Input Binding"))
    {
        inputs_.push_back({ { "Slot", 1 }, { "ModifierAction", "Player.SkillModifier" },
            { "TriggerAction", "Player.StrongAttack" }, { "Priority", 100 } });
        dirty = true;
    }
    int remove_ = -1;
    for (int index_ = 0; index_ < static_cast<int>(inputs_.size()); ++index_)
    {
        auto& json_ = inputs_[index_];
        ImGui::PushID(index_);
        int slotIndex_ = GetInputSlotIndex(json_);
        std::string modifier_ = json_.value("ModifierAction", std::string());
        std::string trigger_ = json_.value("TriggerAction", std::string());
        int priority_ = json_.value("Priority", 100);
        bool changed_ = ImGui::InputInt("SkillSlot (0..2)", &slotIndex_);
        changed_ |= ImGui::InputText("Modifier Hold", &modifier_);
        changed_ |= ImGui::InputText("Trigger Action", &trigger_);
        changed_ |= ImGui::InputInt("Priority", &priority_);
        if (changed_)
        {
            json_["Slot"] = (std::clamp)(slotIndex_, 0, 2); json_["ModifierAction"] = modifier_; json_["TriggerAction"] = trigger_; json_["Priority"] = priority_;
            json_.erase("Action");
            dirty = true;
        }
        if (ImGui::Button("Remove Binding")) remove_ = index_;
        ImGui::Separator();
        ImGui::PopID();
    }
    if (remove_ >= 0) { inputs_.erase(inputs_.begin() + remove_); dirty = true; }
}

float SkillWorkspaceWindow::GetPhaseDisplayDuration(const SkillPhase& phase_) const
{
    if (phase_.endCondition == SkillPhaseEnd::DURATION) return (std::max)(0.05f, phase_.duration);
    const auto target_ = targetObject.lock();
    const auto animation_ = target_ ? target_->GetComponent<AnimationControllerComponent>() : nullptr;
    const auto model_ = target_ ? target_->GetComponent<GltfModelCommponennt>() : nullptr;
    if (animation_ && model_)
    {
        const auto iterator_ = animation_->GetStates().find(phase_.animationState);
        if (iterator_ != animation_->GetStates().end() && iterator_->second)
        {
            const auto& state_ = *iterator_->second;
            // Custom/Blend StateはモデルのAnimation番号を持たない場合がある。
            if (state_.animationIndex < 0 || state_.animationIndex >= static_cast<int>(model_->GetAnimations().size()))
                return (std::max)(0.05f, phase_.maximumDuration);
            const float duration_ = model_->GetAnimationDuration(state_.animationIndex);
            if (duration_ > 0.0f) return duration_ / (std::max)(0.01f, state_.playbackSpeed);
        }
    }
    return (std::max)(0.05f, phase_.maximumDuration);
}

void SkillWorkspaceWindow::SyncPhaseFrames()
{
    if (timelineEditing) return;
    phaseFrames.clear();
    int start_ = 0;
    for (const auto& phase_ : draft.phases)
    {
        const int end_ = start_ + (std::max)(1, static_cast<int>(std::round(GetPhaseDisplayDuration(phase_) * SKILL_TIMELINE_FPS)));
        phaseFrames.push_back({ start_, end_, start_, end_ });
        start_ = end_;
    }
}

void SkillWorkspaceWindow::DrawTimeline()
{
    if (selectedSkill < 0) return;
    SyncPhaseFrames();
    ImGui::TextUnformatted("Sequential phases / 60 FPS");
    ImGui::TextDisabled("Move bars to reorder. Resize a bar to use Duration. Animation / Action End widths are estimates.");
    PhaseTimelineAdapter adapter_(*this);
    ImSequencer::Sequencer(&adapter_, &timelineFrame, &timelineExpanded, &selectedPhase,
        &timelineFirstFrame, ImSequencer::SEQUENCER_EDIT_STARTEND);
}

void SkillWorkspaceWindow::DrawExecutionPanel()
{
    const auto target_ = targetObject.lock();
    if (!target_) { ImGui::TextDisabled("Select a target actor."); return; }
    ImGui::Text("Actor: %s", target_->GetName().c_str());
    const auto dragon_ = target_->GetComponent<DragonAIComponent>();
    const auto player_ = target_->GetComponent<PlayerControllerComponent>();
    const auto ai_ = target_->GetComponent<AIActionComponent>();
    if (player_)
    {
        player_->DrawSkillLoadoutGui();
        ImGui::TextDisabled("Save the scene to keep the equipped skills.");
        const char* slotNames_[] = { "SkillSlot0 (Square)", "SkillSlot1 (Triangle)", "SkillSlot2 (Circle)" };
        ImGui::Combo("Equip / Preview Slot", &selectedPlayerSlot, slotNames_, 3);
        if (ImGui::Button("Reset Skill Cooldowns")) player_->ResetSkillCooldowns();
    }
    else if (dragon_)
    {
        const auto& actions_ = dragon_->GetActionDefinitions();
        if (selectedDragonAction >= static_cast<int>(actions_.size())) selectedDragonAction = 0;
        if (!actions_.empty() && ImGui::BeginCombo("Dragon Action", GetDragonActionName(actions_[selectedDragonAction].action)))
        {
            for (int index_ = 0; index_ < static_cast<int>(actions_.size()); ++index_)
                if (ImGui::Selectable(GetDragonActionName(actions_[index_].action), index_ == selectedDragonAction)) selectedDragonAction = index_;
            ImGui::EndCombo();
        }
    }
    else
    {
        ImGui::InputText("AI Action Name", &aiActionName);
        if (ai_ && ImGui::BeginCombo("Existing AI Actions", aiActionName.c_str()))
        {
            for (const auto& action_ : ai_->GetActions())
                if (ImGui::Selectable(action_.aiActionName.c_str(), aiActionName == action_.aiActionName)) aiActionName = action_.aiActionName;
            ImGui::EndCombo();
        }
        if (!ai_ && ImGui::Button("Add AIActionComponent")) target_->GetOrAddComponent<AIActionComponent>();
    }
    if (selectedSkill >= 0 && ImGui::Button("Save & Apply Binding"))
        if (SaveLibrary() && ApplyBinding()) statusText = "Binding applied. Save the scene to keep actor settings.";
    const auto blackboard_ = target_->GetComponent<BlackboardComponent>();
    if (blackboard_)
    {
        std::string name_; bool active_ = false; int phase_ = -1; float time_ = 0.0f; int result_ = 0;
        blackboard_->GetString("Skill", "Name", name_);
        blackboard_->GetBool("Skill", "Active", active_);
        blackboard_->GetInt("Skill", "Phase", phase_);
        blackboard_->GetFloat("Skill", "PhaseTime", time_);
        blackboard_->GetInt("Skill", "Result", result_);
        ImGui::Text("Current Skill: %s / Active: %s / Phase: %d / %.2f sec / Result: %d",
            name_.c_str(), active_ ? "true" : "false", phase_, time_, result_);
    }
}

bool SkillWorkspaceWindow::ApplyBinding()
{
    const auto target_ = targetObject.lock();
    if (!target_ || selectedSkill < 0) return false;
    if (const auto player_ = target_->GetComponent<PlayerControllerComponent>())
    {
        if (!player_->ReloadSkillDefinitions(libraryPath)) { statusText = "Player library reload failed or a skill is active."; return false; }
        if (!player_->SetEquippedSkill(static_cast<std::size_t>(selectedPlayerSlot), draft.name)) { statusText = "Cannot equip the selected skill."; return false; }
        return true;
    }
    if (const auto dragon_ = target_->GetComponent<DragonAIComponent>())
    {
        const auto& actions_ = dragon_->GetActionDefinitions();
        if (selectedDragonAction < 0 || selectedDragonAction >= static_cast<int>(actions_.size())) return false;
        if (!dragon_->SetSkillForAction(actions_[selectedDragonAction].action, draft.name, libraryPath))
        { statusText = "Dragon binding failed or a skill is active."; return false; }
        return true;
    }
    const auto ai_ = target_->GetComponent<AIActionComponent>();
    if (!ai_) { statusText = "Add AIActionComponent to this actor first."; return false; }
    if (!ai_->SetSkillForAction(aiActionName, draft.name, libraryPath))
    { statusText = "AI binding failed or a skill is active."; return false; }
    return true;
}

void SkillWorkspaceWindow::PlaySelected()
{
    if (!SaveLibrary() || !ApplyBinding()) return;
    const auto target_ = targetObject.lock();
    const auto animation_ = target_ ? target_->GetComponent<AnimationControllerComponent>() : nullptr;
    if (animation_) animation_->SetAnimationPaused(false);
    bool requested_ = false;
    if (const auto player_ = target_->GetComponent<PlayerControllerComponent>())
    {
        requested_ = player_->RequestSkillSlot(static_cast<std::size_t>(selectedPlayerSlot));
    }
    else if (const auto dragon_ = target_->GetComponent<DragonAIComponent>())
        requested_ = dragon_->RequestAction(dragon_->GetActionDefinitions()[selectedDragonAction].action);
    else if (const auto ai_ = target_->GetComponent<AIActionComponent>()) requested_ = ai_->ExecuteAIAction(aiActionName);
    simulateScene = requested_;
    statusText = requested_ ? "Playing on target actor." : "Request rejected. Check animation, cooldown and cancel gates.";
}

void SkillWorkspaceWindow::StopSelected()
{
    const auto target_ = targetObject.lock();
    if (target_)
    {
        if (const auto player_ = target_->GetComponent<PlayerControllerComponent>()) player_->StopSkill();
        else if (const auto dragon_ = target_->GetComponent<DragonAIComponent>()) dragon_->StopSkill();
        else if (const auto ai_ = target_->GetComponent<AIActionComponent>()) ai_->StopSkill();
    }
    simulateScene = false;
}

void SkillWorkspaceWindow::EnsurePreviewResources()
{
    BS_PROFILE_SCOPE("SkillWorkspaceWindow::EnsurePreviewResources");

    if (isPreviewResourceInitialized) return;

    ID3D11Device* device_ = GraphicsDeviceManager::Instance().GetDevice();
    if (!device_) return;

    // Skill Workspaceの専用RTへ現在のSceneを描画する。
    editorPrimitiveRenderer.Initialize(device_);
    previewModelRenderSystem.Initialize(device_);
    previewParticleRenderSystem.Initialize(device_);
    previewMeshVFXRenderSystem.Initialize(device_);
    editorCamera.ResetPreviewCamera(previewCamera);
    previewCamera.SetPerspectiveFov(45.0f, 16.0f / 9.0f, 0.01f, 1000.0f);

    isPreviewResourceInitialized = true;
}


void SkillWorkspaceWindow::DrawPreviewPanel()
{
    BS_PROFILE_SCOPE("SkillWorkspaceWindow::DrawPreviewPanel");

    ImGui::TextUnformatted("Skill Preview");
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


void SkillWorkspaceWindow::RenderPreview(const ImVec2& previewSize_)
{
    BS_PROFILE_SCOPE("SkillWorkspaceWindow::RenderPreview");

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
