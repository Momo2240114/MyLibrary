#pragma once

#include "AudioSource.h"

namespace FMOD
{
    class System;
}

// FMODのSystem生成・更新・解放を一元管理するオーディオ基盤クラス。
// Engine全体で1つのFMOD::Systemを共有し、AudioSourceの生成元として機能する。
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

    // FMOD Systemを生成し、同時再生数などの初期設定を行う。
    void Initialize();
    // FMOD Systemを閉じて解放し、初期化前の状態へ戻す。
    void Finalize();
    // FMOD内部の再生状態やDSP処理を進めるため、毎フレーム更新する。
    void Update();

    // 指定した音声ファイルをFMOD Soundとして読み込み、再生用AudioSourceを生成する。
    AudioSource* LoadAudioSource(const char* filename_);
    // 3D Listener設定など、FMOD APIを直接利用する処理へSystemを公開する。
    FMOD::System* GetSystem() const { return system; }

private:
    // FMOD全体の初期化、Sound生成、再生更新を担当するSystem本体。
    FMOD::System* system = nullptr;
    // FMOD Systemが利用可能な状態まで初期化済みかを示す。
    bool initialized = false;
};
