#include "PrecompiledHeader.h"
#include "CinematicSequenceEditor.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/EventSystem/ActionSequence.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"
#include "Engine/Platform/WindowsDialogHelper.h"
#include "Engine/Debug/TracyProfiler.h"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <cstdio>

namespace
{
    // Blackboard書込みを含む既存JSONの項目を、型を維持したまま編集する。
    void DrawValues(nlohmann::json& value_, const std::string& label_)
    {
        ImGui::PushID(label_.c_str());
        if (value_.is_object() || value_.is_array())
        {
            if (ImGui::TreeNode(label_.c_str()))
            {
                for (auto iterator_ = value_.begin(); iterator_ != value_.end(); ++iterator_)
                    DrawValues(iterator_.value(), value_.is_array() ? std::to_string(iterator_ - value_.begin()) : iterator_.key());
                ImGui::TreePop();
            }
        }
        else if (value_.is_boolean())
        {
            bool enabled_ = value_.get<bool>();
            if (ImGui::Checkbox(label_.c_str(), &enabled_)) value_ = enabled_;
        }
        else if (value_.is_string())
        {
            auto text_ = value_.get<std::string>();
            if (ImGui::InputText(label_.c_str(), &text_)) value_ = text_;
        }
        else if (value_.is_number_integer())
        {
            int number_ = value_.get<int>();
            if (ImGui::DragInt(label_.c_str(), &number_)) value_ = number_;
        }
        else if (value_.is_number())
        {
            float number_ = value_.get<float>();
            if (ImGui::DragFloat(label_.c_str(), &number_, 0.01f) && std::isfinite(number_)) value_ = number_;
        }
        ImGui::PopID();
    }
}

CinematicSequenceEditor::~CinematicSequenceEditor() { Stop(); }

bool CinematicSequenceEditor::HasSequence() const
{
    return draft.contains("Sequences") && draft["Sequences"].is_array() && sequenceIndex >= 0 && sequenceIndex < static_cast<int>(draft["Sequences"].size());
}

float CinematicSequenceEditor::GetDuration() const
{
    return std::isfinite(previewDuration) ? std::clamp(previewDuration, 0.1f, 600.0f) : 10.0f;
}

bool CinematicSequenceEditor::Bind(const std::string& ownerName_, int sequenceIndex_)
{
    if (!manager || manager->IsCinematicSequencePreview()) return false;
    const auto owner_ = manager->Find(ownerName_);
    const auto sequence_ = owner_ ? owner_->GetComponent<ActionSequence>() : nullptr;
    if (!sequence_) return false;
    nlohmann::json data_;
    sequence_->Serialize(data_);
    if (sequenceIndex_ < 0 || sequenceIndex_ >= static_cast<int>(data_["Sequences"].size())) return false;
    draft = std::move(data_);
    ownerName = inspectObjectName = ownerName_;
    SelectSequence(sequenceIndex_);
    dirty = restartPending = restartPlaying = false;
    pendingSeek = -1.0f;
    return true;
}

void CinematicSequenceEditor::SelectSequence(int index_)
{
    sequenceIndex = index_;
    selectedAction = -1;
    const auto& sequences_ = draft["Sequences"];
    const auto& selected_ = sequences_[index_];
    const auto name_ = selected_.value("name", std::string());
    previewDuration = selected_.value("duration", 10.0f);
    auto context_ = GameProgressState::GAME_SCENE_START;
    if (name_ == "BossIntro") context_ = GameProgressState::BOSS_INTRO;
    else if (name_ == "Phase2Intro" || name_ == "PhaseTransitionBattleResume") context_ = GameProgressState::PHASE_2_INTRO;
    else if (name_ == "Phase1Defeat")
    {
        context_ = GameProgressState::PHASE_1_DEFEAT;
        for (const auto& sequence_ : sequences_)
            if (sequence_.value("name", std::string()) == "Phase2Intro")
            {
                // 被覆・生成待ちがあるため、連続再生にはSequenceの尺より余裕を持たせる。
                previewDuration += sequence_.value("duration", 0.0f) + 5.0f;
                break;
            }
    }
    contextIndex = static_cast<int>(context_);
    viewDuration = (std::max)(selected_.value("duration", 10.0f), 1.0f);
}

void CinematicSequenceEditor::Stop()
{
    if (ownsPreview && manager) manager->StopCinematicSequencePreview();
    ownsPreview = restartPending = restartPlaying = false;
    pendingSeek = -1.0f;
}

void CinematicSequenceEditor::Start(bool playing_)
{
    if (!manager || !HasSequence()) return;
    if (manager->IsCinematicSequencePreview())
    {
        if (!ownsPreview) return;
        manager->SetCinematicSequencePaused(!playing_);
        return;
    }
    if (!manager->BeginCinematicSequencePreview(ownerName, sequenceIndex, draft, static_cast<GameProgressState>(contextIndex)))
    {
        status = "Stop Editor Play and wait for Scene loading. The source object must be a saved Scene object with an enabled ActionSequence.";
        return;
    }
    ownsPreview = true;
    status.clear();
    manager->SetCinematicSequencePaused(!playing_);
    manager->SetCinematicSequenceRate(rate);
}

void CinematicSequenceEditor::Seek(float time_)
{
    // 手動シークでは、ループの再開予約を引き継がず停止したコマを確認する。
    restartPlaying = false;
    pendingSeek = std::clamp(time_, 0.0f, GetDuration());
    if (ownsPreview) manager->SetCinematicSequencePaused(true);
}

void CinematicSequenceEditor::Apply()
{
    if (!manager || !manager->CanStartEditorPlay() || manager->IsCinematicSequencePreview() || !HasSequence()) return;
    const auto owner_ = manager->Find(ownerName);
    const auto sequence_ = owner_ ? owner_->GetComponent<ActionSequence>() : nullptr;
    if (!sequence_) return;
    sequence_->Deserialize(draft);
    dirty = false;
    status = "Applied to the source ActionSequence. Save the Scene to keep the changes on disk.";
}

void CinematicSequenceEditor::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("CinematicSequenceEditor::Update");
    if (!manager) return;
    if (ownsPreview && !manager->IsCinematicSequencePreview())
    {
        ownsPreview = false;
        status = manager->GetCinematicPreviewError();
    }
    if (ownsPreview) manager->SetCinematicSequenceRate(rate);
    if (restartPending && !manager->IsCinematicSequencePreview() && manager->CanStartEditorPlay())
    {
        restartPending = false;
        Start(restartPlaying);
    }
    seekDelay -= (std::max)(0.0f, deltaTime_);
    if (pendingSeek >= 0.0f && !restartPending && !ImGui::IsMouseDown(ImGuiMouseButton_Left) && seekDelay <= 0.0f)
    {
        Start(false);
        if (ownsPreview)
        {
            manager->SeekCinematicSequencePreview(pendingSeek);
            pendingSeek = -1.0f;
            seekDelay = 0.12f;
        }
        else pendingSeek = -1.0f;
    }
    if (ownsPreview && !manager->IsCinematicSequenceSeeking() && !manager->IsCinematicSequencePaused() && manager->GetCinematicSequenceTime() >= GetDuration())
    {
        if (loop)
        {
            manager->SeekCinematicSequencePreview(0.0f);
            // 再評価完了後にだけ再開し、先頭へのシーク中に通常再生を重ねない。
            restartPlaying = true;
        }
        else manager->SetCinematicSequencePaused(true);
    }
    if (ownsPreview && restartPlaying && !manager->IsCinematicSequenceSeeking())
    {
        restartPlaying = false;
        manager->SetCinematicSequencePaused(false);
    }
}

void CinematicSequenceEditor::DrawActionInspector()
{
    if (!HasSequence()) return;
    auto& sequence_ = draft["Sequences"][sequenceIndex];
    auto name_ = sequence_.value("name", std::string());
    if (ImGui::InputText("Sequence name", &name_)) sequence_["name"] = name_;
    float duration_ = sequence_.value("duration", 10.0f);
    if (ImGui::DragFloat("Sequence duration", &duration_, 0.01f, 0.01f, 600.0f, "%.3fs", ImGuiSliderFlags_AlwaysClamp)) sequence_["duration"] = duration_;
    for (const char* key_ : { "completionAction", "interruptionAction" })
    {
        auto text_ = sequence_.value(key_, std::string());
        if (ImGui::InputText(key_, &text_)) sequence_[key_] = text_;
    }
    auto& actions_ = sequence_["actions"];
    bool cleanup_ = sequence_.value("cleanupObjectsOnCompletion", false);
    if (ImGui::Checkbox("Cleanup spawned objects on completion", &cleanup_)) sequence_["cleanupObjectsOnCompletion"] = cleanup_;
    if (selectedAction >= 0 && selectedAction < static_cast<int>(actions_.size()))
    {
        ImGui::SeparatorText("Selected Action");
        auto& action_ = actions_[selectedAction];
        auto actionName_ = action_.value("name", std::string());
        if (ImGui::InputText("Action name", &actionName_)) action_["name"] = actionName_;
        if (ImGui::BeginCombo("Registered actions", actionName_.c_str()))
        {
            const auto owner_ = manager->Find(ownerName);
            const auto choices_ = [&](ActionRegistry& registry_, const char* scope_)
            {
                for (const auto& category_ : registry_.GetCategories())
                    if (ImGui::TreeNode((std::string(scope_) + "/" + category_).c_str()))
                    {
                        for (const auto& candidate_ : registry_.GetActionNames(category_))
                            if (ImGui::Selectable(candidate_.c_str(), actionName_ == candidate_)) { action_["name"] = candidate_; action_["cat"] = category_; }
                        ImGui::TreePop();
                    }
            };
            if (owner_) choices_(owner_->localRegistry, "Local");
            choices_(ActionRegistry::Instance(), "Global");
            ImGui::EndCombo();
        }
        float time_ = action_.value("time", 0.0f);
        const bool normalized_ = !action_.value("animationState", std::string()).empty() && action_.value("useNormalizedTime", false);
        if (ImGui::DragFloat("Execution time", &time_, 1.0f / frameRate, 0.0f, normalized_ ? 1.0f : 600.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp)) action_["time"] = time_;
        for (const char* key_ : { "waitForObject", "animationState" })
        {
            auto text_ = action_.value(key_, std::string());
            if (ImGui::InputText(key_, &text_)) action_[key_] = text_;
        }
        bool normalizedTime_ = action_.value("useNormalizedTime", false);
        if (ImGui::Checkbox("Normalized animation time", &normalizedTime_))
        {
            action_["useNormalizedTime"] = normalizedTime_;
            if (normalizedTime_ && !action_.value("animationState", std::string()).empty()) action_["time"] = std::clamp(action_.value("time", 0.0f), 0.0f, 1.0f);
        }
        if (action_.contains("blackboardWrites")) DrawValues(action_["blackboardWrites"], "Blackboard writes");
        if (ImGui::Button("Add Blackboard write")) action_["blackboardWrites"].push_back({{"category", "Attack"}, {"key", "Value"}, {"type", 0}, {"floatValue", 0.0f}, {"intValue", 0}, {"boolValue", false}});
        if (ImGui::Button("Duplicate Action")) { const auto copy_ = action_; actions_.push_back(copy_); selectedAction = static_cast<int>(actions_.size()) - 1; }
        ImGui::SameLine();
        if (ImGui::Button("Delete Action")) { actions_.erase(selectedAction); selectedAction = -1; }
    }
    if (ImGui::Button("Add Action"))
    {
        actions_.push_back({{"cat", ""}, {"name", ""}, {"time", 0.0f}, {"waitForObject", ""}, {"animationState", ""}, {"useNormalizedTime", false}, {"blackboardWrites", nlohmann::json::array()}});
        selectedAction = static_cast<int>(actions_.size()) - 1;
    }
}

void CinematicSequenceEditor::DrawLiveState()
{
    BS_PROFILE_SCOPE("CinematicSequenceEditor::LiveState");
    if (const auto owner_ = manager->Find(ownerName))
        if (const auto sequence_ = owner_->GetComponent<ActionSequence>())
            ImGui::TextWrapped("Sequence: %s / %.3fs %s", sequence_->GetPlayingSequenceName().c_str(), sequence_->GetPlaybackTime(), sequence_->IsWaitingForObject() ? "Waiting for object resources..." : "");
    for (const auto& object_ : manager->GetObjects())
    {
        if (!object_ || object_->IsPendingDestroy()) continue;
        if (const auto animation_ = object_->GetComponent<AnimationControllerComponent>())
            if (const auto state_ = animation_->GetCurrentState())
                ImGui::TextWrapped("%s / %s : %.3f / %.3fs", object_->GetName().c_str(), state_->name.c_str(), animation_->GetAnimatorTime(), animation_->GetCurrentSampleDuration());
        if (object_->GetName() == inspectObjectName)
        {
            if (const auto particle_ = object_->GetComponent<ParticleComponent>())
                for (const auto& track_ : particle_->GetTracks())
                    ImGui::TextWrapped("%s life %.2f - %.2fs, delay %.2fs", track_.name.c_str(), track_.settings.lifespan.x, track_.settings.lifespan.y, track_.delay);
            if (const auto mesh_ = object_->GetComponent<MeshVFXComponent>())
            {
                ImGui::Text("MeshVFX time: %.3fs", mesh_->GetTime());
                for (const auto& part_ : mesh_->GetAsset().parts)
                {
                    if (part_.duration > 0.0f) ImGui::Text("Part: %.3fs %s", part_.duration, part_.loop ? "loop" : "one-shot");
                    else ImGui::TextUnformatted("Part: no duration limit");
                }
            }
        }
    }
}

void CinematicSequenceEditor::DrawSceneInspector()
{
    ImGui::TextWrapped("Stop preview to edit the Scene's camera, actor and VFX settings with their existing inspectors.");
    if (ImGui::BeginCombo("Inspect object", inspectObjectName.c_str()))
    {
        for (const auto& object_ : manager->GetObjects())
            if (object_ && !object_->IsPendingDestroy() && ImGui::Selectable(object_->GetName().c_str(), inspectObjectName == object_->GetName())) inspectObjectName = object_->GetName();
        ImGui::EndCombo();
    }
    if (manager->IsCinematicSequencePreview() || !manager->CanStartEditorPlay()) return;
    const auto object_ = manager->Find(inspectObjectName);
    if (!object_) return;
    const auto components_ = object_->components;
    for (const auto& component_ : components_)
        if (component_ && component_->GetName() != "ActionSequence")
        {
            ImGui::PushID(component_.get());
            if (ImGui::CollapsingHeader(component_->GetName().c_str())) component_->DrawGuiProfiled();
            ImGui::PopID();
        }
}

void CinematicSequenceEditor::DrawTimeline()
{
    BS_PROFILE_SCOPE("CinematicSequenceEditor::Timeline");
    ImGui::Checkbox("Snap", &snap); ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f); ImGui::SliderInt("FPS", &frameRate, 24, 120); ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f); ImGui::DragFloat("View seconds", &viewDuration, 0.1f, 0.5f, 600.0f);
    ImGui::SameLine(); if (ImGui::Button("Fit timeline")) viewDuration = (std::max)(1.0f, GetDuration());
    viewDuration = std::isfinite(viewDuration) ? std::clamp(viewDuration, 0.5f, 600.0f) : 12.0f;
    ImGui::BeginChild("Sequence timeline", ImVec2(0, 0), true);
    constexpr float LABEL_WIDTH = 260.0f;
    constexpr float ROW_HEIGHT = 28.0f;
    const auto origin_ = ImGui::GetCursorScreenPos();
    const float width_ = (std::max)(320.0f, ImGui::GetContentRegionAvail().x);
    const float left_ = origin_.x + LABEL_WIDTH;
    const float scale_ = (width_ - LABEL_WIDTH) / viewDuration;
    const auto drawList_ = ImGui::GetWindowDrawList();
    ImGui::SetCursorScreenPos(ImVec2(left_, origin_.y));
    ImGui::InvisibleButton("Sequence ruler", ImVec2(width_ - LABEL_WIDTH, 28.0f));
    if (ImGui::IsItemActive()) Seek((ImGui::GetIO().MousePos.x - left_) / scale_);
    auto& actions_ = draft["Sequences"][sequenceIndex]["actions"];
    const float bottom_ = origin_.y + ROW_HEIGHT * static_cast<float>(actions_.size() + 2);
    const float interval_ = viewDuration <= 5.0f ? 0.5f : viewDuration <= 20.0f ? 1.0f : 5.0f;
    for (float time_ = 0.0f; time_ <= viewDuration; time_ += interval_)
    {
        const float x_ = left_ + time_ * scale_;
        drawList_->AddLine(ImVec2(x_, origin_.y + 22.0f), ImVec2(x_, bottom_), IM_COL32(150, 165, 185, 45));
        char text_[32]{}; std::snprintf(text_, sizeof(text_), "%.1fs", time_);
        drawList_->AddText(ImVec2(x_ + 3.0f, origin_.y), IM_COL32(210, 220, 235, 255), text_);
    }
    for (int index_ = 0; index_ < static_cast<int>(actions_.size()); ++index_)
    {
        auto& action_ = actions_[index_];
        const float y_ = origin_.y + ROW_HEIGHT * static_cast<float>(index_ + 1);
        ImGui::PushID(index_);
        ImGui::SetCursorScreenPos(ImVec2(origin_.x, y_));
        const auto label_ = action_.value("name", std::string("(Action)"));
        if (ImGui::Selectable(label_.c_str(), selectedAction == index_, 0, ImVec2(LABEL_WIDTH - 8.0f, ROW_HEIGHT - 2.0f))) selectedAction = index_;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", label_.c_str());
        if (!action_.value("animationState", std::string()).empty())
        {
            drawList_->AddText(ImVec2(left_ + 4.0f, y_ + 3.0f), IM_COL32(220, 170, 240, 255), "Animation-bound (see Inspector)");
        }
        else
        {
            const float x_ = left_ + action_.value("time", 0.0f) * scale_;
            if (x_ >= left_ && x_ <= origin_.x + width_)
            {
                drawList_->AddQuadFilled(ImVec2(x_, y_ + 2.0f), ImVec2(x_ + 7.0f, y_ + 13.0f), ImVec2(x_, y_ + 24.0f), ImVec2(x_ - 7.0f, y_ + 13.0f),
                    selectedAction == index_ ? IM_COL32(255, 220, 130, 255) : IM_COL32(70, 170, 225, 255));
                ImGui::SetCursorScreenPos(ImVec2((std::max)(left_, x_ - 9.0f), y_));
                ImGui::InvisibleButton("Action key", ImVec2(18.0f, ROW_HEIGHT - 1.0f));
                if (ImGui::IsItemActivated()) { selectedAction = index_; dragMouseX = ImGui::GetIO().MousePos.x; dragTime = action_.value("time", 0.0f); }
                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f))
                {
                    float time_ = (std::max)(0.0f, dragTime + (ImGui::GetIO().MousePos.x - dragMouseX) / scale_);
                    if (snap) time_ = std::round(time_ * frameRate) / frameRate;
                    action_["time"] = time_;
                }
            }
        }
        ImGui::PopID();
    }
    const float time_ = pendingSeek >= 0.0f ? pendingSeek : manager->GetCinematicSequenceTime();
    const float x_ = left_ + time_ * scale_;
    drawList_->PushClipRect(ImVec2(left_, origin_.y), ImVec2(origin_.x + width_, bottom_), true);
    drawList_->AddLine(ImVec2(x_, origin_.y), ImVec2(x_, bottom_), IM_COL32(255, 235, 180, 255), 2.0f);
    drawList_->PopClipRect();
    ImGui::SetCursorScreenPos(ImVec2(origin_.x, bottom_)); ImGui::Dummy(ImVec2(width_, 1.0f));
    ImGui::EndChild();
}

void CinematicSequenceEditor::Draw()
{
    BS_PROFILE_SCOPE("CinematicSequenceEditor::Draw");
    if (!manager) return;
    ImGui::BeginDisabled(manager->IsCinematicSequencePreview());
    if (ImGui::BeginCombo("ActionSequence owner", ownerName.c_str()))
    {
        for (const auto& object_ : manager->GetObjects())
            if (object_ && object_->saveFlg && object_->GetComponent<ActionSequence>() && ImGui::Selectable(object_->GetName().c_str(), ownerName == object_->GetName())) Bind(object_->GetName(), 0);
        ImGui::EndCombo();
    }
    if (HasSequence() && ImGui::BeginCombo("Sequence", draft["Sequences"][sequenceIndex].value("name", std::string()).c_str()))
    {
        for (int index_ = 0; index_ < static_cast<int>(draft["Sequences"].size()); ++index_)
            if (ImGui::Selectable(draft["Sequences"][index_].value("name", std::string()).c_str(), index_ == sequenceIndex)) SelectSequence(index_);
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    if (!HasSequence()) { ImGui::TextWrapped("Select Cinematic editing in the Action Sequence Editor, or choose a Scene object here."); return; }
    const bool active_ = manager->IsCinematicSequencePreview();
    const bool controls_ = !active_ || ownsPreview;
    ImGui::BeginDisabled(!controls_);
    if (ImGui::Button(active_ && !manager->IsCinematicSequencePaused() ? "Pause" : "Play")) Start(!active_ || manager->IsCinematicSequencePaused());
    ImGui::SameLine(); if (ImGui::Button("Stop + Restore Scene")) Stop();
    ImGui::SameLine(); if (ImGui::Button("< Frame")) Seek(manager->GetCinematicSequenceTime() - 1.0f / frameRate);
    ImGui::SameLine(); if (ImGui::Button("Frame >")) Seek(manager->GetCinematicSequenceTime() + 1.0f / frameRate);
    ImGui::SameLine(); ImGui::SetNextItemWidth(110.0f); ImGui::SliderFloat("Speed", &rate, 0.1f, 2.0f, "%.2fx");
    ImGui::SameLine(); ImGui::Checkbox("Loop", &loop);
    ImGui::SameLine(); ImGui::Text("%.3fs %s", manager->GetCinematicSequenceTime(), manager->IsCinematicSequenceSeeking() ? "Replaying / loading..." : "");
    ImGui::SetNextItemWidth(130.0f); ImGui::DragFloat("Preview end", &previewDuration, 0.1f, 0.1f, 600.0f);
    const auto& sequences_ = draft["Sequences"];
    const bool hasLeadIn_ = sequences_[sequenceIndex].value("name", std::string()) == "Phase2Intro" &&
        std::any_of(sequences_.begin(), sequences_.end(), [](const auto& sequence_)
            { return sequence_.value("name", std::string()) == "Phase1Defeat"; });
    if (hasLeadIn_)
    {
        ImGui::TextDisabled("Phase1Defeat runs first. The timeline starts at Phase2Intro.");
    }
    else
    {
        ImGui::SameLine(); ImGui::SetNextItemWidth(170.0f);
        ImGui::BeginDisabled(active_);
        const char* CONTEXTS[] = {"Game start", "Boss intro", "Phase 1 battle", "Phase 1 defeat", "Phase 2 intro", "Phase 2 battle", "Boss defeated", "Result"};
        ImGui::Combo("Initial GameMode context", &contextIndex, CONTEXTS, IM_ARRAYSIZE(CONTEXTS));
        ImGui::EndDisabled();
    }
    if (ImGui::Button("Refresh preview from draft") && active_)
    {
        manager->StopCinematicSequencePreview(); ownsPreview = false;
        restartPending = true; restartPlaying = false;
    }
    ImGui::SameLine(); ImGui::BeginDisabled(active_ || !manager->CanStartEditorPlay());
    if (ImGui::Button("Apply Sequence to Scene")) Apply();
    ImGui::SameLine();
    if (ImGui::Button("Save Scene") && manager->CanStartEditorPlay())
    {
        Apply();
        const auto scene_ = manager->CaptureStandaloneScene();
        manager->SaveScene(scene_.value("SceneName", std::string("MainScene")));
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Export Sequence data"))
    {
        const auto path_ = WindowsDialogHelper::SaveJsonDialog(L"Export ActionSequence", "Data/ActionSequences/Cinematic.json");
        if (!path_.empty())
        {
            std::ofstream file_(WindowsDialogHelper::Utf8ToPath(path_), std::ios::binary);
            for (const char character_ : draft.dump(4)) { if (character_ == '\n') file_.put('\r'); file_.put(character_); }
            file_ << "\r\n"; file_.flush();
            status = file_ ? "ActionSequence data exported." : "Export failed.";
        }
    }
    ImGui::EndDisabled();
    ImGui::TextWrapped("%s %s", dirty ? "Draft changed. Refresh preview / Apply to Scene." : "", status.c_str());
    const auto before_ = draft;
    const float height_ = (std::max)(180.0f, ImGui::GetContentRegionAvail().y * 0.57f);
    if (ImGui::BeginTable("Generic cinematic panels", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthFixed, 370.0f);
        ImGui::TableNextColumn(); ImGui::BeginChild("Actual Scene preview", ImVec2(0, height_), true);
        const auto texture_ = GraphicsDeviceManager::Instance().GetSceneViewSRV();
        const auto available_ = ImGui::GetContentRegionAvail();
        const auto& graphics_ = GraphicsDeviceManager::Instance();
        const float aspect_ = static_cast<float>((std::max)(graphics_.GetRenderWidth(), 1)) / (std::max)(graphics_.GetRenderHeight(), 1);
        const float width_ = (std::min)(available_.x, available_.y * aspect_);
        if (texture_ && width_ > 1.0f) ImGui::Image(reinterpret_cast<ImTextureID>(texture_), ImVec2(width_, width_ / aspect_));
        ImGui::EndChild();
        ImGui::TableNextColumn(); ImGui::BeginChild("Sequence / Live inspector", ImVec2(0, height_), true);
        if (ImGui::BeginTabBar("Sequence inspector tabs"))
        {
            if (ImGui::BeginTabItem("Action")) { DrawActionInspector(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Live actors / VFX")) { DrawLiveState(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Scene settings")) { DrawSceneInspector(); ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }
        ImGui::EndChild(); ImGui::EndTable();
    }
    DrawTimeline();
    if (before_ != draft)
    {
        dirty = true;
        draft["Sequences"][sequenceIndex]["cinematicEditing"] = true;
        auto& sequence_ = draft["Sequences"][sequenceIndex];
        for (const auto& action_ : sequence_["actions"])
            if (action_.value("animationState", std::string()).empty())
                sequence_["duration"] = (std::max)(sequence_.value("duration", 0.1f), action_.value("time", 0.0f));
        previewDuration = (std::max)(previewDuration, sequence_.value("duration", 0.1f));
    }
}
