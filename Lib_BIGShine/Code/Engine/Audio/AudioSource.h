#pragma once

#include <string>
#include <vector>

#include <DirectXMath.h>
#include <fmod.hpp>

struct AudioPlaybackSettings
{
    bool loop = false;
    float volume = 1.0f;
    float pitch = 1.0f;
    float pan = 0.0f;
    bool muffled = false;
    float lowPassCutoff = 22000.0f;
    bool use3D = false;
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 velocity = { 0.0f, 0.0f, 0.0f };
    float minDistance = 1.0f;
    float maxDistance = 100.0f;
    float dopplerLevel = 1.0f;
    float spread = 0.0f;
    float directOcclusion = 0.0f;
    float reverbOcclusion = 0.0f;
    bool useDistanceFilter = false;
    float distanceFilterLevel = 1.0f;
    float distanceFilterCenterFrequency = 1500.0f;
};

class AudioSource
{
public:
    AudioSource(FMOD::System* system_, const char* filename_);
    ~AudioSource();

    void Play(bool loop_);
    void PlayOneShot(const AudioPlaybackSettings& settings_);
    void Stop();
    void SetVolume(float volume_);
    void SetPitch(float pitch_);
    void SetPan(float pan_);
    void SetMuffled(bool muffled_);
    void SetLowPassCutoff(float cutoff_);
    bool IsValid() const { return sound != nullptr; }

private:
    void ApplyMuffle(FMOD::Channel* channel_, bool muffled_, float cutoff_);
    void ApplyPrimaryMuffle();
    void ApplySettings(FMOD::Channel* channel_, const AudioPlaybackSettings& settings_);
    void CleanupStoppedChannels();

    FMOD::System* system = nullptr;
    FMOD::Sound* sound = nullptr;
    FMOD::Channel* channel = nullptr;
    FMOD::DSP* primaryLowPassDsp = nullptr;
    std::vector<FMOD::Channel*> oneShotChannels;
    std::vector<FMOD::DSP*> oneShotDsps;
    std::string filename;
    float volume = 1.0f;
    float pitch = 1.0f;
    float pan = 0.0f;
    float lowPassCutoff = 22000.0f;
    bool muffled = false;
};
