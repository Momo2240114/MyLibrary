#include "PrecompiledHeader.h"

#include "Engine/Common/misc.h"
#include "AudioSource.h"

#include <algorithm>
#include <fmod_errors.h>

// 音声ファイルからFMOD Soundを生成し、このSourceで再生できる状態にする。
AudioSource::AudioSource(FMOD::System* system_, const char* filename_)
    : system(system_), filename(filename_ ? filename_ : "")
{
    if (system == nullptr || filename.empty()) return;

    FMOD_RESULT result_ = system->createSound(filename.c_str(), FMOD_DEFAULT, nullptr, &sound);
    _ASSERT_EXPR_A(result_ == FMOD_OK, FMOD_ErrorString(result_));
}

// 再生中チャンネルを停止した後、保持しているFMOD Soundを解放する。
AudioSource::~AudioSource()
{
    Stop();
    if (sound != nullptr)
    {
        sound->release();
        sound = nullptr;
    }
}

// 主チャンネルを作り直し、保持中の基本設定を適用して2D再生を開始する。
void AudioSource::Play(bool loop_)
{
    Stop();
    if (system == nullptr || sound == nullptr) return;

    AudioPlaybackSettings settings_;
    settings_.loop = loop_;
    settings_.volume = volume;
    settings_.pitch = pitch;
    settings_.pan = pan;
    settings_.muffled = muffled;
    settings_.lowPassCutoff = lowPassCutoff;

    sound->setMode((loop_ ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF) | FMOD_2D);

    FMOD_RESULT result_ = system->playSound(sound, nullptr, true, &channel);
    _ASSERT_EXPR_A(result_ == FMOD_OK, FMOD_ErrorString(result_));
    ApplySettings(channel, settings_);
    channel->setPaused(false);
}

// 新しいチャンネルを生成し、指定された2D／3D設定で既存音へ重ねて再生する。
void AudioSource::PlayOneShot(const AudioPlaybackSettings& settings_)
{
    CleanupStoppedChannels();
    if (system == nullptr || sound == nullptr) return;

    // Loop設定と2D／3D設定を組み合わせ、Soundの再生モードを決定する。
    FMOD_MODE mode_ = settings_.loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF;
    mode_ |= settings_.use3D ? FMOD_3D : FMOD_2D;
    sound->setMode(mode_);

    // 主チャンネルとは別のChannelを取得し、同じSoundの重複再生を成立させる。
    FMOD::Channel* newChannel_ = nullptr;
    FMOD_RESULT result_ = system->playSound(sound, nullptr, true, &newChannel_);
    _ASSERT_EXPR_A(result_ == FMOD_OK, FMOD_ErrorString(result_));

    ApplySettings(newChannel_, settings_);
    newChannel_->setPaused(false);
    oneShotChannels.push_back(newChannel_);
}

// このSourceが開始した全チャンネルを停止し、生成したDSPを解放する。
void AudioSource::Stop()
{
    if (channel != nullptr)
    {
        channel->stop();
        channel = nullptr;
    }

    if (primaryLowPassDsp != nullptr)
    {
        primaryLowPassDsp->release();
        primaryLowPassDsp = nullptr;
    }

    for (FMOD::Channel* channel_ : oneShotChannels)
    {
        if (channel_) channel_->stop();
    }
    oneShotChannels.clear();

    for (OneShotDsp& dsp_ : oneShotDsps)
    {
        if (dsp_.channel && dsp_.dsp) dsp_.channel->removeDSP(dsp_.dsp);
        if (dsp_.dsp) dsp_.dsp->release();
    }
    oneShotDsps.clear();
}

void AudioSource::Update()
{
    CleanupStoppedChannels();
}

// 保持する音量を範囲内へ補正し、再生中の全チャンネルへ即時反映する。
void AudioSource::SetVolume(float volume_)
{
    CleanupStoppedChannels();
    volume = std::clamp(volume_, 0.0f, 1.0f);
    if (channel != nullptr) channel->setVolume(volume);
    for (FMOD::Channel* channel_ : oneShotChannels)
    {
        if (channel_) channel_->setVolume(volume);
    }
}

// 保持するPitchを範囲内へ補正し、再生中の全チャンネルへ即時反映する。
void AudioSource::SetPitch(float pitch_)
{
    CleanupStoppedChannels();
    pitch = std::clamp(pitch_, 0.01f, 4.0f);
    if (channel != nullptr) channel->setPitch(pitch);
    for (FMOD::Channel* channel_ : oneShotChannels)
    {
        if (channel_) channel_->setPitch(pitch);
    }
}

// 左右定位を範囲内へ補正し、再生中の全チャンネルへ即時反映する。
void AudioSource::SetPan(float pan_)
{
    CleanupStoppedChannels();
    pan = std::clamp(pan_, -1.0f, 1.0f);
    if (channel != nullptr) channel->setPan(pan);
    for (FMOD::Channel* channel_ : oneShotChannels)
    {
        if (channel_) channel_->setPan(pan);
    }
}

// こもり効果の有効状態を更新し、主チャンネルのDSP構成を反映する。
void AudioSource::SetMuffled(bool muffled_)
{
    muffled = muffled_;
    ApplyPrimaryMuffle();
}

// ローパスのカットオフ周波数を更新し、主チャンネルのDSPへ反映する。
void AudioSource::SetLowPassCutoff(float cutoff_)
{
    lowPassCutoff = std::clamp(cutoff_, 10.0f, 22000.0f);
    ApplyPrimaryMuffle();
}

// OneShotチャンネル専用のローパスDSPを生成し、指定周波数でこもり効果を付ける。
void AudioSource::ApplyMuffle(FMOD::Channel* channel_, bool muffled_, float cutoff_)
{
    if (channel_ == nullptr || system == nullptr || !muffled_) return;

    FMOD::DSP* lowPassDsp_ = nullptr;
    FMOD_RESULT result_ = system->createDSPByType(FMOD_DSP_TYPE_LOWPASS, &lowPassDsp_);
    _ASSERT_EXPR_A(result_ == FMOD_OK, FMOD_ErrorString(result_));
    if (lowPassDsp_ == nullptr) return;

    lowPassDsp_->setParameterFloat(FMOD_DSP_LOWPASS_CUTOFF, std::clamp(cutoff_, 10.0f, 22000.0f));
    lowPassDsp_->setActive(true);
    channel_->addDSP(0, lowPassDsp_);
    oneShotDsps.push_back({ channel_, lowPassDsp_ });
}

// 主チャンネルのこもり設定に応じてローパスDSPを生成・更新・解除する。
void AudioSource::ApplyPrimaryMuffle()
{
    if (channel == nullptr || system == nullptr) return;

    if (!muffled)
    {
        if (primaryLowPassDsp != nullptr)
        {
            channel->removeDSP(primaryLowPassDsp);
            primaryLowPassDsp->release();
            primaryLowPassDsp = nullptr;
        }
        return;
    }

    if (primaryLowPassDsp == nullptr)
    {
        FMOD_RESULT result_ = system->createDSPByType(FMOD_DSP_TYPE_LOWPASS, &primaryLowPassDsp);
        _ASSERT_EXPR_A(result_ == FMOD_OK, FMOD_ErrorString(result_));
        if (primaryLowPassDsp != nullptr)
        {
            channel->addDSP(0, primaryLowPassDsp);
        }
    }

    if (primaryLowPassDsp != nullptr)
    {
        primaryLowPassDsp->setParameterFloat(
            FMOD_DSP_LOWPASS_CUTOFF,
            std::clamp(lowPassCutoff, 10.0f, 22000.0f));
        primaryLowPassDsp->setActive(true);
    }
}

// 共通再生設定と3D音響設定を指定チャンネルへまとめて適用する。
void AudioSource::ApplySettings(FMOD::Channel* channel_, const AudioPlaybackSettings& settings_)
{
    if (channel_ == nullptr) return;

    channel_->setVolume(std::clamp(settings_.volume, 0.0f, 1.0f));
    channel_->setPitch(std::clamp(settings_.pitch, 0.01f, 4.0f));
    channel_->setPan(std::clamp(settings_.pan, -1.0f, 1.0f));

    if (settings_.use3D)
    {
        FMOD_VECTOR position_ = { settings_.position.x, settings_.position.y, settings_.position.z };
        FMOD_VECTOR velocity_ = { settings_.velocity.x, settings_.velocity.y, settings_.velocity.z };
        channel_->set3DAttributes(&position_, &velocity_);
        channel_->set3DMinMaxDistance(
            (std::max)(settings_.minDistance, 0.01f),
            (std::max)(settings_.maxDistance, settings_.minDistance + 0.01f));
        channel_->set3DDopplerLevel((std::max)(settings_.dopplerLevel, 0.0f));
        channel_->set3DSpread(std::clamp(settings_.spread, 0.0f, 360.0f));
        channel_->set3DOcclusion(
            std::clamp(settings_.directOcclusion, 0.0f, 1.0f),
            std::clamp(settings_.reverbOcclusion, 0.0f, 1.0f));
        channel_->set3DDistanceFilter(
            settings_.useDistanceFilter,
            std::clamp(settings_.distanceFilterLevel, 0.0f, 1.0f),
            std::clamp(settings_.distanceFilterCenterFrequency, 10.0f, 22000.0f));
    }

    if (channel_ == channel)
    {
        muffled = settings_.muffled;
        lowPassCutoff = settings_.lowPassCutoff;
        ApplyPrimaryMuffle();
    }
    else
    {
        ApplyMuffle(channel_, settings_.muffled, settings_.lowPassCutoff);
    }
}

// 再生を終えたOneShotチャンネルを一覧から除外し、以後の一括設定対象から外す。
void AudioSource::CleanupStoppedChannels()
{
    oneShotDsps.erase(
        std::remove_if(
            oneShotDsps.begin(),
            oneShotDsps.end(),
            [](OneShotDsp& dsp_)
            {
                if (dsp_.dsp == nullptr) return true;

                bool isPlaying_ = false;
                if (dsp_.channel != nullptr)
                {
                    dsp_.channel->isPlaying(&isPlaying_);
                }
                if (isPlaying_) return false;

                // 再生終了済みのOneShotへ接続したDSPを解放し、FMOD側の保持を残さない。
                if (dsp_.channel != nullptr)
                {
                    dsp_.channel->removeDSP(dsp_.dsp);
                }
                dsp_.dsp->release();
                dsp_.dsp = nullptr;
                dsp_.channel = nullptr;
                return true;
            }),
        oneShotDsps.end());

    oneShotChannels.erase(
        std::remove_if(
            oneShotChannels.begin(),
            oneShotChannels.end(),
            [](FMOD::Channel* channel_)
            {
                if (channel_ == nullptr) return true;
                bool isPlaying_ = false;
                channel_->isPlaying(&isPlaying_);
                return !isPlaying_;
            }),
        oneShotChannels.end());
}
