#include "PrecompiledHeader.h"
#include "Engine/Core/GameRuntimeSettings.h"

#include "AudioManager.h"

#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"

#include "ImGui.h"

#include "Engine/Debug/TracyProfiler.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <utility>
#include <nlohmann/json.hpp>

namespace
{
    constexpr size_t INVALID_AUDIO_INDEX = (std::numeric_limits<size_t>::max)();

    // 古いSceneや再生スナップショットのSE指定も、今回採用した音源へ統一する。
    std::string ResolveSeAssetPath(const std::string& path_)
    {
        const std::string normalizedPath_ = std::filesystem::path(path_).lexically_normal().generic_string();
        static const std::pair<const char*, const char*> SE_PATH_REPLACEMENTS[] =
        {
            { "TitleSelect.wav", "Sword-Fight-1.wav" },
            { "TitleMove.wav", "Sword-Practice-2.wav" },
            { "heartbeat.wav", "Sonniss/Presentation/RattlingDowner.wav" },
            { "Gameover_40.wav", "Slow-motion-ends.wav" },
            { "GameClear.wav", "Entering-slow-motion.wav" },
            { "discovery.wav", "Swordsmanship-3.wav" },
            { "heartbeat2.wav", "Sonniss/Presentation/BassDropFast.wav" }
        };
        for (const auto& replacement_ : SE_PATH_REPLACEMENTS)
        {
            if (normalizedPath_ == std::string("Data/Audio/SE/") + replacement_.first)
            {
                return std::string("Data/Audio/SE/") + replacement_.second;
            }
        }
        return path_;
    }

    DirectX::XMFLOAT3 ReadFloat3(const nlohmann::json& json_, const char* key_, const DirectX::XMFLOAT3& fallback_)
    {
        if (!json_.contains(key_) || !json_[key_].is_array() || json_[key_].size() < 3)
        {
            return fallback_;
        }
        return {
            json_[key_][0].get<float>(),
            json_[key_][1].get<float>(),
            json_[key_][2].get<float>()
        };
    }

    AudioPlaybackSettings ReadPlaybackSettings(const nlohmann::json& json_)
    {
        AudioPlaybackSettings settings_;
        settings_.loop = json_.value("Loop", false);
        settings_.volume = std::clamp(json_.value("Volume", 1.0f), 0.0f, 1.0f);
        settings_.pitch = std::clamp(json_.value("Pitch", 1.0f), 0.01f, 4.0f);
        settings_.pan = std::clamp(json_.value("Pan", 0.0f), -1.0f, 1.0f);
        settings_.muffled = json_.value("Muffled", false);
        settings_.lowPassCutoff = std::clamp(json_.value("LowPassCutoff", 1200.0f), 10.0f, 22000.0f);
        settings_.use3D = json_.value("Use3D", false);
        settings_.position = ReadFloat3(json_, "Position", { 0.0f, 0.0f, 0.0f });
        settings_.minDistance = (std::max)(json_.value("MinDistance", 1.0f), 0.01f);
        settings_.maxDistance = (std::max)(json_.value("MaxDistance", 30.0f), settings_.minDistance + 0.01f);
        settings_.dopplerLevel = (std::max)(json_.value("DopplerLevel", 1.0f), 0.0f);
        settings_.spread = std::clamp(json_.value("Spread", 0.0f), 0.0f, 360.0f);
        settings_.directOcclusion = std::clamp(json_.value("DirectOcclusion", 0.0f), 0.0f, 1.0f);
        settings_.reverbOcclusion = std::clamp(json_.value("ReverbOcclusion", 0.0f), 0.0f, 1.0f);
        settings_.useDistanceFilter = json_.value("UseDistanceFilter", false);
        settings_.distanceFilterLevel = std::clamp(json_.value("DistanceFilterLevel", 1.0f), 0.0f, 1.0f);
        settings_.distanceFilterCenterFrequency = std::clamp(json_.value("DistanceFilterCenterFrequency", 1500.0f), 10.0f, 22000.0f);
        return settings_;
    }
}

// FMODを初期化し、標準BGM・SEをパス一覧から事前登録する。
void AudioManager::LoadFile()
{
    Audio::Instance().Initialize();

    const std::vector<std::string> fileNamesBGM_ = {
        "Data/Audio/BGM/PerituneMaterial_Investigation2.mp3",
        "Data/Audio/BGM/PerituneMaterial_Investigation2_1.wav",
        "Data/Audio/BGM/PerituneMaterial_EpicBattle2.mp3",
        "Data/Audio/BGM/PerituneMaterial_Sword_Flash_loop.mp3",
        "Data/Audio/BGM/PerituneMaterial_Raid_FolkMetal2.mp3",
        "Data/Audio/BGM/PerituneMaterial_EpicBattle_Deity.mp3",
    };

    for (const std::string& fileName_ : fileNamesBGM_)
    {
        const size_t index_ = AddAudioFromFile(fileName_, true);
        SetVolumeBGM(index_, 0.65f);
    }

    const std::vector<std::string> fileNamesSE_ = {
        "Data/Audio/SE/Sword-Fight-1.wav",
        "Data/Audio/SE/Sword-Practice-2.wav",
        "Data/Audio/SE/Sonniss/Presentation/RattlingDowner.wav",
        "Data/Audio/SE/Slow-motion-ends.wav",
        "Data/Audio/SE/Entering-slow-motion.wav",
        "Data/Audio/SE/Swordsmanship-3.wav",
        "Data/Audio/SE/Sonniss/Presentation/BassDropFast.wav",
        // 戦闘開始時の初回読み込みを避けるため、採用したSonniss音源を先に登録する。
        "Data/Audio/SE/Swordsmanship-2.wav",
        "Data/Audio/SE/Sonniss/Combat/LightHit.wav",
        "Data/Audio/SE/Sonniss/Combat/MetalWeaponHit.wav",
        "Data/Audio/SE/Sonniss/Dragon/RoarTrex.wav",
        "Data/Audio/SE/Sonniss/Dragon/FireWhoosh.wav",
        "Data/Audio/SE/Sonniss/Mud/LiquidHit.wav",
        "Data/Audio/SE/Sonniss/Mud/SplatHit.wav",
        "Data/Audio/SE/Sonniss/Shadow/TentacleMotion.wav",
        "Data/Audio/SE/Sonniss/Boss/MonsterGrowl.wav",
        "Data/Audio/SE/Sonniss/Presentation/BassDropFast.wav",
        "Data/Audio/SE/Sonniss/Presentation/RattlingDowner.wav",
    };

    for (const std::string& fileName_ : fileNamesSE_)
    {
        AddAudioFromFile(fileName_, false);
    }
    ApplyUserVolumes();
}

// FMODの内部更新を呼び、再生・DSP・3D音響の状態を進める。
void AudioManager::Update()
{
    UpdatePresentationAudio();
    for (const std::unique_ptr<AudioSource>& audioBGM_ : audioBGMs)
    {
        if (audioBGM_)
        {
            audioBGM_->Update();
        }
    }
    for (const std::unique_ptr<AudioSource>& audioSE_ : audioSEs)
    {
        if (audioSE_)
        {
            audioSE_->Update();
        }
    }

    Audio::Instance().Update();
}

// 全体へ軽いこもり、BGMへ追加のこもりを適用し、演出中に追加された音も同じ状態にする。
void AudioManager::EnsurePresentationAudio()
{
    BS_PROFILE_SCOPE("AudioManager::EnsurePresentationAudio");
    auto system_ = Audio::Instance().GetSystem();
    FMOD::ChannelGroup* master_ = nullptr;
    if (!system_ || system_->getMasterChannelGroup(&master_) != FMOD_OK || !master_)
    {
        return;
    }
    const auto attach_ = [&](FMOD::ChannelGroup* group_, FMOD::DSP*& dsp_)
    {
        if (!group_ || dsp_)
        {
            return;
        }
        if (system_->createDSPByType(FMOD_DSP_TYPE_LOWPASS, &dsp_) != FMOD_OK)
        {
            dsp_ = nullptr;
            return;
        }
        dsp_->setBypass(true);
        if (group_->addDSP(0, dsp_) != FMOD_OK)
        {
            dsp_->release();
            dsp_ = nullptr;
        }
    };
    attach_(master_, sharedMuffleDsp);
    const auto ensureGroup_ = [&](const char* name_, FMOD::ChannelGroup*& group_)
    {
        if (group_ || system_->createChannelGroup(name_, &group_) != FMOD_OK || !group_)
        {
            return false;
        }
        if (master_->addGroup(group_) != FMOD_OK)
        {
            group_->release();
            group_ = nullptr;
            return false;
        }
        return true;
    };
    const bool createdBgm_ = ensureGroup_("BGM Presentation", bgmPresentationGroup);
    const bool createdSe_ = ensureGroup_("Sound Effects", seGroup);
    if (createdBgm_ || createdSe_)
    {
        ApplyUserVolumes();
    }
    attach_(bgmPresentationGroup, bgmMuffleDsp);
    attach_(bgmPresentationGroup, bgmDistanceDsp);
}

void AudioManager::ApplyUserVolumes()
{
    const auto& preferences_ = GameRuntimeSettings::GetUserPreferences();
    auto* system_ = Audio::Instance().GetSystem();
    FMOD::ChannelGroup* master_ = nullptr;
    if (system_ && system_->getMasterChannelGroup(&master_) == FMOD_OK && master_)
    {
        master_->setVolume(preferences_.masterVolume);
    }
    if (bgmPresentationGroup)
    {
        bgmPresentationGroup->setVolume(preferences_.bgmVolume * deathBgmGain);
    }
    if (seGroup)
    {
        seGroup->setVolume(preferences_.seVolume);
    }
}

void AudioManager::PlaySpatialAttackSound(const std::string& path_, const DirectX::XMFLOAT3& position_,
    float volume_, float minDistance_, float maxDistance_, bool muffled_, float cutoff_)
{
    BS_PROFILE_SCOPE("AudioManager::PlaySpatialAttackSound");
    if (path_.empty() || !std::isfinite(volume_) || !std::isfinite(minDistance_) || !std::isfinite(maxDistance_))
    {
        return;
    }
    AudioPlaybackSettings settings_;
    settings_.use3D = true;
    settings_.position = position_;
    settings_.volume = std::clamp(volume_, 0.0f, 1.0f);
    settings_.minDistance = (std::max)(minDistance_, 0.01f);
    settings_.maxDistance = (std::max)(maxDistance_, settings_.minDistance + 0.01f);
    settings_.muffled = muffled_;
    settings_.lowPassCutoff = std::isfinite(cutoff_) ? std::clamp(cutoff_, 10.0f, 22000.0f) : 1200.0f;
    // 短い攻撃音では移動速度を追跡せず、カメラ移動による不要な音程変化を避ける。
    settings_.dopplerLevel = 0.0f;
    PlayOneShotByPath(path_, settings_, false);
}

void AudioManager::PlayTimedMuffle(float duration_, float cutoff_)
{
    BS_PROFILE_SCOPE("AudioManager::PlayTimedMuffle");
    if (!std::isfinite(duration_) || !std::isfinite(cutoff_) || duration_ <= 0.0f)
    {
        return;
    }
    EnsurePresentationAudio();
    muffleDuration = muffleRemaining = std::clamp(duration_, 0.05f, 10.0f);
    muffleCutoff = std::clamp(cutoff_, 10.0f, 22000.0f);
}

// スローとは独立した実時間でこもりを解除し、Editor一時停止中は残り時間を保持する。
void AudioManager::UpdatePresentationAudio()
{
    BS_PROFILE_SCOPE("AudioManager::UpdatePresentationAudio");
    const auto now_ = std::chrono::steady_clock::now();
    const float deltaTime_ = std::chrono::duration<float>(now_ - previousAudioUpdate).count();
    previousAudioUpdate = now_;
    if (editorPlayPaused || (!bgmMuffleDsp && !sharedMuffleDsp))
    {
        return;
    }
    muffleRemaining = (std::max)(muffleRemaining - deltaTime_, 0.0f);
    const float releaseTime_ = (std::min)(muffleDuration * 0.4f, 0.2f);
    const float attackTime_ = (std::min)(muffleDuration * 0.1f, 0.025f);
    const float amount_ = (std::min)(std::clamp((muffleDuration - muffleRemaining) / attackTime_, 0.0f, 1.0f),
        std::clamp(muffleRemaining / releaseTime_, 0.0f, 1.0f));
    if (bgmMuffleDsp)
    {
        bgmMuffleDsp->setParameterFloat(FMOD_DSP_LOWPASS_CUTOFF, 22000.0f * std::pow(muffleCutoff / 22000.0f, amount_));
        bgmMuffleDsp->setBypass(muffleRemaining <= 0.0f);
    }
    // SEの輪郭を残しながら、BGMと同じ立ち上がり・復帰で軽いこもりを加える。
    constexpr float MIN_SHARED_CUTOFF = 4800.0f;
    const float sharedCutoff_ = (std::max)(muffleCutoff, MIN_SHARED_CUTOFF);
    if (sharedMuffleDsp)
    {
        sharedMuffleDsp->setParameterFloat(FMOD_DSP_LOWPASS_CUTOFF, 22000.0f * std::pow(sharedCutoff_ / 22000.0f, amount_));
        sharedMuffleDsp->setBypass(muffleRemaining <= 0.0f);
    }
}

void AudioManager::SetDeathBgmEffect(float amount_)
{
    BS_PROFILE_SCOPE("AudioManager::SetDeathBgmEffect");
    if (!std::isfinite(amount_))
    {
        return;
    }
    EnsurePresentationAudio();
    const float amountValue_ = std::clamp(amount_, 0.0f, 1.0f);
    deathBgmGain = (1.0f - amountValue_) * (1.0f - amountValue_);
    if (bgmPresentationGroup)
    {
        bgmPresentationGroup->setVolume(GameRuntimeSettings::GetUserPreferences().bgmVolume * deathBgmGain);
    }
    if (bgmDistanceDsp)
    {
        bgmDistanceDsp->setParameterFloat(FMOD_DSP_LOWPASS_CUTOFF, 22000.0f * std::pow(600.0f / 22000.0f, amountValue_));
        bgmDistanceDsp->setBypass(amountValue_ <= 0.0f);
    }
}

void AudioManager::ResetPresentationAudio()
{
    BS_PROFILE_SCOPE("AudioManager::ResetPresentationAudio");
    muffleRemaining = 0.0f;
    hasPreviousListenerPosition = false;
    previousAudioUpdate = std::chrono::steady_clock::now();
    if (bgmMuffleDsp)
    {
        bgmMuffleDsp->setBypass(true);
    }
    if (sharedMuffleDsp)
    {
        sharedMuffleDsp->setBypass(true);
    }
    if (bgmDistanceDsp)
    {
        bgmDistanceDsp->setBypass(true);
    }
    deathBgmGain = 1.0f;
    if (bgmPresentationGroup)
    {
        bgmPresentationGroup->setVolume(GameRuntimeSettings::GetUserPreferences().bgmVolume);
    }
}

// CameraをListenerとして扱い、位置・速度・前方・上方向をFMODへ渡す。
void AudioManager::UpdateListenerFromCamera(const CameraComponent* camera_, float deltaTime_)
{
    if (camera_ == nullptr || Audio::Instance().GetSystem() == nullptr)
    {
        return;
    }

    // Listener位置にはCameraの視点位置、向きにはCameraの前方・上方向を使用する。
    const DirectX::XMFLOAT3 position_ = camera_->GetEye();
    const DirectX::XMFLOAT3 forward_ = camera_->GetFront();
    const DirectX::XMFLOAT3 up_ = camera_->GetUp();

    // 前フレーム位置との差をdeltaTimeで割り、ドップラー計算に必要なListener速度を求める。
    DirectX::XMFLOAT3 velocity_ = { 0.0f, 0.0f, 0.0f };
    if (hasPreviousListenerPosition && deltaTime_ > 0.0001f)
    {
        velocity_.x = (position_.x - previousListenerPosition.x) / deltaTime_;
        velocity_.y = (position_.y - previousListenerPosition.y) / deltaTime_;
        velocity_.z = (position_.z - previousListenerPosition.z) / deltaTime_;
    }

    FMOD_VECTOR fmodPosition_ = { position_.x, position_.y, position_.z };
    FMOD_VECTOR fmodVelocity_ = { velocity_.x, velocity_.y, velocity_.z };
    FMOD_VECTOR fmodForward_ = { forward_.x, forward_.y, forward_.z };
    FMOD_VECTOR fmodUp_ = { up_.x, up_.y, up_.z };

    Audio::Instance().GetSystem()->set3DListenerAttributes(
        0,
        &fmodPosition_,
        &fmodVelocity_,
        &fmodForward_,
        &fmodUp_);

    previousListenerPosition = position_;
    // 演出カメラの即時切替後は速度差分を再開せず、次の通常更新で基準位置を取り直す。
    hasPreviousListenerPosition = deltaTime_ > 0.0001f;
}

// 管理中の音声をすべて停止・破棄してからFMOD Systemを終了する。
void AudioManager::Finalize()
{
    ClearAll();
    Audio::Instance().Finalize();
}

void AudioManager::AddBGM(std::unique_ptr<AudioSource> source_)
{
    EnsurePresentationAudio();
    if (source_)
    {
        source_->SetChannelGroup(bgmPresentationGroup);
    }
    audioBGMs.push_back(std::move(source_));
}

void AudioManager::AddSE(std::unique_ptr<AudioSource> source_)
{
    EnsurePresentationAudio();
    if (source_)
    {
        source_->SetChannelGroup(seGroup);
    }
    audioSEs.push_back(std::move(source_));
}

void AudioManager::PlayBGM(size_t index_) const
{
    if (index_ >= audioBGMs.size())
    {
        return;
    }
    audioBGMs[index_]->Play(true);
    currentBgmIndex = index_;
}

void AudioManager::SwitchBGM(size_t index_)
{
    BS_PROFILE_SCOPE("AudioManager::SwitchBGM");
    if (index_ >= audioBGMs.size() || !audioBGMs[index_] || currentBgmIndex == index_)
    {
        return;
    }
    // 個別音量を維持したまま旧曲を停止し、同時に複数の戦闘曲が鳴ることを防ぐ。
    StopBGM();
    PlayBGM(index_);
}

void AudioManager::SetVolumeBGM(size_t index_, float volume_) const
{
    if (index_ >= audioBGMs.size())
    {
        return;
    }
    audioBGMs[index_]->SetVolume(volume_);
}

std::vector<float> AudioManager::CaptureBgmVolumes() const
{
    std::vector<float> volumes_;
    for (const auto& source_ : audioBGMs)
    {
        volumes_.push_back(source_ ? source_->GetVolume() : 1.0f);
    }
    return volumes_;
}

void AudioManager::ApplyBgmVolumes(const std::vector<float>& volumes_, float scale_)
{
    for (std::size_t index_ = 0; index_ < (std::min)(volumes_.size(),audioBGMs.size()); ++index_)
    {
        if (audioBGMs[index_])
        {
            audioBGMs[index_]->SetVolume(volumes_[index_]*std::clamp(scale_,0.0f,1.0f));
        }
    }
}

void AudioManager::SetVolumeSE(size_t index_, float volume_) const
{
    if (index_ >= audioSEs.size())
    {
        return;
    }
    audioSEs[index_]->SetVolume(volume_);
}

void AudioManager::PlaySE(size_t index_) const
{
    if (index_ >= audioSEs.size())
    {
        return;
    }
    audioSEs[index_]->Play(false);
}

void AudioManager::StopBGM()
{
    currentBgmIndex = INVALID_AUDIO_INDEX;
    SetDeathBgmEffect(0.0f);
    for (auto& audioBGM_ : audioBGMs)
    {
        if (audioBGM_)
        {
            audioBGM_->Stop();
        }
    }
}

void AudioManager::SetEditorPlayPaused(bool paused_)
{
    // 同じ状態を毎フレームFMODへ送らず、変更が必要な時だけMixerを切り替える。
    if (editorPlayPaused == paused_)
    {
        return;
    }
    if (auto system_ = Audio::Instance().GetSystem())
    {
        FMOD::ChannelGroup* master_ = nullptr;
        if (system_->getMasterChannelGroup(&master_) == FMOD_OK && master_ && master_->setPaused(paused_) == FMOD_OK)
        {
            editorPlayPaused = paused_;
        }
    }
}

void AudioManager::StopSE()
{
    for (auto& audioSE_ : audioSEs)
    {
        if (audioSE_)
        {
            audioSE_->Stop();
        }
    }
}

void AudioManager::StopSE(int index_)
{
    if (index_ < 0 || static_cast<size_t>(index_) >= audioSEs.size())
    {
        return;
    }
    audioSEs[index_]->Stop();
}

void AudioManager::RemoveBGM(size_t index_)
{
    // 配列の詰め直し後に、別の曲を現在曲として扱わないようにする。
    currentBgmIndex = INVALID_AUDIO_INDEX;
    if (index_ < audioBGMs.size())
    {
        audioBGMs.erase(audioBGMs.begin() + index_);
    }
}

void AudioManager::PlayBGMByPath(const std::string& path_, bool loop_)
{
    const size_t index_ = AddAudioFromFile(path_, true);
    if (index_ < audioBGMs.size())
    {
        // SceneのAudioComponentによる選曲も、通常戦闘曲と重ならないようにする。
        if (currentBgmIndex != index_)
        {
            StopBGM();
        }
        audioBGMs[index_]->Play(loop_);
        currentBgmIndex = index_;
    }
}

void AudioManager::PlaySEByPath(const std::string& path_, bool loop_)
{
    const size_t index_ = AddAudioFromFile(path_, false);
    if (index_ < audioSEs.size())
    {
        audioSEs[index_]->Play(loop_);
    }
}

// パスから音源を取得し、指定設定で既存再生と重ねるOneShot再生を行う。
void AudioManager::PlayOneShotByPath(
    const std::string& path_,
    const AudioPlaybackSettings& settings_,
    bool isBGM_)
{
    const size_t index_ = AddAudioFromFile(path_, isBGM_);
    if (isBGM_)
    {
        if (index_ < audioBGMs.size())
        {
            audioBGMs[index_]->PlayOneShot(settings_);
        }
        return;
    }

    if (index_ < audioSEs.size())
    {
        audioSEs[index_]->PlayOneShot(settings_);
    }
}

// BGMとSEの検索テーブルからパスを探し、対応するAudioSourceを停止する。
void AudioManager::StopByPath(const std::string& path_)
{
    const std::string resolvedPath_ = ResolveSeAssetPath(path_);
    auto bgmIt_ = bgmPathToIndex.find(resolvedPath_);
    if (bgmIt_ != bgmPathToIndex.end() && bgmIt_->second < audioBGMs.size())
    {
        audioBGMs[bgmIt_->second]->Stop();
        if (currentBgmIndex == bgmIt_->second)
        {
            currentBgmIndex = INVALID_AUDIO_INDEX;
        }
        return;
    }

    auto seIt_ = sePathToIndex.find(resolvedPath_);
    if (seIt_ != sePathToIndex.end() && seIt_->second < audioSEs.size())
    {
        audioSEs[seIt_->second]->Stop();
    }
}

void AudioManager::SetVolumeByPath(const std::string& path_, float volume_)
{
    const std::string resolvedPath_ = ResolveSeAssetPath(path_);
    auto bgmIt_ = bgmPathToIndex.find(resolvedPath_);
    if (bgmIt_ != bgmPathToIndex.end() && bgmIt_->second < audioBGMs.size())
    {
        audioBGMs[bgmIt_->second]->SetVolume(volume_);
    }

    auto seIt_ = sePathToIndex.find(resolvedPath_);
    if (seIt_ != sePathToIndex.end() && seIt_->second < audioSEs.size())
    {
        audioSEs[seIt_->second]->SetVolume(volume_);
    }
}

void AudioManager::SetPitchByPath(const std::string& path_, float pitch_)
{
    const std::string resolvedPath_ = ResolveSeAssetPath(path_);
    auto bgmIt_ = bgmPathToIndex.find(resolvedPath_);
    if (bgmIt_ != bgmPathToIndex.end() && bgmIt_->second < audioBGMs.size())
    {
        audioBGMs[bgmIt_->second]->SetPitch(pitch_);
    }

    auto seIt_ = sePathToIndex.find(resolvedPath_);
    if (seIt_ != sePathToIndex.end() && seIt_->second < audioSEs.size())
    {
        audioSEs[seIt_->second]->SetPitch(pitch_);
    }
}

void AudioManager::SetPanByPath(const std::string& path_, float pan_)
{
    const std::string resolvedPath_ = ResolveSeAssetPath(path_);
    auto bgmIt_ = bgmPathToIndex.find(resolvedPath_);
    if (bgmIt_ != bgmPathToIndex.end() && bgmIt_->second < audioBGMs.size())
    {
        audioBGMs[bgmIt_->second]->SetPan(pan_);
    }

    auto seIt_ = sePathToIndex.find(resolvedPath_);
    if (seIt_ != sePathToIndex.end() && seIt_->second < audioSEs.size())
    {
        audioSEs[seIt_->second]->SetPan(pan_);
    }
}

void AudioManager::SetMuffledByPath(const std::string& path_, bool muffled_, float cutoff_)
{
    const std::string resolvedPath_ = ResolveSeAssetPath(path_);
    auto bgmIt_ = bgmPathToIndex.find(resolvedPath_);
    if (bgmIt_ != bgmPathToIndex.end() && bgmIt_->second < audioBGMs.size())
    {
        audioBGMs[bgmIt_->second]->SetLowPassCutoff(cutoff_);
        audioBGMs[bgmIt_->second]->SetMuffled(muffled_);
    }

    auto seIt_ = sePathToIndex.find(resolvedPath_);
    if (seIt_ != sePathToIndex.end() && seIt_->second < audioSEs.size())
    {
        audioSEs[seIt_->second]->SetLowPassCutoff(cutoff_);
        audioSEs[seIt_->second]->SetMuffled(muffled_);
    }
}

// AudioWorkspace JSONを読み込み、プリセット一覧・検索表・AudioSourceを再構築する。
bool AudioManager::LoadAudioWorkspace(const std::string& path_)
{
    std::ifstream file_(path_);
    if (!file_.is_open())
    {
        return false;
    }

    nlohmann::json json_;
    file_ >> json_;

    // 別Workspaceを読み込むため、前回のプリセット内容と検索テーブルを初期化する。
    workspacePresets.clear();
    workspacePresetToIndex.clear();

    if (!json_.contains("Presets") || !json_["Presets"].is_array())
    {
        return false;
    }

    for (const auto& presetJson_ : json_["Presets"])
    {
        // JSON 1要素を再生可能なプリセット構造体へ変換する。
        AudioWorkspacePreset preset_;
        preset_.name = presetJson_.value("Name", std::string("New Audio"));
        preset_.actionName = presetJson_.value("ActionName", preset_.name);
        preset_.path = presetJson_.value("Path", std::string{});
        preset_.isBGM = presetJson_.value("IsBGM", false);
        preset_.settings = ReadPlaybackSettings(presetJson_);
        if (preset_.path.empty())
        {
            continue;
        }

        const size_t index_ = workspacePresets.size();
        workspacePresets.push_back(preset_);
        if (!preset_.name.empty())
        {
            workspacePresetToIndex[preset_.name] = index_;
        }
        if (!preset_.actionName.empty())
        {
            workspacePresetToIndex[preset_.actionName] = index_;
        }
        AddAudioFromFile(preset_.path, preset_.isBGM);
    }

    return !workspacePresets.empty();
}

// WorkspaceプリセットをActionRegistryへ登録し、イベントから名前で再生できるようにする。
void AudioManager::RegisterAudioWorkspaceActions(const std::string& path_)
{
    LoadAudioWorkspace(path_);

    ActionRegistry& registry_ = ActionRegistry::Instance();
    registry_.RegisterAction("Audio", "---", nullptr);

    for (const std::string& actionName_ : registeredWorkspaceActionNames)
    {
        registry_.RemoveAction("Audio", actionName_);
    }
    registeredWorkspaceActionNames.clear();

    // 各プリセットのAction名を登録し、呼び出された際に同名プリセットを再生する。
    for (const AudioWorkspacePreset& preset_ : workspacePresets)
    {
        const std::string actionName_ = preset_.actionName.empty() ? preset_.name : preset_.actionName;
        if (actionName_.empty())
        {
            continue;
        }
        if (registry_.HasActionInCategory("Audio", actionName_))
        {
            continue;
        }

        registry_.RegisterAction("Audio", actionName_, [this, actionName_]()
            {
                PlayWorkspacePreset(actionName_, true);
            });
        registeredWorkspaceActionNames.push_back(actionName_);
    }
}

// プリセット名またはAction名から設定を取得し、重複可否に応じた方法で再生する。
bool AudioManager::PlayWorkspacePreset(const std::string& presetName_, bool allowOverlap_)
{
    auto it_ = workspacePresetToIndex.find(presetName_);
    if (it_ == workspacePresetToIndex.end())
    {
        if (!LoadAudioWorkspace())
        {
            return false;
        }
        it_ = workspacePresetToIndex.find(presetName_);
        if (it_ == workspacePresetToIndex.end())
        {
            return false;
        }
    }

    const AudioWorkspacePreset& preset_ = workspacePresets[it_->second];
    // 重複再生または3D音源は独立Channelが必要なため、OneShotとして再生する。
    if (allowOverlap_ || preset_.settings.use3D)
    {
        PlayOneShotByPath(preset_.path, preset_.settings, preset_.isBGM);
        return true;
    }

    SetVolumeByPath(preset_.path, preset_.settings.volume);
    SetPitchByPath(preset_.path, preset_.settings.pitch);
    SetPanByPath(preset_.path, preset_.settings.pan);
    SetMuffledByPath(preset_.path, preset_.settings.muffled, preset_.settings.lowPassCutoff);
    if (preset_.isBGM)
    {
        PlayBGMByPath(preset_.path, preset_.settings.loop || preset_.isBGM);
    }
    else
    {
        PlaySEByPath(preset_.path, preset_.settings.loop);
    }
    return true;
}

// Workspace上の名前から音声パスを解決し、対応する再生を停止する。
bool AudioManager::StopWorkspacePreset(const std::string& presetName_)
{
    auto it_ = workspacePresetToIndex.find(presetName_);
    if (it_ == workspacePresetToIndex.end())
    {
        if (!LoadAudioWorkspace())
        {
            return false;
        }
        it_ = workspacePresetToIndex.find(presetName_);
        if (it_ == workspacePresetToIndex.end())
        {
            return false;
        }
    }

    StopByPath(workspacePresets[it_->second].path);
    return true;
}

std::vector<std::string> AudioManager::GetWorkspacePresetNames() const
{
    std::vector<std::string> names_;
    names_.reserve(workspacePresets.size());
    for (const AudioWorkspacePreset& preset_ : workspacePresets)
    {
        names_.push_back(preset_.name);
    }
    return names_;
}

// Audioフォルダーを走査し、未ロード音声を手動登録できるImGuiを描画する。
void AudioManager::DrawGUI()
{
    namespace fs = std::filesystem;

    ImGui::Begin("Audio Loader");

    auto drawFiles_ = [&](const std::string& directory_, bool isBGM_)
        {
            if (!fs::exists(directory_))
            {
                ImGui::Text("Directory not found: %s", directory_.c_str());
                return;
            }

            for (const auto& entry_ : fs::directory_iterator(directory_))
            {
                if (!entry_.is_regular_file())
            {
                continue;
            }

            const std::string path_ = entry_.path().generic_string();
                const std::string fileName_ = entry_.path().filename().string();
                const bool isLoaded_ = loadedPaths.find(path_) != loadedPaths.end();

                if (isLoaded_)
                {
                    ImGui::TextDisabled("[Loaded] %s", fileName_.c_str());
                }
                else if (ImGui::Button(fileName_.c_str()))
                {
                    AddAudioFromFile(path_, isBGM_);
                }
            }
        };

    if (ImGui::CollapsingHeader("BGM Selection", ImGuiTreeNodeFlags_DefaultOpen))
    {
        drawFiles_("Data/Audio/BGM", true);
    }

    if (ImGui::CollapsingHeader("SE Selection", ImGuiTreeNodeFlags_DefaultOpen))
    {
        drawFiles_("Data/Audio/SE", false);
    }

    ImGui::End();
}

// 全BGM・SEを停止し、AudioSourceと各種検索テーブルを空にする。
void AudioManager::ClearAll()
{
    StopBGM();
    StopSE();

    ResetPresentationAudio();
    // Source停止後にDSPとBusを解放し、次の初期化へFMODの参照を持ち越さない。
    if (sharedMuffleDsp)
    {
        FMOD::ChannelGroup* master_ = nullptr;
        if (auto system_ = Audio::Instance().GetSystem())
        {
            if (system_->getMasterChannelGroup(&master_) == FMOD_OK && master_)
            {
                master_->removeDSP(sharedMuffleDsp);
            }
        }
        sharedMuffleDsp->release();
        sharedMuffleDsp = nullptr;
    }
    if (bgmMuffleDsp)
    {
        if (bgmPresentationGroup)
        {
            bgmPresentationGroup->removeDSP(bgmMuffleDsp);
        }
        bgmMuffleDsp->release();
        bgmMuffleDsp = nullptr;
    }
    if (bgmDistanceDsp)
    {
        if (bgmPresentationGroup)
        {
            bgmPresentationGroup->removeDSP(bgmDistanceDsp);
        }
        bgmDistanceDsp->release();
        bgmDistanceDsp = nullptr;
    }
    audioBGMs.clear();
    audioSEs.clear();
    if (bgmPresentationGroup)
    {
        bgmPresentationGroup->release();
        bgmPresentationGroup = nullptr;
    }
    if (seGroup)
    {
        seGroup->release();
        seGroup = nullptr;
    }
    loadedPaths.clear();
    bgmPathToIndex.clear();
    sePathToIndex.clear();
    workspacePresets.clear();
    workspacePresetToIndex.clear();
    registeredWorkspaceActionNames.clear();
}

// 同じ分類・パスの重複読み込みを避けながら、AudioSourceをBGMまたはSEへ登録する。
size_t AudioManager::AddAudioFromFile(const std::string& path_, bool isBGM_)
{
    const std::string resolvedPath_ = ResolveSeAssetPath(path_);
    if (resolvedPath_.empty())
    {
        return INVALID_AUDIO_INDEX;
    }

    if (isBGM_)
    {
        auto it_ = bgmPathToIndex.find(resolvedPath_);
        if (it_ != bgmPathToIndex.end())
        {
            return it_->second;
        }
    }
    else
    {
        auto it_ = sePathToIndex.find(resolvedPath_);
        if (it_ != sePathToIndex.end())
        {
            return it_->second;
        }
    }

    AudioSource* raw_ = Audio::Instance().LoadAudioSource(resolvedPath_.c_str());
    if (!raw_)
    {
        return INVALID_AUDIO_INDEX;
    }
    EnsurePresentationAudio();
    raw_->SetChannelGroup(isBGM_ ? bgmPresentationGroup : seGroup);

    if (isBGM_)
    {
        const size_t index_ = audioBGMs.size();
        audioBGMs.push_back(std::unique_ptr<AudioSource>(raw_));
        bgmPathToIndex[resolvedPath_] = index_;
        loadedPaths.insert(resolvedPath_);
        return index_;
    }

    const size_t index_ = audioSEs.size();
    audioSEs.push_back(std::unique_ptr<AudioSource>(raw_));
    sePathToIndex[resolvedPath_] = index_;
    loadedPaths.insert(resolvedPath_);
    return index_;
}


