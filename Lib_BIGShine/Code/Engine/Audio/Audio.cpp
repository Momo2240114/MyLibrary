#include "Engine/Common/misc.h"
#include "Audio.h"

#include <fmod_errors.h>

void Audio::Initialize()
{
    if (initialized) return;

    FMOD_RESULT result_ = FMOD::System_Create(&system);
    _ASSERT_EXPR_A(result_ == FMOD_OK, FMOD_ErrorString(result_));

    result_ = system->init(512, FMOD_INIT_NORMAL, nullptr);
    _ASSERT_EXPR_A(result_ == FMOD_OK, FMOD_ErrorString(result_));

    initialized = true;
}

void Audio::Finalize()
{
    if (system != nullptr)
    {
        system->close();
        system->release();
        system = nullptr;
    }

    initialized = false;
}

void Audio::Update()
{
    if (system != nullptr)
    {
        system->update();
    }
}

AudioSource* Audio::LoadAudioSource(const char* filename_)
{
    if (!initialized) Initialize();
    if (system == nullptr) return nullptr;
    return new AudioSource(system, filename_);
}
