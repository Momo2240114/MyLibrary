#pragma once

#include "AudioSource.h"

namespace FMOD
{
    class System;
}

class Audio
{
private:
    Audio() = default;
    ~Audio() = default;

public:
    static Audio& Instance()
    {
        static Audio instance;
        return instance;
    }

    void Initialize();
    void Finalize();
    void Update();

    AudioSource* LoadAudioSource(const char* filename_);
    FMOD::System* GetSystem() const { return system; }

private:
    FMOD::System* system = nullptr;
    bool initialized = false;
};
