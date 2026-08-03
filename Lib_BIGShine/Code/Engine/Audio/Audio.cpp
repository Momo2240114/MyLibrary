#include "PrecompiledHeader.h"

#include "Engine/Common/misc.h"
#include "Audio.h"

#include <fmod_errors.h>

// FMOD Systemを一度だけ生成し、Engineから音声を再生できる状態にする。
void Audio::Initialize()
{
    // 多重初期化によって既存Systemを上書きしないよう、初期化済みなら処理を終了する。
    if (initialized) return;

    // FMODの中核となるSystemオブジェクトを生成する。
    FMOD_RESULT result_ = FMOD::System_Create(&system);
    _ASSERT_EXPR_A(result_ == FMOD_OK, FMOD_ErrorString(result_));

    // 最大512チャンネルを扱える通常モードでFMODを初期化する。
    result_ = system->init(512, FMOD_INIT_NORMAL, nullptr);
    _ASSERT_EXPR_A(result_ == FMOD_OK, FMOD_ErrorString(result_));

    initialized = true;
}

// FMOD Systemが保持する内部リソースを閉じ、System本体を解放する。
void Audio::Finalize()
{
    if (system != nullptr)
    {
        // 再生システムを終了してからSystemの参照を解放する。
        system->close();
        system->release();
        system = nullptr;
    }

    initialized = false;
}

// FMODのチャンネル、DSP、ストリーミングなどの内部状態を毎フレーム更新する。
void Audio::Update()
{
    if (system != nullptr)
    {
        system->update();
    }
}

// 指定ファイルを再生するAudioSourceを生成し、呼び出し元へ所有権を渡す。
AudioSource* Audio::LoadAudioSource(const char* filename_)
{
    // 明示的な初期化前に呼ばれた場合でも、ここで利用可能な状態へ遷移させる。
    if (!initialized) Initialize();
    if (system == nullptr) return nullptr;
    return new AudioSource(system, filename_);
}
