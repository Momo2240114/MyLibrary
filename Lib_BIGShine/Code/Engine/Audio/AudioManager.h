#pragma once

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <DirectXMath.h>

#include "Audio.h"

class CameraComponent;

struct AudioWorkspacePreset
{
    std::string name;
    std::string actionName;
    std::string path;
    bool isBGM = false;
    AudioPlaybackSettings settings;
};

class AudioManager
{
public:
    static AudioManager& Instance()
    {
        static AudioManager ins;
        return ins;
    }

    void LoadFile();
    void Update();
    void Finalize();
    void UpdateListenerFromCamera(const CameraComponent* camera_, float deltaTime_);

    void AddBGM(std::unique_ptr<AudioSource> source_);
    void AddSE(std::unique_ptr<AudioSource> source_);

    void PlayBGM(size_t index_) const;
    void SetVolumeBGM(size_t index_, float volume_) const;
    void SetVolumeSE(size_t index_, float volume_) const;

    void PlaySE(size_t index_) const;
    void StopBGM();
    void StopSE();
    void StopSE(int index_);
    void RemoveBGM(size_t index_);

    void PlayBGMByPath(const std::string& path_, bool loop_ = true);
    void PlaySEByPath(const std::string& path_, bool loop_ = false);
    void PlayOneShotByPath(const std::string& path_, const AudioPlaybackSettings& settings_, bool isBGM_ = false);
    void StopByPath(const std::string& path_);
    void SetVolumeByPath(const std::string& path_, float volume_);
    void SetPitchByPath(const std::string& path_, float pitch_);
    void SetPanByPath(const std::string& path_, float pan_);
    void SetMuffledByPath(const std::string& path_, bool muffled_, float cutoff_ = 1200.0f);
    bool LoadAudioWorkspace(const std::string& path_ = "Data/Audio/AudioWorkspace.json");
    void RegisterAudioWorkspaceActions(const std::string& path_ = "Data/Audio/AudioWorkspace.json");
    bool PlayWorkspacePreset(const std::string& presetName_, bool allowOverlap_ = true);
    bool StopWorkspacePreset(const std::string& presetName_);
    std::vector<std::string> GetWorkspacePresetNames() const;

    void ClearAll();

    size_t AddAudioFromFile(const std::string& path_, bool isBGM_);
    void DrawGUI();

private:
    ~AudioManager() = default;
    AudioManager() = default;
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    std::vector<std::unique_ptr<AudioSource>> audioBGMs;
    std::vector<std::unique_ptr<AudioSource>> audioSEs;
    std::set<std::string> loadedPaths;
    std::unordered_map<std::string, size_t> bgmPathToIndex;
    std::unordered_map<std::string, size_t> sePathToIndex;
    std::vector<AudioWorkspacePreset> workspacePresets;
    std::vector<std::string> registeredWorkspaceActionNames;
    std::unordered_map<std::string, size_t> workspacePresetToIndex;
    DirectX::XMFLOAT3 previousListenerPosition = { 0.0f, 0.0f, 0.0f };
    bool hasPreviousListenerPosition = false;
};

enum BGM_NUMBER
{
    BGM_TITLE,
    BGM_TUTORIAL,
    BGM_GAME,
};

enum SE_NUMBER
{
    SE_TITLESELECT,
    SE_TTILEMOVE,
    SE_HEARTBEAT,
    SE_GAMEOVER,
    SE_GAMECLEAR,
    SE_DISCOVERY,
};


