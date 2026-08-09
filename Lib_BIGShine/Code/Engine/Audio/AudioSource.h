#pragma once

#include <string>
#include <vector>

#include <DirectXMath.h>
#include <fmod.hpp>

// 1回の音声再生に適用する再生パラメータ。
// 2D再生だけでなく、位置・速度・遮蔽・距離フィルターなどFMODの3D音響設定もまとめて保持する。
struct AudioPlaybackSettings
{
    // 再生終了後に先頭へ戻って繰り返すか。
    bool loop = false;
    // 再生音量。0.0で無音、1.0で基準音量。
    float volume = 1.0f;
    // 再生速度を伴う音程倍率。1.0が元の音程。
    float pitch = 1.0f;
    // 2D音声の左右定位。-1.0が左、1.0が右。
    float pan = 0.0f;
    // ローパスDSPを適用し、音がこもった表現にするか。
    bool muffled = false;
    // ローパスフィルターが減衰を開始するカットオフ周波数。
    float lowPassCutoff = 22000.0f;
    // 位置とListenerの関係から減衰・定位を計算する3D音声として再生するか。
    bool use3D = false;
    // 3D音源のワールド座標。
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    // ドップラー効果の計算に使用する3D音源の移動速度。
    DirectX::XMFLOAT3 velocity = { 0.0f, 0.0f, 0.0f };
    // 音量減衰が始まる音源からの最小距離。
    float minDistance = 1.0f;
    // 3D音声の減衰計算に使用する最大距離。
    float maxDistance = 100.0f;
    // 相対速度によるドップラー効果の強さ。
    float dopplerLevel = 1.0f;
    // 3D音源がスピーカーへ広がる角度。
    float spread = 0.0f;
    // 障害物による直接音の遮蔽率。
    float directOcclusion = 0.0f;
    // 障害物によるリバーブ成分の遮蔽率。
    float reverbOcclusion = 0.0f;
    // 距離に応じたFMODの周波数フィルターを使用するか。
    bool useDistanceFilter = false;
    // 距離フィルターの適用量。
    float distanceFilterLevel = 1.0f;
    // 距離フィルターの中心周波数。
    float distanceFilterCenterFrequency = 1500.0f;
};

// 1つの音声ファイルと、その再生チャンネル・DSPを管理するクラス。
// 通常再生用の主チャンネルに加え、同じSoundを重ねて鳴らすOneShotチャンネルも保持する。
class AudioSource
{
public:
    AudioSource(FMOD::System* system_, const char* filename_);
    ~AudioSource();

    // 主チャンネルを使って2D音声を再生する。既存の主再生は停止してから開始する。
    void Play(bool loop_);
    // 独立したチャンネルを追加し、既存音を止めずに重ねて再生する。
    void PlayOneShot(const AudioPlaybackSettings& settings_);
    // 主チャンネルと全OneShotチャンネルを停止し、関連DSPを解放する。
    void Stop();
    // 再生が終わったOneShot ChannelとDSPを解放する。
    void Update();
    void SetVolume(float volume_);
    void SetPitch(float pitch_);
    void SetPan(float pan_);
    void SetMuffled(bool muffled_);
    void SetLowPassCutoff(float cutoff_);
    bool IsValid() const { return sound != nullptr; }

private:
    // OneShot再生へ接続したDSPを、対応するChannelの終了に合わせて解放する。
    struct OneShotDsp
    {
        FMOD::Channel* channel = nullptr;
        FMOD::DSP* dsp = nullptr;
    };

    // OneShotチャンネルへローパスDSPを追加し、こもり効果を適用する。
    void ApplyMuffle(FMOD::Channel* channel_, bool muffled_, float cutoff_);
    // 主チャンネルのローパスDSPを現在設定に合わせて追加・更新・解除する。
    void ApplyPrimaryMuffle();
    // 音量やPitchに加え、3D属性とDSPを指定チャンネルへまとめて反映する。
    void ApplySettings(FMOD::Channel* channel_, const AudioPlaybackSettings& settings_);
    // 再生終了済みのOneShotチャンネルを管理配列から取り除く。
    void CleanupStoppedChannels();

    // Sound生成、Channel再生、DSP生成に使用するFMOD System。所有権はAudio側が持つ。
    FMOD::System* system = nullptr;
    // 読み込んだ音声データを表すFMOD Sound。
    FMOD::Sound* sound = nullptr;
    // Playで使用する通常再生用の主チャンネル。
    FMOD::Channel* channel = nullptr;
    // 主チャンネルへ接続しているこもり表現用ローパスDSP。
    FMOD::DSP* primaryLowPassDsp = nullptr;
    // 重複再生中のOneShotチャンネル一覧。
    std::vector<FMOD::Channel*> oneShotChannels;
    // OneShotごとに生成したローパスDSPの解放管理用一覧。
    std::vector<OneShotDsp> oneShotDsps;
    // このAudioSourceが参照する音声ファイルパス。
    std::string filename;
    float volume = 1.0f;
    float pitch = 1.0f;
    float pan = 0.0f;
    float lowPassCutoff = 22000.0f;
    bool muffled = false;
};
