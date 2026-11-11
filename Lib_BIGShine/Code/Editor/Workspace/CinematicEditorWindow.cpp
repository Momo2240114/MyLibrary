#include "PrecompiledHeader.h"
#include "CinematicEditorWindow.h"
#include "CinematicSequenceEditor.h"
#include "Editor/Window/AssetBrowserWindow.h"
#include "Editor/AssetBrowserPayload.h"
#include "Game/GameMode/BossFinishDirector.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/EventSystem/ActionSequence.h"
#include "Engine/Platform/WindowsDialogHelper.h"
#include "Engine/Debug/TracyProfiler.h"
#include <imgui_stdlib.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <cstdio>

namespace
{
    constexpr const char* GAME_PRESET = "Data/Presentation/AnathemaBossFinish.json";
    constexpr std::size_t MAX_UNDO = 64;

    // JSONの編集値を一時変数へ取り出し、既存のプリセット項目へ戻す。
    void EditNumber(nlohmann::json& value_, const char* key_, float minimum_, float maximum_)
    {
        float number_ = value_.value(key_, minimum_);
        if (ImGui::DragFloat(key_, &number_, 0.01f, minimum_, maximum_, "%.3f", ImGuiSliderFlags_AlwaysClamp)) value_[key_] = number_;
    }

    void EditPosition(nlohmann::json& value_, const char* key_)
    {
        if (!value_.contains(key_) || !value_[key_].is_array() || value_[key_].size() != 3) return;
        float vector_[3]{ value_[key_][0].get<float>(), value_[key_][1].get<float>(), value_[key_][2].get<float>() };
        if (ImGui::DragFloat3(key_, vector_, 0.01f)) value_[key_] = { vector_[0], vector_[1], vector_[2] };
    }

    void EditText(nlohmann::json& value_, const char* key_)
    {
        auto text_ = value_.value(key_, std::string());
        if (ImGui::InputText(key_, &text_)) value_[key_] = text_;
    }

    void EditFlag(nlohmann::json& value_, const char* key_, bool fallback_ = false)
    {
        bool enabled_ = value_.value(key_, fallback_);
        if (ImGui::Checkbox(key_, &enabled_)) value_[key_] = enabled_;
    }

    // 検証済みの文字列Payloadだけを受け付け、バッファ外まで読み進めない。
    std::string ReadAssetDrop()
    {
        std::string path_;
        if (ImGui::BeginDragDropTarget())
        {
            for (const char* type_ : { AssetBrowserPayload::JSON, AssetBrowserPayload::FILE })
                if (const auto payload_ = ImGui::AcceptDragDropPayload(type_))
                {
                    const auto begin_ = static_cast<const char*>(payload_->Data);
                    if (begin_ && payload_->DataSize > 0)
                    {
                        const auto end_ = std::find(begin_, begin_ + payload_->DataSize, '\0');
                        if (end_ != begin_ + payload_->DataSize) path_.assign(begin_, end_);
                    }
                }
            ImGui::EndDragDropTarget();
        }
        return path_;
    }
}

CinematicEditorWindow::CinematicEditorWindow(GameObjectManager* manager_)
    : EditorWorkspaceWindow("Cinematic Editor", manager_), sequenceEditor(std::make_unique<CinematicSequenceEditor>(manager_))
{
    isVisible = false;
    style.backgroundColor = ImVec4(0.055f, 0.065f, 0.085f, 1.0f);
    style.titleBarColor = ImVec4(0.29f, 0.19f, 0.10f, 1.0f);
}

CinematicEditorWindow::~CinematicEditorWindow() { StopPreview(); }

bool CinematicEditorWindow::OpenCinematicSequence(const std::string& ownerName_, int sequenceIndex_)
{
    StopPreview();
    authoringSource = AuthoringSource::ACTION_SEQUENCE;
    return sequenceEditor->Bind(ownerName_, sequenceIndex_);
}

BossFinishDirector* CinematicEditorWindow::GetDirector() const
{
    return gameObjectManager ? gameObjectManager->GetBossFinishDirector() : nullptr;
}

float CinematicEditorWindow::GetTime() const
{
    const auto director_ = GetDirector();
    return seekPending ? seekTime : director_ && ownsPreview ? director_->GetTime() : 0.0f;
}

float CinematicEditorWindow::GetEndTime() const
{
    const auto director_ = GetDirector();
    return director_ ? director_->GetPreviewEndTime(authoringSource == AuthoringSource::BAD_END ?
        BossFinishDirector::Ending::BAD_END : BossFinishDirector::Ending::TRUE_END) : 8.0f;
}

void CinematicEditorWindow::OnVisibilityChanged(bool visible_)
{
    if (!visible_) StopPreview();
}

void CinematicEditorWindow::StartPreview(bool playing_)
{
    BS_PROFILE_SCOPE("CinematicEditor::StartPreview");
    const auto director_ = GetDirector();
    if (!director_ || (gameObjectManager->IsCinematicSequencePreview() && !ownsFinishScene) ||
        (director_->IsActive() && (!director_->IsPreview() || !ownsPreview))) return;
    if (ownsFinishScene && !gameObjectManager->IsCinematicFinishReady())
    {
        playWhenFinishReady = playing_;
        return;
    }
    if (gameObjectManager->GetSceneRuntimeState() != SCENE_RUNTIME_STATE::RUNNING || gameObjectManager->IsPlayerDeathActive())
    {
        status = "Wait for Scene loading / player death presentation to finish before previewing.";
        return;
    }
    if (!director_->IsActive())
    {
        const auto bossName_ = director_->GetSettings().at("Boss").value("Object", std::string());
        if (!ownsFinishScene && bossName_ == "HumanBoss" && !gameObjectManager->Find(bossName_))
        {
            const auto transition_ = gameObjectManager->Find("PhaseTransition");
            const auto sequence_ = transition_ ? transition_->GetComponent<ActionSequence>() : nullptr;
            if (sequence_)
            {
                nlohmann::json data_;
                sequence_->Serialize(data_);
                const auto& sequences_ = data_["Sequences"];
                for (int index_ = 0; index_ < static_cast<int>(sequences_.size()); ++index_)
                {
                    if (sequences_[index_].value("name", std::string()) != "Phase1Defeat") continue;
                    // MainSceneの第2形態は戦闘中に生成されるため、試写用Sceneで形態移行を完了させる。
                    if (gameObjectManager->BeginCinematicSequencePreview(transition_->GetName(), index_, data_,
                        GameProgressState::PHASE_1_DEFEAT, true))
                    {
                        ownsFinishScene = true;
                        playWhenFinishReady = playing_;
                        status = "Preparing phase 2 for the boss finish...";
                    }
                    else status = "Stop Editor Play and wait for the Scene before previewing the boss finish.";
                    return;
                }
            }
        }
        // 遅延ロード中のモデルを使って不完全なプレビューを開始しない。
        for (const char* key_ : { "Player", "Boss" })
        {
            const auto object_ = gameObjectManager->Find(director_->GetSettings().at(key_).value("Object", std::string()));
            const auto model_ = object_ ? object_->GetComponent<GltfModelCommponennt>() : nullptr;
            if (!model_ || !model_->IsModelReady())
            {
                if (ownsFinishScene) StopPreview();
                status = std::string(key_) + ": Scene object/model is unavailable. Set Object in the inspector.";
                return;
            }
        }
        director_->Begin(true, authoringSource == AuthoringSource::BAD_END ?
            BossFinishDirector::Ending::BAD_END : BossFinishDirector::Ending::TRUE_END);
        ownsPreview = director_->IsPreview() && director_->IsActive();
        if (ownsPreview) status = director_->GetStatus();
    }
    if (playing_ && ownsPreview && director_->GetTime() >= GetEndTime())
    {
        director_->SetPreviewCameraOverride(false);
        exploreCamera = false;
        director_->Seek(0.0f);
    }
    director_->SetPreviewPlaybackRate(previewRate);
    director_->SetPaused(!playing_);
}

void CinematicEditorWindow::StopPreview()
{
    if (sequenceEditor) sequenceEditor->Stop();
    const auto director_ = GetDirector();
    if (ownsPreview && director_ && director_->IsPreview()) director_->Stop();
    if (ownsFinishScene && gameObjectManager)
    {
        gameObjectManager->StopCinematicSequencePreview();
        if (!ownsPreview) status.clear();
    }
    ownsFinishScene = playWhenFinishReady = false;
    ownsPreview = seekPending = exploreCamera = false;
    draggingTrack = -1;
    dragMode = DragMode::NONE;
}

void CinematicEditorWindow::RequestSeek(float time_)
{
    seekTime = std::clamp(time_, 0.0f, GetEndTime());
    seekPending = true;
    if (ownsPreview) if (const auto director_ = GetDirector()) director_->SetPaused(true);
}

void CinematicEditorWindow::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("CinematicEditor::Update");
    if (authoringSource == AuthoringSource::ACTION_SEQUENCE) { sequenceEditor->Update(deltaTime_); return; }
    const auto director_ = GetDirector();
    if (!isVisible || !director_) return;
    if (ownsFinishScene && !gameObjectManager->IsCinematicSequencePreview())
    {
        StopPreview();
        status = gameObjectManager->GetCinematicPreviewError();
    }
    if (ownsPreview && (!director_->IsActive() || !director_->IsPreview())) StopPreview();
    if (ownsFinishScene && !ownsPreview && !seekPending && gameObjectManager->IsCinematicFinishReady())
        StartPreview(playWhenFinishReady);
    if (ownsPreview) director_->SetPreviewPlaybackRate(previewRate);
    seekCooldown -= (std::max)(deltaTime_, 0.0f);
    // BadEnd後半は共通区間から再評価するため、ドラッグを離した時点で映像を更新する。
    const bool draggingBadEnd_ = authoringSource == AuthoringSource::BAD_END &&
        seekTime >= director_->GetEndingBranchTime() && ImGui::IsMouseDown(ImGuiMouseButton_Left);
    if (seekPending && !draggingBadEnd_ && (seekCooldown <= 0.0f || !ImGui::IsMouseDown(ImGuiMouseButton_Left)))
    {
        StartPreview(false);
        if (ownsPreview)
        {
            float selectedKeyTime_ = -1.0f;
            const auto& camera_ = director_->GetSettings()["Camera"];
            if (camera_.contains("Keys") && camera_["Keys"].is_array() && selectedCameraKey >= 0 && selectedCameraKey < static_cast<int>(camera_["Keys"].size()))
                selectedKeyTime_ = camera_["Keys"][selectedCameraKey].value("Time", 0.0f);
            director_->SetPreviewCameraOverride(false);
            exploreCamera = false;
            director_->Seek(seekTime);
            // キーの時刻を編集して並び順が変わっても、同じキーをInspectorに残す。
            if (selectedKeyTime_ >= 0.0f)
            {
                selectedCameraKey = -1;
                const auto& keys_ = director_->GetSettings()["Camera"]["Keys"];
                for (int index_ = 0; index_ < static_cast<int>(keys_.size()); ++index_)
                    if (std::abs(keys_[index_].value("Time", 0.0f) - selectedKeyTime_) < 0.0001f) { selectedCameraKey = index_; break; }
            }
            seekPending = false;
            seekCooldown = 0.08f;
        }
        else if (!ownsFinishScene) seekPending = false;
    }
    if (ownsPreview && !director_->IsPaused() && !seekPending)
    {
        const float end_ = loop ? (std::min)(loopEnd, GetEndTime()) : GetEndTime();
        if (director_->GetTime() >= end_)
        {
            if (loop)
            {
                director_->Seek(std::clamp(loopStart, 0.0f, (std::max)(0.0f, end_ - 1.0f / frameRate)));
                director_->SetPaused(false);
            }
            else director_->SetPaused(true);
        }
    }
}

void CinematicEditorWindow::RestoreHistory(bool redo_)
{
    auto& from_ = redo_ ? redoHistory : undoHistory;
    auto& to_ = redo_ ? undoHistory : redoHistory;
    const auto director_ = GetDirector();
    if (!director_ || from_.empty() || editing || (director_->IsActive() && (!ownsPreview || !director_->IsPreview()))) return;
    const float time_ = GetTime();
    to_.push_back(director_->GetSettings());
    director_->GetSettings() = from_.back();
    from_.pop_back();
    dirty = true;
    selectedIndex = selectedCameraKey = -1;
    if (ownsPreview) { RequestSeek(time_); seekCooldown = 0.0f; }
}

void CinematicEditorWindow::CommitEdits(const nlohmann::json& before_)
{
    const auto director_ = GetDirector();
    if (!director_) return;
    if (before_ != director_->GetSettings())
    {
        if (!editing) { editStart = before_; editing = true; }
        dirty = true;
        // 配列の削除直後も、次のDirector更新より先にVFXの対応を作り直す。
        if (ownsPreview)
        {
            RequestSeek(GetTime());
            if (before_.at("Effects").size() != director_->GetSettings().at("Effects").size()) seekCooldown = 0.0f;
        }
    }
    if (editing && !ImGui::IsAnyItemActive())
    {
        undoHistory.push_back(editStart);
        if (undoHistory.size() > MAX_UNDO) undoHistory.erase(undoHistory.begin());
        redoHistory.clear();
        editing = false;
    }
}

void CinematicEditorWindow::DrawToolbar()
{
    const auto director_ = GetDirector();
    if (!director_) return;
    // テキスト編集中のSpaceやCtrl+Zを、再生操作や履歴操作に流さない。
    const bool preparing_ = ownsFinishScene && !gameObjectManager->IsCinematicFinishReady();
    const bool keyboard_ = !preparing_ && (!director_->IsActive() || ownsPreview) &&
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput;
    if (keyboard_ && ImGui::IsKeyPressed(ImGuiKey_Space, false))
    {
        seekPending = false;
        StartPreview(director_->IsPaused() || !ownsPreview);
    }
    if (keyboard_ && ImGui::GetIO().KeyCtrl && !editing && ImGui::IsKeyPressed(ImGuiKey_Z, false)) RestoreHistory(false);
    if (keyboard_ && ImGui::GetIO().KeyCtrl && !editing && ImGui::IsKeyPressed(ImGuiKey_Y, false)) RestoreHistory(true);
    ImGui::BeginDisabled(preparing_);
    if (ImGui::Button(ownsPreview && !director_->IsPaused() ? "Pause" : "Play"))
    {
        seekPending = false;
        StartPreview(director_->IsPaused() || !ownsPreview);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Stop + Restore")) StopPreview();
    ImGui::SameLine();
    if (ImGui::Button("|<")) RequestSeek(0.0f);
    ImGui::SameLine();
    if (ImGui::Button("< Frame")) RequestSeek(GetTime() - 1.0f / frameRate);
    ImGui::SameLine();
    if (ImGui::Button("Frame >")) RequestSeek(GetTime() + 1.0f / frameRate);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    ImGui::SliderFloat("Speed", &previewRate, 0.1f, 2.0f, "%.2fx");
    ImGui::SameLine();
    if (ImGui::Button("Impact")) RequestSeek(director_->GetImpactCueTime());
    ImGui::SameLine();
    if (ImGui::Button("Ending start")) RequestSeek(director_->GetEndingBranchTime());
    ImGui::SameLine(); ImGui::Text("%s  %.3fs / %dF", dirty ? "*" : "", GetTime(), static_cast<int>(std::round(GetTime() * frameRate)));

    ImGui::BeginDisabled(editing);
    if (ImGui::Button("Open preset"))
    {
        const auto path_ = WindowsDialogHelper::OpenJsonDialog(L"Open Cinematic Preset", "Data/Presentation");
        if (!path_.empty())
        {
            StopPreview();
            if (director_->LoadPreset(path_)) { presetPath = path_; dirty = false; undoHistory.clear(); redoHistory.clear(); }
            status = director_->GetStatus();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) { if (director_->SavePreset(presetPath)) dirty = false; status = director_->GetStatus(); }
    ImGui::SameLine();
    if (ImGui::Button("Save As"))
    {
        const auto path_ = WindowsDialogHelper::SaveJsonDialog(L"Save Cinematic Preset", presetPath);
        if (!path_.empty() && director_->SavePreset(path_)) { presetPath = path_; dirty = false; }
        status = director_->GetStatus();
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply to game")) { if (director_->Save()) dirty = false; status = std::string(GAME_PRESET) + ": " + director_->GetStatus(); }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save current settings to %s", GAME_PRESET);
    ImGui::SameLine();
    ImGui::BeginDisabled(undoHistory.empty());
    if (ImGui::Button("Undo")) RestoreHistory(false);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(redoHistory.empty());
    if (ImGui::Button("Redo")) RestoreHistory(true);
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::TextDisabled("%s", presetPath.c_str());
}

void CinematicEditorWindow::DrawPreview()
{
    BS_PROFILE_SCOPE("CinematicEditor::Preview");
    const auto director_ = GetDirector();
    if (!director_) return;
    ImGui::Checkbox("Safe frame", &safeFrame);
    ImGui::SameLine();
    ImGui::BeginDisabled(!ownsPreview);
    if (ImGui::Checkbox("Free camera", &exploreCamera))
    {
        director_->SetPaused(true);
        director_->SetShowFixed(false);
        director_->SetPreviewCameraOverride(exploreCamera);
        if (const auto camera_ = director_->GetCamera()) freeCamera.CaptureFromCamera(*camera_);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(authoringSource == AuthoringSource::BAD_END && GetTime() >= director_->GetEndingBranchTime());
    if (ImGui::Button("Add camera key from view")) AddCameraKey();
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    const auto texture_ = GraphicsDeviceManager::Instance().GetSceneViewSRV();
    const auto available_ = ImGui::GetContentRegionAvail();
    if (!texture_ || available_.x <= 1.0f || available_.y <= 1.0f) { ImGui::TextDisabled("Scene preview is preparing."); return; }
    const auto& graphics_ = GraphicsDeviceManager::Instance();
    const float aspect_ = static_cast<float>((std::max)(graphics_.GetRenderWidth(), 1)) / (std::max)(graphics_.GetRenderHeight(), 1);
    const float width_ = (std::min)(available_.x, available_.y * aspect_);
    const ImVec2 size_(width_, width_ / aspect_);
    const auto cursor_ = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(cursor_.x + (available_.x - size_.x) * 0.5f, cursor_.y + (available_.y - size_.y) * 0.5f));
    ImGui::Image(reinterpret_cast<ImTextureID>(texture_), size_);
    const bool hovered_ = ImGui::IsItemHovered();
    const auto minimum_ = ImGui::GetItemRectMin();
    const auto maximum_ = ImGui::GetItemRectMax();
    const auto dropped_ = ReadAssetDrop();
    if (!dropped_.empty()) AddEffect(dropped_);
    if (exploreCamera && ownsPreview)
        if (const auto camera_ = director_->GetCamera()) freeCamera.UpdatePreviewFreeCamera(*camera_, ImGui::GetIO().DeltaTime, hovered_);
    if (safeFrame)
    {
        const auto drawList_ = ImGui::GetWindowDrawList();
        drawList_->AddRect(ImVec2(minimum_.x + size_.x * 0.05f, minimum_.y + size_.y * 0.05f),
            ImVec2(maximum_.x - size_.x * 0.05f, maximum_.y - size_.y * 0.05f), IM_COL32(240, 210, 140, 140));
        for (int index_ = 1; index_ <= 2; ++index_)
        {
            const float ratio_ = static_cast<float>(index_) / 3.0f;
            drawList_->AddLine(ImVec2(minimum_.x + size_.x * ratio_, minimum_.y), ImVec2(minimum_.x + size_.x * ratio_, maximum_.y), IM_COL32(230, 230, 230, 40));
            drawList_->AddLine(ImVec2(minimum_.x, minimum_.y + size_.y * ratio_), ImVec2(maximum_.x, minimum_.y + size_.y * ratio_), IM_COL32(230, 230, 230, 40));
        }
    }
}

void CinematicEditorWindow::AddEffect(const std::string& path_)
{
    BS_PROFILE_SCOPE("CinematicEditor::AddEffect");
    if (authoringSource == AuthoringSource::BAD_END && GetTime() >= GetDirector()->GetEndingBranchTime())
    {
        status = "Move the playhead into the shared opening to add a VFX clip.";
        return;
    }
    try
    {
        std::ifstream file_(WindowsDialogHelper::Utf8ToPath(path_));
        const auto asset_ = nlohmann::json::parse(file_);
        const auto type_ = asset_.value("AssetType", std::string());
        const bool particle_ = type_ == "ParticleEffect";
        if (!particle_ && type_ != "MeshVFX") { status = "Choose a ParticleEffect or MeshVFX asset."; return; }
        auto& effects_ = GetDirector()->GetSettings()["Effects"];
        const float origin_ = GetDirector()->GetSettings().value("HitStop", 0.16f);
        const float time_ = (std::max)(0.0f, GetTime() - origin_);
        effects_.push_back({ {"Kind", particle_ ? "Particle" : "Mesh"}, {"Asset", path_},
            {"Position", {0.0f, 0.0f, 0.0f}}, {"Rotation", {0.0f, 0.0f, 0.0f}}, {"Scale", {1.0f, 1.0f, 1.0f}},
            {"Start", time_}, {"Stop", time_ + 1.0f}, {"PoseTime", 1.0f}, {"Color", {1.0f, 1.0f, 1.0f, 1.0f}},
            {"Emission", 1.0f}, {"Anchor", "Impact"}, {"Offset", {0.0f, 0.0f, 0.0f}}, {"RequiresImpact", false} });
        selectedType = TrackType::EFFECT; selectedIndex = static_cast<int>(effects_.size()) - 1;
        status = "VFX added. Start/Stop control visibility; PoseTime controls simulation age.";
    }
    catch (const std::exception& error_) { status = std::string("Asset open failed: ") + error_.what(); }
}

void CinematicEditorWindow::AddCameraKey()
{
    const auto director_ = GetDirector();
    if (!director_ || !ownsPreview) return;
    const auto camera_ = director_->GetCamera();
    if (!camera_) return;
    auto& settings_ = director_->GetSettings();
    auto& keys_ = settings_["Camera"]["Keys"];
    if (!keys_.is_array()) keys_ = nlohmann::json::array();
    const auto& eye_ = camera_->GetEye();
    const auto& focus_ = camera_->GetFocus();
    const auto& anchor_ = settings_["Anchor"];
    keys_.push_back({ {"Time", (std::min)(GetTime(), director_->GetCaptureTime() + settings_.value("HitStop", 0.16f))},
        {"Position", {eye_.x - anchor_[0].get<float>(), eye_.y - anchor_[1].get<float>(), eye_.z - anchor_[2].get<float>()}},
        {"Focus", {focus_.x - anchor_[0].get<float>(), focus_.y - anchor_[1].get<float>(), focus_.z - anchor_[2].get<float>()}},
        {"Fov", settings_["Camera"].value("Fov", 43.0f)}, {"Roll", 0.0f}, {"Easing", "Smooth"} });
    selectedType = TrackType::CAMERA;
    selectedCameraKey = static_cast<int>(keys_.size()) - 1;
}

void CinematicEditorWindow::DrawInspector()
{
    BS_PROFILE_SCOPE("CinematicEditor::Inspector");
    const auto director_ = GetDirector();
    auto& settings_ = director_->GetSettings();
    ImGui::TextUnformatted("Selection / 本番設定");
    ImGui::Separator();
    if (selectedType == TrackType::ENDING)
    {
        const auto cues_ = director_->GetBadEndingCues();
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(cues_.size()))
        {
            const auto& cue_ = cues_[selectedIndex];
            const float branchTime_ = director_->GetEndingBranchTime();
            ImGui::TextUnformatted(cue_.label);
            ImGui::Text("%.3fs - %.3fs", branchTime_ + cue_.start, branchTime_ + cue_.end);
            if (ImGui::Button("Seek to cue")) RequestSeek(branchTime_ + cue_.start);
        }
        ImGui::TextWrapped("BadEnd cue times are read-only. Both endings use the same editable opening preset.");
        return;
    }
    if (selectedType == TrackType::PLAYER || selectedType == TrackType::BOSS)
    {
        const bool boss_ = selectedType == TrackType::BOSS;
        auto& actor_ = settings_[boss_ ? "Boss" : "Player"];
        EditText(actor_, "Object");
        EditText(actor_, "StateAsset");
        if (ImGui::Button("Choose AnimationState"))
        {
            const auto path_ = WindowsDialogHelper::OpenJsonDialog(L"Animation State", "Data/Animation/StateData");
            if (!path_.empty()) actor_["StateAsset"] = path_;
        }
        const auto dropped_ = ReadAssetDrop();
        if (!dropped_.empty()) actor_["StateAsset"] = dropped_;
        const auto info_ = director_->GetPreviewActorInfo(boss_);
        if (info_.ready)
        {
            ImGui::TextWrapped("State / clip: %s", info_.clipName.c_str());
            ImGui::Text("State length: %.3fs / %.0fF", info_.duration, info_.duration * frameRate);
            ImGui::Text("Sample time: %.3fs", info_.sampleTime);
            ImGui::ProgressBar(info_.duration > 0.0f ? std::clamp(info_.sampleTime / info_.duration, 0.0f, 1.0f) : 0.0f);
        }
        else ImGui::TextWrapped("Play/Seek to load actual animation duration. Check the Scene object and AnimationState if unavailable.");
        EditPosition(actor_, "Position");
        EditNumber(actor_, "Yaw", -360.0f, 360.0f);
        EditNumber(actor_, "PoseTime", 0.01f, 30.0f);
        if (!boss_)
        {
            EditPosition(actor_, "EndPosition");
            EditNumber(settings_, "CounterStart", settings_.value("BlackoutEnd", 0.65f), 15.0f);
            EditNumber(actor_, "WindupTime", 0.01f, 3.0f);
            EditNumber(actor_, "WindupSpeed", 0.01f, 5.0f);
            EditNumber(actor_, "StrikeSpeed", 0.01f, 5.0f);
            EditNumber(actor_, "HitPoseTime", 0.01f, 3.0f);
            if (info_.ready && ImGui::Button("Use full State length")) actor_["PoseTime"] = (std::max)(0.01f, info_.duration);
            EditText(actor_, "WeaponObject"); EditText(actor_, "BladeStartNode"); EditText(actor_, "BladeEndNode");
        }
        else
        {
            EditNumber(actor_, "WindupPoseTime", 0.0f, 30.0f);
            EditFlag(actor_, "KeepPoseAnimation");
            EditText(actor_, "ReactionAnimation");
            EditNumber(actor_, "ReactionSpeed", 0.01f, 5.0f);
            EditNumber(actor_, "ReactionPoseTime", 0.0f, 30.0f);
            EditText(actor_, "HitNode"); EditNumber(actor_, "HitRadius", 0.01f, 5.0f);
        }
    }
    else if (selectedType == TrackType::EFFECT && selectedIndex >= 0 && selectedIndex < static_cast<int>(settings_["Effects"].size()))
    {
        auto& effect_ = settings_["Effects"][selectedIndex];
        ImGui::Text("VFX %d (%s)", selectedIndex + 1, effect_.value("Kind", std::string("Mesh")).c_str());
        EditText(effect_, "Asset");
        if (ImGui::Button("Replace asset"))
        {
            const auto path_ = WindowsDialogHelper::OpenJsonDialog(L"VFX Asset", effect_.value("Kind", "Mesh") == "Particle" ? "Data/Particle" : "Data/VFX");
            if (!path_.empty()) effect_["Asset"] = path_;
        }
        EditNumber(effect_, "Start", -25.0f, 25.0f);
        EditNumber(effect_, "Stop", effect_.value("Start", 0.0f) + 1.0f / frameRate, 30.0f);
        EditNumber(effect_, "PoseTime", 0.0f, 30.0f);
        ImGui::TextWrapped("Start/Stop = visibility range. PoseTime = maximum VFX simulation age, including particle trails.");
        // 本番で読み込まれた発生器の寿命を表示し、バーの長さと粒子寿命の違いを確認できるようにする。
        if (ownsPreview && selectedIndex < static_cast<int>(director_->GetEffects().size()))
        {
            const auto& object_ = director_->GetEffects()[selectedIndex];
            if (const auto particle_ = object_->GetComponent<ParticleComponent>())
                for (const auto& track_ : particle_->GetTracks())
                    ImGui::TextWrapped("%s: delay %.2fs / life %.2f - %.2fs (%s)", track_.name.c_str(), track_.delay,
                        track_.settings.lifespan.x, track_.settings.lifespan.y, track_.settings.emissionMode == 0 ? "continuous" : "burst");
            if (const auto mesh_ = object_->GetComponent<MeshVFXComponent>())
                for (const auto& part_ : mesh_->GetAsset().parts)
                {
                    if (part_.duration > 0.0f) ImGui::Text("Mesh part: %.2fs %s", part_.duration, part_.loop ? "loop" : "one-shot");
                    else ImGui::TextUnformatted("Mesh part: no duration limit");
                }
        }
        bool impact_ = effect_.value("TimeAnchor", std::string()) == "Impact";
        if (ImGui::Checkbox("Time relative to Impact", &impact_))
        {
            const float offset_ = director_->GetImpactCueTime() - settings_.value("HitStop", 0.16f);
            effect_["Start"] = effect_.value("Start", 0.0f) + (impact_ ? -offset_ : offset_);
            effect_["Stop"] = effect_.value("Stop", 1.0f) + (impact_ ? -offset_ : offset_);
            effect_["TimeAnchor"] = impact_ ? "Impact" : "Sequence";
        }
        const auto anchor_ = effect_.value("Anchor", std::string("World"));
        if (ImGui::BeginCombo("Anchor", anchor_.c_str()))
        {
            for (const char* name_ : { "Impact", "Boss", "Player", "World" })
                if (ImGui::Selectable(name_, anchor_ == name_))
                {
                    effect_["Anchor"] = name_;
                    if (!effect_.contains("Offset")) effect_["Offset"] = {0.0f, 0.0f, 0.0f};
                }
            ImGui::EndCombo();
        }
        EditFlag(effect_, "RequiresImpact");
        EditText(effect_, "Node");
        EditPosition(effect_, "Position"); EditPosition(effect_, "Offset");
        EditPosition(effect_, "Rotation"); EditPosition(effect_, "Scale");
        float color_[4]{effect_["Color"][0].get<float>(), effect_["Color"][1].get<float>(), effect_["Color"][2].get<float>(), effect_["Color"][3].get<float>()};
        if (ImGui::ColorEdit4("Tint", color_, ImGuiColorEditFlags_HDR)) effect_["Color"] = {color_[0], color_[1], color_[2], color_[3]};
        EditNumber(effect_, "Emission", 0.0f, 30.0f);
        if (ImGui::Button("Duplicate VFX"))
        {
            const auto copy_ = effect_;
            settings_["Effects"].push_back(copy_);
            selectedIndex = static_cast<int>(settings_["Effects"].size()) - 1;
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove VFX")) { settings_["Effects"].erase(selectedIndex); selectedIndex = -1; }
    }
    else if (selectedType == TrackType::CAMERA)
    {
        auto& camera_ = settings_["Camera"];
        ImGui::TextUnformatted("Base camera / keyframed camera");
        EditPosition(camera_, "Position"); EditPosition(camera_, "Focus");
        EditNumber(camera_, "Fov", 15.0f, 100.0f); EditNumber(camera_, "Roll", -180.0f, 180.0f);
        EditNumber(camera_, "Shake", 0.0f, 2.0f); EditNumber(camera_, "ImpactShake", 0.0f, 2.0f);
        EditNumber(camera_, "ImpactShakeDuration", 0.01f, 2.0f);
        EditNumber(camera_, "ImpactFovKick", 0.0f, 30.0f); EditNumber(camera_, "ImpactRoll", -30.0f, 30.0f);
        if (camera_.contains("Keys") && camera_["Keys"].is_array())
        {
            auto& keys_ = camera_["Keys"];
            ImGui::SeparatorText("Camera keys (sequence seconds)");
            for (int index_ = 0; index_ < static_cast<int>(keys_.size()); ++index_)
            {
                ImGui::PushID(index_);
                const auto label_ = std::to_string(index_ + 1) + " : " + std::to_string(keys_[index_].value("Time", 0.0f)) + "s";
                if (ImGui::Selectable(label_.c_str(), selectedCameraKey == index_)) selectedCameraKey = index_;
                ImGui::PopID();
            }
            if (selectedCameraKey >= 0 && selectedCameraKey < static_cast<int>(keys_.size()))
            {
                auto& key_ = keys_[selectedCameraKey];
                EditNumber(key_, "Time", 0.0f, director_->GetCaptureTime() + settings_.value("HitStop", 0.16f));
                EditPosition(key_, "Position"); EditPosition(key_, "Focus");
                EditNumber(key_, "Fov", 15.0f, 100.0f); EditNumber(key_, "Roll", -180.0f, 180.0f);
                const auto easing_ = key_.value("Easing", std::string("Smooth"));
                if (ImGui::BeginCombo("Easing to next key", easing_.c_str()))
                {
                    for (const char* name_ : { "Linear", "Smooth", "EaseIn", "EaseOut" })
                        if (ImGui::Selectable(name_, easing_ == name_)) key_["Easing"] = name_;
                    ImGui::EndCombo();
                }
                if (ImGui::Button("Seek key")) RequestSeek(key_.value("Time", 0.0f));
                ImGui::SameLine();
                if (ImGui::Button("Delete key")) { keys_.erase(selectedCameraKey); selectedCameraKey = -1; }
            }
        }
        ImGui::TextWrapped("No keys: original fixed camera. Keys: position, focus, FOV and roll interpolate through the runtime Curve evaluator.");
    }
    else
    {
        EditNumber(settings_, "HitStop", 0.0f, 1.0f);
        EditNumber(settings_, "BlackoutEnd", 0.1f, 3.0f);
        EditNumber(settings_, "ImpactHold", 0.0f, 0.5f);
        EditNumber(settings_, "CaptureTime", settings_.value("CounterStart", 2.8f) + 0.1f, 25.0f);
        EditNumber(settings_, "HoldTime", 0.0f, 10.0f); EditNumber(settings_, "PromptDelay", 0.0f, 20.0f);
        EditNumber(settings_, "Aspect", 0.5f, 3.5f);
        EditText(settings_, "ClearText"); EditText(settings_, "PromptText");
        EditPosition(settings_, "Anchor");
    }
    ImGui::Separator();
    if (ImGui::Button("Add Particle / MeshVFX"))
    {
        const auto path_ = WindowsDialogHelper::OpenJsonDialog(L"Add Cinematic VFX", "Data");
        if (!path_.empty()) AddEffect(path_);
    }
    ImGui::TextWrapped("%s", status.c_str());
    ImGui::TextWrapped("%s", director_->GetStatus().c_str());
}

void CinematicEditorWindow::BuildTracks()
{
    tracks.clear();
    const auto director_ = GetDirector();
    if (!director_) return;
    const auto& settings_ = director_->GetSettings();
    const float hitStop_ = settings_.value("HitStop", 0.16f);
    const float capture_ = director_->GetEndingBranchTime();
    const auto& player_ = settings_.at("Player");
    const float start_ = hitStop_ + settings_.value("CounterStart", 2.8f);
    const float windup_ = player_.value("WindupTime", 0.35f);
    const float limit_ = player_.value("PoseTime", 0.85f);
    const float windupSpeed_ = (std::max)(player_.value("WindupSpeed", 0.22f), 0.01f);
    const float end_ = (limit_ <= windup_ * windupSpeed_ ? start_ + limit_ / windupSpeed_ :
        start_ + windup_ + (limit_ - windup_ * windupSpeed_) / (std::max)(player_.value("StrikeSpeed", 1.8f), 0.01f)) + settings_.value("ImpactHold", 0.16f);
    tracks.push_back({TrackType::PLAYER, -1, "Player / Animation", start_, (std::min)(end_, capture_), IM_COL32(60, 145, 215, 240)});
    tracks.push_back({TrackType::BOSS, -1, "Boss / Animation", hitStop_, capture_, IM_COL32(165, 85, 190, 240)});
    tracks.push_back({TrackType::CAMERA, -1, "Camera / Keys", 0.0f, capture_, IM_COL32(215, 168, 60, 240)});
    const auto& effects_ = settings_.at("Effects");
    for (int index_ = 0; index_ < static_cast<int>(effects_.size()); ++index_)
    {
        const auto& effect_ = effects_[index_];
        const bool particle_ = effect_.value("Kind", "Mesh") == "Particle";
        const auto anchor_ = effect_.value("TimeAnchor", std::string());
        const float origin_ = anchor_ == "Dissolve" ? director_->GetImpactCueTime() + settings_.value("DissolveDelay", 0.0f) :
            anchor_ == "Impact" ? director_->GetImpactCueTime() : hitStop_;
        const auto file_ = WindowsDialogHelper::PathToUtf8(WindowsDialogHelper::Utf8ToPath(effect_.value("Asset", std::string())).stem());
        tracks.push_back({TrackType::EFFECT, index_, std::to_string(index_ + 1) + " " + file_, origin_ + effect_.value("Start", 0.0f),
            origin_ + effect_.value("Stop", 1.0f), particle_ ? IM_COL32(55, 180, 140, 240) : IM_COL32(225, 115, 70, 240)});
    }
    tracks.push_back({TrackType::IMPACT, -1, "Impact / Hit hold", director_->GetImpactCueTime(), director_->GetImpactCueTime() + settings_.value("ImpactHold", 0.16f), IM_COL32(225, 75, 80, 250)});
    if (authoringSource == AuthoringSource::BAD_END)
    {
        const auto cues_ = director_->GetBadEndingCues();
        for (int index_ = 0; index_ < static_cast<int>(cues_.size()); ++index_)
        {
            const auto& cue_ = cues_[index_];
            tracks.push_back({TrackType::ENDING, index_, cue_.label, capture_ + cue_.start, capture_ + cue_.end, IM_COL32(154, 60, 72, 240)});
        }
    }
    else tracks.push_back({TrackType::CAPTURE, -1, "TrueEnd / Clear UI", capture_, GetEndTime(), IM_COL32(110, 125, 150, 230)});
}

void CinematicEditorWindow::ApplyTrackDrag(const Track& track_, float delta_)
{
    auto& settings_ = GetDirector()->GetSettings();
    const auto quantize_ = [&](float time_) { return snap ? std::round(time_ * frameRate) / frameRate : time_; };
    if (track_.type == TrackType::EFFECT && track_.index >= 0 && track_.index < static_cast<int>(settings_["Effects"].size()))
    {
        auto& effect_ = settings_["Effects"][track_.index];
        const auto anchor_ = effect_.value("TimeAnchor", std::string());
        const float origin_ = anchor_ == "Dissolve" ? GetDirector()->GetImpactCueTime() + settings_.value("DissolveDelay", 0.0f) :
            anchor_ == "Impact" ? GetDirector()->GetImpactCueTime() : settings_.value("HitStop", 0.16f);
        float start_ = dragStart, end_ = dragEnd;
        if (dragMode == DragMode::MOVE)
        {
            start_ = (std::max)(0.0f, quantize_(dragStart + delta_));
            end_ = start_ + dragEnd - dragStart;
        }
        else if (dragMode == DragMode::START) start_ = std::clamp(quantize_(dragStart + delta_), 0.0f, (std::max)(0.0f, end_ - 1.0f / frameRate));
        else end_ = (std::max)(start_ + 1.0f / frameRate, quantize_(dragEnd + delta_));
        effect_["Start"] = start_ - origin_; effect_["Stop"] = end_ - origin_;
    }
    else if (track_.type == TrackType::PLAYER)
        settings_["CounterStart"] = std::clamp(quantize_(dragStart + delta_) - settings_.value("HitStop", 0.16f), settings_.value("BlackoutEnd", 0.65f), 15.0f);
    else if (track_.type == TrackType::CAPTURE)
        settings_["CaptureTime"] = std::clamp(quantize_(dragStart + delta_) - settings_.value("HitStop", 0.16f), settings_.value("CounterStart", 2.8f) + 0.1f, 25.0f);
}

void CinematicEditorWindow::DrawTimeline()
{
    BS_PROFILE_SCOPE("CinematicEditor::Timeline");
    const float endTime_ = GetEndTime();
    loopStart = std::clamp(loopStart, 0.0f, (std::max)(0.0f, endTime_ - 1.0f / frameRate));
    loopEnd = std::clamp(loopEnd, loopStart + 1.0f / frameRate, endTime_);
    ImGui::Checkbox("Loop", &loop); ImGui::SameLine();
    ImGui::SetNextItemWidth(85.0f); ImGui::DragFloat("In", &loopStart, 0.01f, 0.0f, GetEndTime()); ImGui::SameLine();
    ImGui::SetNextItemWidth(85.0f); ImGui::DragFloat("Out", &loopEnd, 0.01f, loopStart + 1.0f / frameRate, GetEndTime()); ImGui::SameLine();
    ImGui::Checkbox("Snap", &snap); ImGui::SameLine();
    ImGui::SetNextItemWidth(65.0f);
    if (ImGui::BeginCombo("FPS", std::to_string(frameRate).c_str()))
    {
        for (const int rate_ : {24, 30, 60}) if (ImGui::Selectable(std::to_string(rate_).c_str(), frameRate == rate_)) frameRate = rate_;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Fit")) { viewStart = 0.0f; viewDuration = GetEndTime(); }
    ImGui::SameLine(); ImGui::SetNextItemWidth(85.0f); ImGui::DragFloat("View", &viewStart, 0.03f, 0.0f, GetEndTime());
    ImGui::SameLine(); ImGui::SetNextItemWidth(85.0f); ImGui::DragFloat("Span", &viewDuration, 0.03f, 0.5f, 50.0f);
    viewDuration = std::clamp(viewDuration, 0.5f, 50.0f);
    ImGui::BeginChild("Timeline rows", ImVec2(0, 0), true);
    constexpr float LABEL_WIDTH = 190.0f;
    constexpr float ROW_HEIGHT = 32.0f;
    const auto drawList_ = ImGui::GetWindowDrawList();
    const auto origin_ = ImGui::GetCursorScreenPos();
    const float width_ = (std::max)(260.0f, ImGui::GetContentRegionAvail().x);
    const float left_ = origin_.x + LABEL_WIDTH;
    const float pixelsPerSecond_ = (width_ - LABEL_WIDTH) / viewDuration;
    const auto x_ = [&](float time_) { return left_ + (time_ - viewStart) * pixelsPerSecond_; };
    ImGui::SetCursorScreenPos(ImVec2(left_, origin_.y));
    ImGui::InvisibleButton("Ruler", ImVec2(width_ - LABEL_WIDTH, 27.0f));
    if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        float time_ = viewStart + (ImGui::GetIO().MousePos.x - left_) / pixelsPerSecond_;
        if (snap) time_ = std::round(time_ * frameRate) / frameRate;
        RequestSeek(time_);
    }
    const float interval_ = viewDuration > 20.0f ? 2.0f : viewDuration > 8.0f ? 1.0f : viewDuration > 3.0f ? 0.5f : 0.1f;
    const float bottom_ = origin_.y + 28.0f + ROW_HEIGHT * static_cast<float>(tracks.size());
    for (float time_ = std::ceil(viewStart / interval_) * interval_; time_ <= viewStart + viewDuration; time_ += interval_)
    {
        drawList_->AddLine(ImVec2(x_(time_), origin_.y + 22.0f), ImVec2(x_(time_), bottom_), IM_COL32(110, 125, 145, 50));
        char label_[32]{}; snprintf(label_, sizeof(label_), "%.2fs", time_);
        drawList_->AddText(ImVec2(x_(time_) + 3.0f, origin_.y), IM_COL32(185, 195, 215, 255), label_);
    }
    for (int index_ = 0; index_ < static_cast<int>(tracks.size()); ++index_)
    {
        const auto& track_ = tracks[index_];
        const float y_ = origin_.y + 28.0f + static_cast<float>(index_) * ROW_HEIGHT;
        const bool selected_ = selectedType == track_.type && selectedIndex == track_.index;
        ImGui::PushID(index_);
        ImGui::SetCursorScreenPos(ImVec2(origin_.x, y_));
        if (ImGui::Selectable(track_.label.c_str(), selected_, 0, ImVec2(LABEL_WIDTH - 6.0f, ROW_HEIGHT - 3.0f)))
        { selectedType = track_.type; selectedIndex = track_.index; }
        drawList_->PushClipRect(ImVec2(left_, y_), ImVec2(origin_.x + width_, y_ + ROW_HEIGHT), true);
        const ImVec2 minimum_(x_(track_.start), y_ + 3.0f);
        const ImVec2 maximum_((std::max)(x_(track_.start) + 4.0f, x_(track_.end)), y_ + ROW_HEIGHT - 4.0f);
        drawList_->AddRectFilled(minimum_, maximum_, track_.color, 4.0f);
        if (selected_) drawList_->AddRect(minimum_, maximum_, IM_COL32(255, 230, 170, 255), 4.0f, 0, 2.0f);
        if (track_.type == TrackType::CAMERA)
        {
            const auto& camera_ = GetDirector()->GetSettings()["Camera"];
            if (camera_.contains("Keys") && camera_["Keys"].is_array())
                for (const auto& key_ : camera_["Keys"])
                {
                    const float keyX_ = x_(key_.value("Time", 0.0f));
                    drawList_->AddQuadFilled(ImVec2(keyX_, y_ + 6.0f), ImVec2(keyX_ + 6.0f, y_ + 15.0f), ImVec2(keyX_, y_ + 24.0f), ImVec2(keyX_ - 6.0f, y_ + 15.0f), IM_COL32(255, 245, 215, 255));
                }
        }
        drawList_->PopClipRect();
        const ImVec2 hitMin_((std::max)(left_, minimum_.x), minimum_.y);
        const ImVec2 hitMax_((std::min)(origin_.x + width_, maximum_.x), maximum_.y);
        if (hitMax_.x > hitMin_.x)
        {
            ImGui::SetCursorScreenPos(hitMin_);
            ImGui::InvisibleButton("Clip", ImVec2(hitMax_.x - hitMin_.x, hitMax_.y - hitMin_.y));
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\n%.3fs - %.3fs\n%s", track_.label.c_str(), track_.start, track_.end,
                track_.type == TrackType::ENDING ? "Click to select cue" : "Drag body: move / VFX edges: resize");
            if (ImGui::IsItemActivated())
            {
                selectedType = track_.type; selectedIndex = track_.index;
                draggingTrack = track_.type == TrackType::ENDING ? -1 : index_; dragMouseX = ImGui::GetIO().MousePos.x;
                dragStart = track_.start; dragEnd = track_.end;
                dragMode = track_.type == TrackType::EFFECT && std::abs(dragMouseX - minimum_.x) < 8.0f ? DragMode::START :
                    track_.type == TrackType::EFFECT && std::abs(dragMouseX - maximum_.x) < 8.0f ? DragMode::END : DragMode::MOVE;
            }
            if (ImGui::IsItemActive() && draggingTrack == index_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f))
                ApplyTrackDrag(track_, (ImGui::GetIO().MousePos.x - dragMouseX) / pixelsPerSecond_);
        }
        ImGui::PopID();
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) { draggingTrack = -1; dragMode = DragMode::NONE; }
    drawList_->PushClipRect(ImVec2(left_, origin_.y), ImVec2(origin_.x + width_, bottom_), true);
    const float impactX_ = x_(GetDirector()->GetImpactCueTime());
    drawList_->AddLine(ImVec2(impactX_, origin_.y + 20.0f), ImVec2(impactX_, bottom_), IM_COL32(255, 100, 100, 180), 1.0f);
    const float branchX_ = x_(GetDirector()->GetEndingBranchTime());
    drawList_->AddLine(ImVec2(branchX_, origin_.y + 20.0f), ImVec2(branchX_, bottom_), IM_COL32(190, 150, 255, 200), 1.0f);
    const float cursorX_ = x_(GetTime());
    drawList_->AddLine(ImVec2(cursorX_, origin_.y), ImVec2(cursorX_, bottom_), IM_COL32(255, 240, 190, 255), 2.0f);
    drawList_->AddTriangleFilled(ImVec2(cursorX_ - 5.0f, origin_.y + 17.0f), ImVec2(cursorX_ + 5.0f, origin_.y + 17.0f), ImVec2(cursorX_, origin_.y + 25.0f), IM_COL32(255, 240, 190, 255));
    drawList_->PopClipRect();
    ImGui::SetCursorScreenPos(ImVec2(origin_.x, bottom_ + 4.0f)); ImGui::Dummy(ImVec2(width_, 1.0f));
    ImGui::EndChild();
}

void CinematicEditorWindow::Draw()
{
    BS_PROFILE_SCOPE("CinematicEditor::Draw");
    if (!BeginWorkspaceWindow()) { if (!isVisible) StopPreview(); EndEditorWindow(); return; }
    int source_ = static_cast<int>(authoringSource);
    if (ImGui::Combo("Cinematic event", &source_, "ActionSequence\0BadEnd\0TrueEnd\0"))
    {
        StopPreview();
        authoringSource = static_cast<AuthoringSource>(source_);
        selectedType = TrackType::PLAYER;
        selectedIndex = selectedCameraKey = -1;
        viewStart = 0.0f;
        viewDuration = GetEndTime();
        loopStart = 0.0f;
        loopEnd = viewDuration;
    }
    if (authoringSource == AuthoringSource::ACTION_SEQUENCE)
    {
        sequenceEditor->Draw();
        EndEditorWindow();
        if (!isVisible) StopPreview();
        return;
    }
    const auto director_ = GetDirector();
    if (!director_) { ImGui::TextDisabled("Scene director is unavailable."); EndEditorWindow(); return; }
    const bool otherPreview_ = director_->IsActive() && (!director_->IsPreview() || !ownsPreview);
    if (otherPreview_) ImGui::TextWrapped("Another runtime/preview owns the director. Stop it in ANATHEMA Boss Finish before editing here.");
    ImGui::BeginDisabled(otherPreview_);
    DrawToolbar();
    const auto before_ = director_->GetSettings();
    BuildTracks();
    const float height_ = (std::max)(160.0f, ImGui::GetContentRegionAvail().y * 0.60f);
    if (ImGui::BeginTable("Cinematic layout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Tracks", ImGuiTableColumnFlags_WidthFixed, 190.0f);
        ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthFixed, 330.0f);
        ImGui::TableNextColumn();
        ImGui::BeginChild("Track list", ImVec2(0, height_), true);
        ImGui::TextUnformatted(authoringSource == AuthoringSource::BAD_END ? "BadEnd" : "TrueEnd");
        ImGui::TextWrapped("Shared opening until %.3fs. Edits to the opening apply to both endings.", director_->GetEndingBranchTime());
        ImGui::Separator();
        for (int index_ = 0; index_ < static_cast<int>(tracks.size()); ++index_)
        {
            const auto& track_ = tracks[index_];
            ImGui::PushID(index_);
            if (ImGui::Selectable(track_.label.c_str(), selectedType == track_.type && selectedIndex == track_.index))
            { selectedType = track_.type; selectedIndex = track_.index; }
            ImGui::PopID();
        }
        ImGui::TextWrapped("Blue: player\nPurple: boss\nGold: camera\nGreen: Particle\nOrange: MeshVFX\nRed: impact");
        ImGui::EndChild();
        ImGui::TableNextColumn();
        ImGui::BeginChild("Cinematic preview", ImVec2(0, height_), true);
        DrawPreview(); ImGui::EndChild();
        ImGui::TableNextColumn();
        ImGui::BeginChild("Cinematic inspector", ImVec2(0, height_), true);
        DrawInspector(); ImGui::EndChild();
        ImGui::EndTable();
    }
    if (ImGui::BeginTabBar("Cinematic tabs"))
    {
        if (ImGui::BeginTabItem("Timeline")) { DrawTimeline(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Assets (drag VFX to preview)"))
        {
            if (!assetBrowser) assetBrowser = std::make_unique<AssetBrowserWindow>(gameObjectManager);
            assetBrowser->DrawContent();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    CommitEdits(before_);
    ImGui::EndDisabled();
    EndEditorWindow();
    if (!isVisible) StopPreview();
}
