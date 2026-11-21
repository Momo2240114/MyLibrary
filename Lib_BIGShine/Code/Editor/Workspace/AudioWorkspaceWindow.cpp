#include "PrecompiledHeader.h"

#include "AudioWorkspaceWindow.h"

#include "Engine/Audio/AudioManager.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <nlohmann/json.hpp>
#include "ImSequencer.h"

namespace
{
    bool IsAudioFile(const std::filesystem::path& path_)
    {
        // エディタのブラウザ対象を実際に扱える音声拡張子だけに絞る。
        const std::string extension_ = path_.extension().string();
        return extension_ == ".wav" || extension_ == ".mp3" || extension_ == ".ogg";
    }
}


struct AudioWorkspaceWindow::AudioSequenceAdapter : public ImSequencer::SequenceInterface
{
    // ImSequencerはフレーム単位で編集するため、秒指定のイベントを一時的なフレーム範囲へ変換して渡す。
    struct FrameRange
    {
        int start = 0;
        int end = 2;
    };

    std::vector<SequenceEvent>* events = nullptr;
    const std::vector<AudioPreset>* presets = nullptr;
    float sequenceLength = 0.0f;
    float fps = 60.0f;
    std::vector<FrameRange> frameRanges;

    int ToFrame(float time_) const
    {
        return static_cast<int>(time_ * fps);
    }

    float ToTime(int frame_) const
    {
        return static_cast<float>(frame_) / fps;
    }

    void SyncFrames()
    {
        // イベント配列の現在値から表示用フレームを作り直し、削除・追加後のズレを防ぐ。
        const int count_ = GetItemCount();
        frameRanges.resize(count_);
        for (int eventIndex_ = 0; eventIndex_ < count_; ++eventIndex_)
        {
            const int frame_ = ToFrame((*events)[eventIndex_].startTime);
            frameRanges[eventIndex_].start = frame_;
            frameRanges[eventIndex_].end = frame_ + 2;
        }
    }

    int GetFrameMin() const override
    {
        return 0;
    }

    int GetFrameMax() const override
    {
        return (std::max)(1, ToFrame(sequenceLength));
    }

    int GetItemCount() const override
    {
        return events ? static_cast<int>(events->size()) : 0;
    }

    int GetItemTypeCount() const override
    {
        return 1;
    }

    const char* GetItemTypeName(int typeIndex_) const override
    {
        return "Audio Event";
    }

    void BeginEdit(int index_) override
    {
    }

    void EndEdit() override
    {
        if (!events)
        {
            return;
        }
        // タイムライン上で動かされたバーを秒へ戻し、シーケンス長の範囲に収める。
        for (int eventIndex_ = 0;
            eventIndex_ < static_cast<int>(events->size()) && eventIndex_ < static_cast<int>(frameRanges.size());
            ++eventIndex_)
        {
            const int clampedFrame_ = (std::max)(GetFrameMin(), (std::min)(GetFrameMax(), frameRanges[eventIndex_].start));
            (*events)[eventIndex_].startTime = (std::max)(0.0f, (std::min)(sequenceLength, ToTime(clampedFrame_)));
            frameRanges[eventIndex_].start = clampedFrame_;
            frameRanges[eventIndex_].end = clampedFrame_ + 2;
        }
    }

    void Get(int index_, int** start_, int** end_, int* type_, unsigned int* color_) override
    {
        if (!events || index_ < 0 || index_ >= static_cast<int>(events->size()))
        {
            return;
        }
        if (frameRanges.size() != events->size())
        {
            SyncFrames();
        }

        if (start_)
        {
            *start_ = &frameRanges[index_].start;
        }
        if (end_)
        {
            *end_ = &frameRanges[index_].end;
        }
        if (type_)
        {
            *type_ = 0;
        }
        if (color_)
        {
            *color_ = 0xFF46D98A;
        }
    }

    const char* GetItemLabel(int index_) const override
    {
        if (!events || index_ < 0 || index_ >= static_cast<int>(events->size()))
        {
            return "";
        }
        return (*events)[index_].name.c_str();
    }

    void CustomDraw(
        int index_,
        ImDrawList* drawList_,
        const ImRect& rc_,
        const ImRect& legendRect_,
        const ImRect& clippingRect_,
        const ImRect& legendClippingRect_) override
    {
        if (!events || index_ < 0 || index_ >= static_cast<int>(events->size()))
        {
            return;
        }

        // バー上にはイベント名と参照プリセット名を出し、音源の割り当てミスを見つけやすくする。
        const SequenceEvent& event_ = (*events)[index_];
        const char* presetName_ = "Missing Preset";
        if (presets && event_.presetIndex >= 0 && event_.presetIndex < static_cast<int>(presets->size()))
        {
            presetName_ = (*presets)[event_.presetIndex].name.c_str();
        }

        const std::string label_ = event_.name + "  [" + presetName_ + "]";
        drawList_->PushClipRect(rc_.Min, rc_.Max, true);
        drawList_->AddText(ImVec2(rc_.Min.x + 3.0f, rc_.Min.y + 2.0f), 0xFFFFFFFF, label_.c_str());
        drawList_->PopClipRect();
    }

    void Add(int type_) override
    {
        if (!events)
        {
            return;
        }

        // 追加イベントは最後のイベントの少し後へ置き、連続追加しても重なりにくい初期値にする。
        SequenceEvent event_;
        event_.name = "Event " + std::to_string(static_cast<int>(events->size()) + 1);
        event_.presetIndex = presets && !presets->empty() ? 0 : -1;
        event_.startTime = events->empty()
            ? 0.0f
            : (std::min)(sequenceLength, events->back().startTime + 0.2f);
        events->push_back(event_);
        SyncFrames();
    }

    void Del(int index_) override
    {
        if (!events || index_ < 0 || index_ >= static_cast<int>(events->size()))
        {
            return;
        }
        events->erase(events->begin() + index_);
        SyncFrames();
    }

    void Duplicate(int index_) override
    {
        if (!events || index_ < 0 || index_ >= static_cast<int>(events->size()))
        {
            return;
        }
        // 複製直後に再生済みフラグが残ると鳴らないため、コピー時は未発火へ戻す。
        SequenceEvent copied_ = (*events)[index_];
        copied_.name += " Copy";
        copied_.startTime = (std::min)(sequenceLength, copied_.startTime + 0.2f);
        copied_.fired = false;
        events->insert(events->begin() + index_ + 1, copied_);
        SyncFrames();
    }
};

AudioWorkspaceWindow::AudioWorkspaceWindow(GameObjectManager* manager_)
    : EditorWorkspaceWindow("Audio Workspace", manager_)
{
    // 空のワークスペースでもすぐ編集できるように、最低1つのプリセットとイベントを用意してから保存データを読む。
    presets.push_back(AudioPreset{});
    sequenceEvents.push_back(SequenceEvent{});
    RefreshAudioFiles();
    LoadWorkspace();
}

void AudioWorkspaceWindow::Update(float deltaTime_)
{
    if (!sequencePlaying)
    {
        return;
    }

    // 再生中は現在時刻を進め、開始時刻を超えたイベントだけを一度ずつ発火する。
    sequenceTime += (std::max)(deltaTime_, 0.0f);
    for (SequenceEvent& event_ : sequenceEvents)
    {
        if (event_.fired || event_.presetIndex < 0 || event_.presetIndex >= static_cast<int>(presets.size()))
        {
            continue;
        }
        if (sequenceTime < event_.startTime)
        {
            continue;
        }

        FireSequenceEvent(event_);
        event_.fired = true;
    }

    if (sequenceTime >= sequenceLength)
    {
        sequencePlaying = false;
    }
}

void AudioWorkspaceWindow::Draw()
{
    if (!BeginWorkspaceWindow(ImGuiWindowFlags_MenuBar))
    {
        EndEditorWindow();
        return;
    }

    DrawToolbar();

    const ImVec2 available_ = ImGui::GetContentRegionAvail();
    const float leftWidth_ = (std::max)(250.0f, available_.x * 0.26f);
    const float browserHeight_ = (std::max)(150.0f, available_.y * 0.28f);

    // 左をプリセット一覧、右上を編集タブ、右下を音声ファイルブラウザに固定して作業導線を分ける。
    ImGui::BeginChild("AudioPresetList", ImVec2(leftWidth_, 0.0f), true);
    DrawPresetList();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("AudioInspector", ImVec2(0.0f, 0.0f), true);
    ImGui::BeginChild("AudioSettings", ImVec2(0.0f, available_.y - browserHeight_ - ImGui::GetStyle().ItemSpacing.y), false);
    if (ImGui::BeginTabBar("AudioWorkspaceTabs"))
    {
        if (ImGui::BeginTabItem("Preset"))
        {
            DrawInspector();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Sequencer"))
        {
            DrawSequencer();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    ImGui::BeginChild("AudioFileBrowser", ImVec2(0.0f, 0.0f), true);
    DrawAudioFileBrowser();
    ImGui::EndChild();
    ImGui::EndChild();

    EndEditorWindow();
}

void AudioWorkspaceWindow::DrawToolbar()
{
    // 保存/読込と試聴系の操作を上部に集約し、プリセット調整中にすぐ確認できるようにする。
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::MenuItem("Save"))
        {
            SaveWorkspace();
        }
        if (ImGui::MenuItem("Load"))
        {
            LoadWorkspace();
        }
        if (ImGui::MenuItem("Refresh Files"))
        {
            RefreshAudioFiles();
        }
        ImGui::EndMenuBar();
    }

    if (ImGui::Button("Play", ImVec2(80.0f, 0.0f)))
    {
        PlaySelected();
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop", ImVec2(80.0f, 0.0f)))
    {
        StopSelected();
    }
    ImGui::SameLine();
    if (ImGui::Button("Overlap", ImVec2(90.0f, 0.0f)))
    {
        if (const AudioPreset* preset_ = GetSelectedPreset())
        {
            PlayPreset(*preset_, true);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(sequencePlaying ? "Stop Seq" : "Play Seq", ImVec2(100.0f, 0.0f)))
    {
        sequencePlaying ? StopSequence() : PlaySequence();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save JSON", ImVec2(100.0f, 0.0f)))
    {
        SaveWorkspace();
    }
    ImGui::Separator();
}

void AudioWorkspaceWindow::DrawPresetList()
{
    // プリセットはAudioManagerへ登録する1音声設定の単位で、選択すると右側のInspectorへ反映される。
    if (ImGui::Button("Add Preset", ImVec2(-1.0f, 0.0f)))
    {
        AudioPreset preset_;
        preset_.name = "Audio " + std::to_string(static_cast<int>(presets.size()) + 1);
        preset_.actionName = "Audio_" + std::to_string(static_cast<int>(presets.size()) + 1);
        presets.push_back(preset_);
        selectedPresetIndex = static_cast<int>(presets.size()) - 1;
    }

    if (ImGui::Button("Duplicate", ImVec2(-1.0f, 0.0f)))
    {
        if (const AudioPreset* selected_ = GetSelectedPreset())
        {
            AudioPreset copied_ = *selected_;
            copied_.name += " Copy";
            presets.push_back(copied_);
            selectedPresetIndex = static_cast<int>(presets.size()) - 1;
        }
    }

    if (ImGui::Button("Remove", ImVec2(-1.0f, 0.0f)))
    {
        if (!presets.empty() && selectedPresetIndex >= 0 && selectedPresetIndex < static_cast<int>(presets.size()))
        {
            presets.erase(presets.begin() + selectedPresetIndex);
            selectedPresetIndex = (std::min)(selectedPresetIndex, static_cast<int>(presets.size()) - 1);
        }
    }

    ImGui::Separator();

    for (int presetIndex_ = 0; presetIndex_ < static_cast<int>(presets.size()); ++presetIndex_)
    {
        const bool selected_ = selectedPresetIndex == presetIndex_;
        if (ImGui::Selectable(presets[presetIndex_].name.c_str(), selected_, 0, ImVec2(0.0f, 34.0f)))
        {
            selectedPresetIndex = presetIndex_;
            ApplySelected();
        }
    }
}

void AudioWorkspaceWindow::DrawInspector()
{
    AudioPreset* preset_ = GetSelectedPreset();
    if (preset_ == nullptr)
    {
        ImGui::TextDisabled("No audio preset selected.");
        return;
    }

    char nameBuffer_[128];
    strncpy_s(nameBuffer_, preset_->name.c_str(), sizeof(nameBuffer_));
    if (ImGui::InputText("Name", nameBuffer_, sizeof(nameBuffer_)))
    {
        preset_->name = nameBuffer_;
    }

    char actionNameBuffer_[128];
    strncpy_s(actionNameBuffer_, preset_->actionName.c_str(), sizeof(actionNameBuffer_));
    if (ImGui::InputText("Action Name", actionNameBuffer_, sizeof(actionNameBuffer_)))
    {
        preset_->actionName = actionNameBuffer_;
    }

    char pathBuffer_[256];
    strncpy_s(pathBuffer_, preset_->path.c_str(), sizeof(pathBuffer_));
    if (ImGui::InputText("Path", pathBuffer_, sizeof(pathBuffer_)))
    {
        // パスを変えた時点でAudioManagerへ読み込ませ、直後の試聴やパラメータ反映に備える。
        preset_->path = pathBuffer_;
        AudioManager::Instance().AddAudioFromFile(preset_->path, preset_->isBGM);
        ApplySelected();
    }

    bool changed_ = false;
    // 2D/BGM設定と、3D減衰・遮蔽・距離フィルタを同じプリセット上で編集する。
    changed_ |= ImGui::Checkbox("BGM", &preset_->isBGM);
    changed_ |= ImGui::Checkbox("Loop", &preset_->loop);
    changed_ |= ImGui::SliderFloat("Volume", &preset_->volume, 0.0f, 1.0f, "%.2f");
    changed_ |= ImGui::SliderFloat("Pitch", &preset_->pitch, 0.25f, 2.0f, "%.2f");
    changed_ |= ImGui::SliderFloat("Pan", &preset_->pan, -1.0f, 1.0f, "%.2f");
    changed_ |= ImGui::Checkbox("Muffled", &preset_->muffled);
    changed_ |= ImGui::SliderFloat("Low Pass Cutoff", &preset_->lowPassCutoff, 100.0f, 22000.0f, "%.0f Hz");

    ImGui::SeparatorText("3D Attenuation");
    changed_ |= ImGui::Checkbox("Use 3D", &preset_->use3D);
    changed_ |= ImGui::DragFloat3("Position", &preset_->position.x, 0.1f);
    changed_ |= ImGui::DragFloat("Min Distance", &preset_->minDistance, 0.05f, 0.01f, 1000.0f);
    changed_ |= ImGui::DragFloat("Max Distance", &preset_->maxDistance, 0.5f, 0.01f, 5000.0f);
    changed_ |= ImGui::DragFloat("Doppler Level", &preset_->dopplerLevel, 0.05f, 0.0f, 5.0f);
    changed_ |= ImGui::SliderFloat("Spread", &preset_->spread, 0.0f, 360.0f, "%.0f deg");
    changed_ |= ImGui::SliderFloat("Direct Occlusion", &preset_->directOcclusion, 0.0f, 1.0f, "%.2f");
    changed_ |= ImGui::SliderFloat("Reverb Occlusion", &preset_->reverbOcclusion, 0.0f, 1.0f, "%.2f");
    changed_ |= ImGui::Checkbox("Distance Filter", &preset_->useDistanceFilter);
    changed_ |= ImGui::SliderFloat("Filter Level", &preset_->distanceFilterLevel, 0.0f, 1.0f, "%.2f");
    changed_ |= ImGui::SliderFloat("Filter Center", &preset_->distanceFilterCenterFrequency, 10.0f, 22000.0f, "%.0f Hz");

    if (changed_)
    {
        // UI入力値をAudioManagerが受け取れる範囲に丸め、破綻した音量や距離設定を保存しないようにする。
        preset_->volume = std::clamp(preset_->volume, 0.0f, 1.0f);
        preset_->pitch = std::clamp(preset_->pitch, 0.01f, 4.0f);
        preset_->pan = std::clamp(preset_->pan, -1.0f, 1.0f);
        preset_->lowPassCutoff = std::clamp(preset_->lowPassCutoff, 10.0f, 22000.0f);
        preset_->minDistance = (std::max)(preset_->minDistance, 0.01f);
        preset_->maxDistance = (std::max)(preset_->maxDistance, preset_->minDistance + 0.01f);
        preset_->dopplerLevel = (std::max)(preset_->dopplerLevel, 0.0f);
        preset_->spread = std::clamp(preset_->spread, 0.0f, 360.0f);
        preset_->directOcclusion = std::clamp(preset_->directOcclusion, 0.0f, 1.0f);
        preset_->reverbOcclusion = std::clamp(preset_->reverbOcclusion, 0.0f, 1.0f);
        preset_->distanceFilterLevel = std::clamp(preset_->distanceFilterLevel, 0.0f, 1.0f);
        preset_->distanceFilterCenterFrequency = std::clamp(preset_->distanceFilterCenterFrequency, 10.0f, 22000.0f);
        ApplySelected();
    }

    ImGui::Separator();
    if (ImGui::Button("Play Current", ImVec2(140.0f, 0.0f)))
    {
        PlaySelected();
    }
    ImGui::SameLine();
    if (ImGui::Button("Overlap Current", ImVec2(150.0f, 0.0f)))
    {
        PlayPreset(*preset_, true);
    }
    ImGui::SameLine();
    if (ImGui::Button("Register Action", ImVec2(140.0f, 0.0f)))
    {
        SaveWorkspace();
        AudioManager::Instance().RegisterAudioWorkspaceActions(workspacePath);
    }
}

void AudioWorkspaceWindow::DrawSequencer()
{
    ImGui::SeparatorText("Audio Event Sequencer");

    // シーケンサーは秒で保持しつつ、ImSequencerにはフレームへ変換して渡す。
    sequenceLength = (std::max)(0.1f, sequenceLength);
    sequenceTime = std::clamp(sequenceTime, 0.0f, sequenceLength);

    if (ImGui::Button(sequencePlaying ? "Pause" : "Play", ImVec2(80.0f, 0.0f)))
    {
        sequencePlaying = !sequencePlaying;
        if (sequencePlaying)
        {
            // 途中位置から再生するときは、現在時刻より前のイベントを発火済みにして二重再生を避ける。
            for (SequenceEvent& event_ : sequenceEvents)
            {
                event_.fired = event_.startTime < sequenceTime;
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop", ImVec2(80.0f, 0.0f)))
    {
        StopSequence();
        sequenceTime = 0.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Event", ImVec2(100.0f, 0.0f)))
    {
        // 新規イベントは現在の再生ヘッド位置に置き、選択中プリセットを初期参照にする。
        SequenceEvent event_;
        event_.name = "Event " + std::to_string(static_cast<int>(sequenceEvents.size()) + 1);
        event_.presetIndex = presets.empty() ? -1 : selectedPresetIndex;
        event_.startTime = sequenceTime;
        sequenceEvents.push_back(event_);
        selectedSequenceEventIndex = static_cast<int>(sequenceEvents.size()) - 1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Sort by Start Time"))
    {
        std::stable_sort(
            sequenceEvents.begin(),
            sequenceEvents.end(),
            [](const SequenceEvent& left_, const SequenceEvent& right_)
            {
                return left_.startTime < right_.startTime;
            });
        selectedSequenceEventIndex = -1;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::DragFloat("Length", &sequenceLength, 0.1f, 0.1f, 120.0f, "%.2f sec");
    ImGui::SameLine();
    ImGui::Text("Time: %.2f / %.2f sec", sequenceTime, sequenceLength);

    AudioSequenceAdapter adapter_;
    // 毎フレームアダプタを作り、現在のイベント配列をImSequencerの編集インターフェースへ接続する。
    adapter_.events = &sequenceEvents;
    adapter_.presets = &presets;
    adapter_.sequenceLength = sequenceLength;
    adapter_.fps = 60.0f;
    adapter_.SyncFrames();

    int currentFrame_ = adapter_.ToFrame(sequenceTime);
    const int previousFrame_ = currentFrame_;

    ImGui::PushItemWidth(-1);
    const bool showSequencer_ = ImGui::BeginChild("AudioSequencerRegion", ImVec2(0.0f, 260.0f), true);
    if (showSequencer_)
    {
        if (ImSequencer::Sequencer(
            &adapter_,
            &currentFrame_,
            &audioSequencerExpanded,
            &selectedSequenceEventIndex,
            &firstSequenceFrame,
            ImSequencer::SEQUENCER_EDIT_ALL |
            ImSequencer::SEQUENCER_ADD |
            ImSequencer::SEQUENCER_DEL |
            ImSequencer::SEQUENCER_CHANGE_FRAME))
        {
            adapter_.EndEdit();
            if (selectedSequenceEventIndex >= static_cast<int>(sequenceEvents.size()))
            {
                selectedSequenceEventIndex = static_cast<int>(sequenceEvents.size()) - 1;
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopItemWidth();

    if (currentFrame_ != previousFrame_)
    {
        // ユーザーがタイムラインをスクラブした場合は一時停止し、発火状態をリセットして再確認しやすくする。
        const int clampedFrame_ = (std::max)(adapter_.GetFrameMin(), (std::min)(adapter_.GetFrameMax(), currentFrame_));
        sequenceTime = adapter_.ToTime(clampedFrame_);
        sequencePlaying = false;
        for (SequenceEvent& event_ : sequenceEvents)
        {
            event_.fired = false;
        }
    }

    if (selectedSequenceEventIndex < 0 || selectedSequenceEventIndex >= static_cast<int>(sequenceEvents.size()))
    {
        ImGui::TextDisabled("Click an audio event bar or name to edit details.");
        return;
    }

    SequenceEvent& event_ = sequenceEvents[selectedSequenceEventIndex];
    ImGui::SeparatorText("Selected Audio Event");

    // 選択イベントではプリセット音に対する音量・ピッチ・パン・位置の上書きだけを編集する。
    char eventNameBuffer_[128];
    strncpy_s(eventNameBuffer_, event_.name.c_str(), sizeof(eventNameBuffer_));
    if (ImGui::InputText("Event Name", eventNameBuffer_, sizeof(eventNameBuffer_)))
    {
        event_.name = eventNameBuffer_;
    }

    ImGui::SetNextItemWidth(160.0f);
    ImGui::DragFloat("Start Time", &event_.startTime, 0.01f, 0.0f, sequenceLength, "%.2f sec");
    event_.startTime = std::clamp(event_.startTime, 0.0f, sequenceLength);

    const char* presetPreview_ = "None";
    if (!presets.empty() && event_.presetIndex >= 0 && event_.presetIndex < static_cast<int>(presets.size()))
    {
        presetPreview_ = presets[event_.presetIndex].name.c_str();
    }
    ImGui::SetNextItemWidth(260.0f);
    if (ImGui::BeginCombo("Preset", presetPreview_))
    {
        for (int presetIndex_ = 0; presetIndex_ < static_cast<int>(presets.size()); ++presetIndex_)
        {
            const bool selected_ = event_.presetIndex == presetIndex_;
            if (ImGui::Selectable(presets[presetIndex_].name.c_str(), selected_))
            {
                event_.presetIndex = presetIndex_;
            }
            if (selected_)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat("Volume Scale", &event_.volumeScale, 0.0f, 2.0f, "%.2f");
    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat("Pitch Scale", &event_.pitchScale, 0.25f, 2.0f, "%.2f");
    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat("Pan Offset", &event_.panOffset, -1.0f, 1.0f, "%.2f");
    ImGui::Checkbox("Use Preset Position", &event_.usePresetPosition);
    if (!event_.usePresetPosition)
    {
        ImGui::DragFloat3("Event Position", &event_.position.x, 0.1f);
    }

    if (ImGui::Button("Fire Event Now", ImVec2(140.0f, 0.0f)))
    {
        FireSequenceEvent(event_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplicate Event", ImVec2(140.0f, 0.0f)))
    {
        SequenceEvent copied_ = event_;
        copied_.name += " Copy";
        copied_.startTime = (std::min)(sequenceLength, copied_.startTime + 0.2f);
        copied_.fired = false;
        sequenceEvents.insert(sequenceEvents.begin() + selectedSequenceEventIndex + 1, copied_);
        selectedSequenceEventIndex += 1;
    }
}


void AudioWorkspaceWindow::DrawAudioFileBrowser()
{
    // Data/Audio配下のファイルを選ぶと、現在プリセットのパスへ直接割り当てて読み込む。
    if (ImGui::Button("Refresh Audio Files"))
    {
        RefreshAudioFiles();
    }
    ImGui::Separator();

    AudioPreset* preset_ = GetSelectedPreset();
    for (const std::string& path_ : audioFiles)
    {
        const bool selected_ = preset_ && preset_->path == path_;
        if (ImGui::Selectable(path_.c_str(), selected_))
        {
            if (preset_)
            {
                preset_->path = path_;
                AudioManager::Instance().AddAudioFromFile(preset_->path, preset_->isBGM);
                ApplySelected();
            }
        }
    }
}

void AudioWorkspaceWindow::PlaySelected()
{
    if (AudioPreset* preset_ = GetSelectedPreset())
    {
        PlayPreset(*preset_, false);
    }
}

void AudioWorkspaceWindow::FireSequenceEvent(const SequenceEvent& event_)
{
    if (event_.presetIndex < 0 || event_.presetIndex >= static_cast<int>(presets.size()))
    {
        return;
    }

    // プリセット設定をベースに、イベント固有の倍率や位置指定を重ねてワンショット再生する。
    const AudioPreset& preset_ = presets[event_.presetIndex];
    AudioPlaybackSettings settings_ = MakeSettings(preset_);
    settings_.volume = std::clamp(settings_.volume * event_.volumeScale, 0.0f, 1.0f);
    settings_.pitch = std::clamp(settings_.pitch * event_.pitchScale, 0.01f, 4.0f);
    settings_.pan = std::clamp(settings_.pan + event_.panOffset, -1.0f, 1.0f);
    if (!event_.usePresetPosition)
    {
        settings_.position = event_.position;
    }

    AudioManager::Instance().PlayOneShotByPath(preset_.path, settings_, preset_.isBGM);
}

void AudioWorkspaceWindow::PlayPreset(const AudioPreset& preset_, bool allowOverlap_)
{
    if (preset_.path.empty())
    {
        return;
    }

    // 重ね再生や3D音は個別インスタンス扱いにし、通常の2D音は既存再生の設定を更新してから鳴らす。
    AudioManager& audioManager_ = AudioManager::Instance();
    audioManager_.AddAudioFromFile(preset_.path, preset_.isBGM);
    if (allowOverlap_ || preset_.use3D)
    {
        audioManager_.PlayOneShotByPath(preset_.path, MakeSettings(preset_), preset_.isBGM);
        return;
    }

    ApplySelected();
    if (preset_.isBGM)
    {
        audioManager_.PlayBGMByPath(preset_.path, preset_.loop || preset_.isBGM);
    }
    else
    {
        audioManager_.PlaySEByPath(preset_.path, preset_.loop);
    }
}

void AudioWorkspaceWindow::StopSelected()
{
    const AudioPreset* preset_ = GetSelectedPreset();
    if (preset_ == nullptr || preset_->path.empty())
    {
        return;
    }
    AudioManager::Instance().StopByPath(preset_->path);
}

void AudioWorkspaceWindow::ApplySelected()
{
    const AudioPreset* preset_ = GetSelectedPreset();
    if (preset_ == nullptr || preset_->path.empty())
    {
        return;
    }

    AudioManager& audioManager_ = AudioManager::Instance();
    // Inspectorの値を現在読み込まれている音源へ即時反映し、編集中の試聴結果と保存値をそろえる。
    audioManager_.AddAudioFromFile(preset_->path, preset_->isBGM);
    audioManager_.SetVolumeByPath(preset_->path, preset_->volume);
    audioManager_.SetPitchByPath(preset_->path, preset_->pitch);
    audioManager_.SetPanByPath(preset_->path, preset_->pan);
    audioManager_.SetMuffledByPath(preset_->path, preset_->muffled, preset_->lowPassCutoff);
}

void AudioWorkspaceWindow::PlaySequence()
{
    sequenceTime = 0.0f;
    sequencePlaying = true;
    for (SequenceEvent& event_ : sequenceEvents)
    {
        event_.fired = false;
    }
}

void AudioWorkspaceWindow::StopSequence()
{
    sequencePlaying = false;
    for (const AudioPreset& preset_ : presets)
    {
        AudioManager::Instance().StopByPath(preset_.path);
    }
}

void AudioWorkspaceWindow::SaveWorkspace()
{
    // プリセットとシーケンスを1つのJSONへ保存し、ゲーム側のアクション登録にも同じファイルを使う。
    std::filesystem::create_directories(std::filesystem::path(workspacePath).parent_path());

    nlohmann::json json_;
    json_["SelectedPresetIndex"] = selectedPresetIndex;
    json_["SelectedSequenceEventIndex"] = selectedSequenceEventIndex;
    json_["SequenceLength"] = sequenceLength;
    json_["Presets"] = nlohmann::json::array();
    json_["SequenceEvents"] = nlohmann::json::array();
    for (const AudioPreset& preset_ : presets)
    {
        json_["Presets"].push_back(SerializePreset(preset_));
    }
    for (const SequenceEvent& event_ : sequenceEvents)
    {
        json_["SequenceEvents"].push_back(SerializeSequenceEvent(event_));
    }

    std::ofstream file_(workspacePath);
    if (file_.is_open())
    {
        file_ << json_.dump(4);
        file_.close();
    }
    AudioManager::Instance().RegisterAudioWorkspaceActions(workspacePath);
}

void AudioWorkspaceWindow::LoadWorkspace()
{
    std::ifstream file_(workspacePath);
    if (!file_.is_open())
    {
        return;
    }

    // 保存データを読み直すときは配列を作り直し、空なら編集可能なデフォルト要素を補う。
    nlohmann::json json_;
    file_ >> json_;

    presets.clear();
    sequenceEvents.clear();
    if (json_.contains("Presets") && json_["Presets"].is_array())
    {
        for (const auto& presetJson_ : json_["Presets"])
        {
            AudioPreset preset_;
            DeserializePreset(preset_, presetJson_);
            presets.push_back(preset_);
        }
    }
    if (json_.contains("SequenceEvents") && json_["SequenceEvents"].is_array())
    {
        for (const auto& eventJson_ : json_["SequenceEvents"])
        {
            SequenceEvent event_;
            DeserializeSequenceEvent(event_, eventJson_);
            sequenceEvents.push_back(event_);
        }
    }

    if (presets.empty())
    {
        presets.push_back(AudioPreset{});
    }
    if (sequenceEvents.empty())
    {
        sequenceEvents.push_back(SequenceEvent{});
    }

    selectedPresetIndex = std::clamp(json_.value("SelectedPresetIndex", 0), 0, static_cast<int>(presets.size()) - 1);
    selectedSequenceEventIndex = std::clamp(json_.value("SelectedSequenceEventIndex", -1), -1, static_cast<int>(sequenceEvents.size()) - 1);
    sequenceLength = (std::max)(json_.value("SequenceLength", 5.0f), 0.1f);
    ApplySelected();
    AudioManager::Instance().RegisterAudioWorkspaceActions(workspacePath);
}

void AudioWorkspaceWindow::RefreshAudioFiles()
{
    // 音声ブラウザは実ファイル階層を再帰走査し、表示順が安定するよう最後にソートする。
    audioFiles.clear();
    const std::filesystem::path rootPath_("Data/Audio");
    if (!std::filesystem::exists(rootPath_))
    {
        return;
    }

    for (const auto& entry_ : std::filesystem::recursive_directory_iterator(rootPath_))
    {
        if (!entry_.is_regular_file() || !IsAudioFile(entry_.path()))
        {
            continue;
        }
        audioFiles.push_back(entry_.path().generic_string());
    }

    std::sort(audioFiles.begin(), audioFiles.end());
}

nlohmann::json AudioWorkspaceWindow::SerializePreset(const AudioPreset& preset_) const
{
    // JSONキー名は既存保存データとの互換を持つため、C++側のメンバ名へ寄せず固定している。
    nlohmann::json json_;
    json_["Name"] = preset_.name;
    json_["ActionName"] = preset_.actionName;
    json_["Path"] = preset_.path;
    json_["IsBGM"] = preset_.isBGM;
    json_["Loop"] = preset_.loop;
    json_["Volume"] = preset_.volume;
    json_["Pitch"] = preset_.pitch;
    json_["Pan"] = preset_.pan;
    json_["Muffled"] = preset_.muffled;
    json_["LowPassCutoff"] = preset_.lowPassCutoff;
    json_["Use3D"] = preset_.use3D;
    json_["Position"] = { preset_.position.x, preset_.position.y, preset_.position.z };
    json_["MinDistance"] = preset_.minDistance;
    json_["MaxDistance"] = preset_.maxDistance;
    json_["DopplerLevel"] = preset_.dopplerLevel;
    json_["Spread"] = preset_.spread;
    json_["DirectOcclusion"] = preset_.directOcclusion;
    json_["ReverbOcclusion"] = preset_.reverbOcclusion;
    json_["UseDistanceFilter"] = preset_.useDistanceFilter;
    json_["DistanceFilterLevel"] = preset_.distanceFilterLevel;
    json_["DistanceFilterCenterFrequency"] = preset_.distanceFilterCenterFrequency;
    return json_;
}

void AudioWorkspaceWindow::DeserializePreset(AudioPreset& preset_, const nlohmann::json& json_) const
{
    // 読み込み時にも範囲を丸め、古い/手編集されたJSONで不正値が入っても編集状態を壊さない。
    preset_.name = json_.value("Name", std::string("New Audio"));
    preset_.actionName = json_.value("ActionName", preset_.name);
    preset_.path = json_.value("Path", std::string("Data/Audio/SE/Sword-Fight-1.wav"));
    preset_.isBGM = json_.value("IsBGM", false);
    preset_.loop = json_.value("Loop", false);
    preset_.volume = std::clamp(json_.value("Volume", 1.0f), 0.0f, 1.0f);
    preset_.pitch = std::clamp(json_.value("Pitch", 1.0f), 0.01f, 4.0f);
    preset_.pan = std::clamp(json_.value("Pan", 0.0f), -1.0f, 1.0f);
    preset_.muffled = json_.value("Muffled", false);
    preset_.lowPassCutoff = std::clamp(json_.value("LowPassCutoff", 1200.0f), 10.0f, 22000.0f);
    preset_.use3D = json_.value("Use3D", false);
    if (json_.contains("Position") && json_["Position"].is_array() && json_["Position"].size() >= 3)
    {
        preset_.position = { json_["Position"][0], json_["Position"][1], json_["Position"][2] };
    }
    preset_.minDistance = (std::max)(json_.value("MinDistance", 1.0f), 0.01f);
    preset_.maxDistance = (std::max)(json_.value("MaxDistance", 30.0f), preset_.minDistance + 0.01f);
    preset_.dopplerLevel = (std::max)(json_.value("DopplerLevel", 1.0f), 0.0f);
    preset_.spread = std::clamp(json_.value("Spread", 0.0f), 0.0f, 360.0f);
    preset_.directOcclusion = std::clamp(json_.value("DirectOcclusion", 0.0f), 0.0f, 1.0f);
    preset_.reverbOcclusion = std::clamp(json_.value("ReverbOcclusion", 0.0f), 0.0f, 1.0f);
    preset_.useDistanceFilter = json_.value("UseDistanceFilter", false);
    preset_.distanceFilterLevel = std::clamp(json_.value("DistanceFilterLevel", 1.0f), 0.0f, 1.0f);
    preset_.distanceFilterCenterFrequency = std::clamp(json_.value("DistanceFilterCenterFrequency", 1500.0f), 10.0f, 22000.0f);
}

nlohmann::json AudioWorkspaceWindow::SerializeSequenceEvent(const SequenceEvent& event_) const
{
    nlohmann::json json_;
    json_["Name"] = event_.name;
    json_["PresetIndex"] = event_.presetIndex;
    json_["StartTime"] = event_.startTime;
    json_["VolumeScale"] = event_.volumeScale;
    json_["PitchScale"] = event_.pitchScale;
    json_["PanOffset"] = event_.panOffset;
    json_["UsePresetPosition"] = event_.usePresetPosition;
    json_["Position"] = { event_.position.x, event_.position.y, event_.position.z };
    return json_;
}

void AudioWorkspaceWindow::DeserializeSequenceEvent(SequenceEvent& event_, const nlohmann::json& json_) const
{
    event_.name = json_.value("Name", std::string("Event"));
    event_.presetIndex = json_.value("PresetIndex", 0);
    event_.startTime = (std::max)(json_.value("StartTime", 0.0f), 0.0f);
    event_.volumeScale = std::clamp(json_.value("VolumeScale", 1.0f), 0.0f, 2.0f);
    event_.pitchScale = std::clamp(json_.value("PitchScale", 1.0f), 0.01f, 4.0f);
    event_.panOffset = std::clamp(json_.value("PanOffset", 0.0f), -1.0f, 1.0f);
    event_.usePresetPosition = json_.value("UsePresetPosition", true);
    if (json_.contains("Position") && json_["Position"].is_array() && json_["Position"].size() >= 3)
    {
        event_.position = { json_["Position"][0], json_["Position"][1], json_["Position"][2] };
    }
}

AudioPlaybackSettings AudioWorkspaceWindow::MakeSettings(const AudioPreset& preset_) const
{
    // AudioPresetの編集値をAudioManagerの再生設定へ詰め替える共通経路。
    AudioPlaybackSettings settings_;
    settings_.loop = preset_.loop;
    settings_.volume = preset_.volume;
    settings_.pitch = preset_.pitch;
    settings_.pan = preset_.pan;
    settings_.muffled = preset_.muffled;
    settings_.lowPassCutoff = preset_.lowPassCutoff;
    settings_.use3D = preset_.use3D;
    settings_.position = preset_.position;
    settings_.minDistance = preset_.minDistance;
    settings_.maxDistance = preset_.maxDistance;
    settings_.dopplerLevel = preset_.dopplerLevel;
    settings_.spread = preset_.spread;
    settings_.directOcclusion = preset_.directOcclusion;
    settings_.reverbOcclusion = preset_.reverbOcclusion;
    settings_.useDistanceFilter = preset_.useDistanceFilter;
    settings_.distanceFilterLevel = preset_.distanceFilterLevel;
    settings_.distanceFilterCenterFrequency = preset_.distanceFilterCenterFrequency;
    return settings_;
}

AudioWorkspaceWindow::AudioPreset* AudioWorkspaceWindow::GetSelectedPreset()
{
    if (selectedPresetIndex < 0 || selectedPresetIndex >= static_cast<int>(presets.size()))
    {
        return nullptr;
    }
    return &presets[selectedPresetIndex];
}

const AudioWorkspaceWindow::AudioPreset* AudioWorkspaceWindow::GetSelectedPreset() const
{
    if (selectedPresetIndex < 0 || selectedPresetIndex >= static_cast<int>(presets.size()))
    {
        return nullptr;
    }
    return &presets[selectedPresetIndex];
}

