#pragma once

#include "EditorWorkspaceWindow.h"
#include "Engine/Audio/AudioSource.h"

#include <string>
#include <vector>

#include <DirectXMath.h>
#include <nlohmann/json.hpp>

class AudioWorkspaceWindow : public EditorWorkspaceWindow
{
public:
    explicit AudioWorkspaceWindow(GameObjectManager* manager_);

    void Draw() override;
    void Update(float deltaTime_) override;

    bool ShouldPauseScene() const override{ return true; }
private:
    struct AudioPreset
    {
        // AudioManagerへ登録・試聴する1音声分の再生設定。
        std::string name = "New Audio";
        std::string actionName = "Audio_NewAudio";
        std::string path = "Data/Audio/SE/TitleSelect.wav";
        bool isBGM = false;
        bool loop = false;
        float volume = 1.0f;
        float pitch = 1.0f;
        float pan = 0.0f;
        bool muffled = false;
        float lowPassCutoff = 1200.0f;
        bool use3D = false;
        // 3D再生時だけ使う位置・距離減衰・遮蔽・距離フィルタ設定。
        DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
        float minDistance = 1.0f;
        float maxDistance = 30.0f;
        float dopplerLevel = 1.0f;
        float spread = 0.0f;
        float directOcclusion = 0.0f;
        float reverbOcclusion = 0.0f;
        bool useDistanceFilter = false;
        float distanceFilterLevel = 1.0f;
        float distanceFilterCenterFrequency = 1500.0f;
    };

    struct SequenceEvent
    {
        // シーケンサー上で指定時刻に鳴らすイベント。プリセットに対して倍率や位置を上書きできる。
        std::string name = "Event";
        int presetIndex = 0;
        float startTime = 0.0f;
        float volumeScale = 1.0f;
        float pitchScale = 1.0f;
        float panOffset = 0.0f;
        bool usePresetPosition = true;
        DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
        // 再生中の二重発火を防ぐランタイム用フラグ。保存対象ではない。
        bool fired = false;
    };

    void DrawToolbar();
    void DrawPresetList();
    void DrawInspector();
    void DrawSequencer();
    void DrawAudioFileBrowser();
    void PlaySelected();
    void FireSequenceEvent(const SequenceEvent& event_);
    void PlayPreset(const AudioPreset& preset_, bool allowOverlap_);
    void StopSelected();
    void ApplySelected();
    void PlaySequence();
    void StopSequence();
    void SaveWorkspace();
    void LoadWorkspace();
    void RefreshAudioFiles();

    nlohmann::json SerializePreset(const AudioPreset& preset_) const;
    void DeserializePreset(AudioPreset& preset_, const nlohmann::json& json_) const;
    nlohmann::json SerializeSequenceEvent(const SequenceEvent& event_) const;
    void DeserializeSequenceEvent(SequenceEvent& event_, const nlohmann::json& json_) const;
    AudioPlaybackSettings MakeSettings(const AudioPreset& preset_) const;
    AudioPreset* GetSelectedPreset();
    const AudioPreset* GetSelectedPreset() const;

    struct AudioSequenceAdapter;

private:
    std::vector<AudioPreset> presets;
    std::vector<SequenceEvent> sequenceEvents;
    // Data/Audio配下から見つけた音声ファイル一覧。ブラウザ表示とプリセット割り当てに使う。
    std::vector<std::string> audioFiles;
    int selectedPresetIndex = 0;
    int selectedSequenceEventIndex = -1;
    float sequenceTime = 0.0f;
    float sequenceLength = 5.0f;
    bool sequencePlaying = false;
    // ImSequencerの表示位置と展開状態を保持し、スクロール/選択を毎フレーム失わないようにする。
    int firstSequenceFrame = 0;
    bool audioSequencerExpanded = true;
    std::string workspacePath = "Data/Audio/AudioWorkspace.json";
};
