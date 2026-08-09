#include "PrecompiledHeader.h"

#include "AudioManager.h"

#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"

#include "ImGui.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>

namespace
{
    constexpr size_t INVALID_AUDIO_INDEX = (std::numeric_limits<size_t>::max)();

    DirectX::XMFLOAT3 ReadFloat3(const nlohmann::json& json_, const char* key_, const DirectX::XMFLOAT3& fallback_)
    {
        if (!json_.contains(key_) || !json_[key_].is_array() || json_[key_].size() < 3) return fallback_;
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
        "Data/Audio/BGM/Texture_Ambi01-1.wav",
        "Data/Audio/BGM/PerituneMaterial_Investigation2_1.wav",
    };

    for (const std::string& fileName_ : fileNamesBGM_)
    {
        AddAudioFromFile(fileName_, true);
    }

    const std::vector<std::string> fileNamesSE_ = {
        "Data/Audio/SE/TitleSelect.wav",
        "Data/Audio/SE/TitleMove.wav",
        "Data/Audio/SE/heartbeat.wav",
        "Data/Audio/SE/Gameover_40.wav",
        "Data/Audio/SE/GameClear.wav",
        "Data/Audio/SE/discovery.wav",
        "Data/Audio/SE/heartbeat2.wav",
    };

    for (const std::string& fileName_ : fileNamesSE_)
    {
        AddAudioFromFile(fileName_, false);
    }
}

// FMODの内部更新を呼び、再生・DSP・3D音響の状態を進める。
void AudioManager::Update()
{
    for (const std::unique_ptr<AudioSource>& audioBGM_ : audioBGMs)
    {
        if (audioBGM_) audioBGM_->Update();
    }
    for (const std::unique_ptr<AudioSource>& audioSE_ : audioSEs)
    {
        if (audioSE_) audioSE_->Update();
    }

    Audio::Instance().Update();
}

// CameraをListenerとして扱い、位置・速度・前方・上方向をFMODへ渡す。
void AudioManager::UpdateListenerFromCamera(const CameraComponent* camera_, float deltaTime_)
{
    if (camera_ == nullptr || Audio::Instance().GetSystem() == nullptr) return;

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
    hasPreviousListenerPosition = true;
}
// 管理中の音声をすべて停止・破棄してからFMOD Systemを終了する。
void AudioManager::Finalize()
{
    ClearAll();
    Audio::Instance().Finalize();
}

void AudioManager::AddBGM(std::unique_ptr<AudioSource> source_)
{
    audioBGMs.push_back(std::move(source_));
}

void AudioManager::AddSE(std::unique_ptr<AudioSource> source_)
{
    audioSEs.push_back(std::move(source_));
}

void AudioManager::PlayBGM(size_t index_) const
{
    if (index_ >= audioBGMs.size()) return;
    audioBGMs[index_]->Play(true);
}

void AudioManager::SetVolumeBGM(size_t index_, float volume_) const
{
    if (index_ >= audioBGMs.size()) return;
    audioBGMs[index_]->SetVolume(volume_);
}

void AudioManager::SetVolumeSE(size_t index_, float volume_) const
{
    if (index_ >= audioSEs.size()) return;
    audioSEs[index_]->SetVolume(volume_);
}

void AudioManager::PlaySE(size_t index_) const
{
    if (index_ >= audioSEs.size()) return;
    audioSEs[index_]->Play(false);
}

void AudioManager::StopBGM()
{
    for (auto& audioBGM_ : audioBGMs)
    {
        if (audioBGM_) audioBGM_->Stop();
    }
}

void AudioManager::StopSE()
{
    for (auto& audioSE_ : audioSEs)
    {
        if (audioSE_) audioSE_->Stop();
    }
}

void AudioManager::StopSE(int index_)
{
    if (index_ < 0 || static_cast<size_t>(index_) >= audioSEs.size()) return;
    audioSEs[index_]->Stop();
}

void AudioManager::RemoveBGM(size_t index_)
{
    if (index_ < audioBGMs.size())
    {
        audioBGMs.erase(audioBGMs.begin() + index_);
    }
}

void AudioManager::PlayBGMByPath(const std::string& path_, bool loop_)
{
    const size_t index_ = AddAudioFromFile(path_, true);
    if (index_ < audioBGMs.size()) audioBGMs[index_]->Play(loop_);
}

void AudioManager::PlaySEByPath(const std::string& path_, bool loop_)
{
    const size_t index_ = AddAudioFromFile(path_, false);
    if (index_ < audioSEs.size()) audioSEs[index_]->Play(loop_);
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
        if (index_ < audioBGMs.size()) audioBGMs[index_]->PlayOneShot(settings_);
        return;
    }

    if (index_ < audioSEs.size()) audioSEs[index_]->PlayOneShot(settings_);
}

// BGMとSEの検索テーブルからパスを探し、対応するAudioSourceを停止する。
void AudioManager::StopByPath(const std::string& path_)
{
    auto bgmIt_ = bgmPathToIndex.find(path_);
    if (bgmIt_ != bgmPathToIndex.end() && bgmIt_->second < audioBGMs.size())
    {
        audioBGMs[bgmIt_->second]->Stop();
        return;
    }

    auto seIt_ = sePathToIndex.find(path_);
    if (seIt_ != sePathToIndex.end() && seIt_->second < audioSEs.size())
    {
        audioSEs[seIt_->second]->Stop();
    }
}

void AudioManager::SetVolumeByPath(const std::string& path_, float volume_)
{
    auto bgmIt_ = bgmPathToIndex.find(path_);
    if (bgmIt_ != bgmPathToIndex.end() && bgmIt_->second < audioBGMs.size())
    {
        audioBGMs[bgmIt_->second]->SetVolume(volume_);
    }

    auto seIt_ = sePathToIndex.find(path_);
    if (seIt_ != sePathToIndex.end() && seIt_->second < audioSEs.size())
    {
        audioSEs[seIt_->second]->SetVolume(volume_);
    }
}

void AudioManager::SetPitchByPath(const std::string& path_, float pitch_)
{
    auto bgmIt_ = bgmPathToIndex.find(path_);
    if (bgmIt_ != bgmPathToIndex.end() && bgmIt_->second < audioBGMs.size())
    {
        audioBGMs[bgmIt_->second]->SetPitch(pitch_);
    }

    auto seIt_ = sePathToIndex.find(path_);
    if (seIt_ != sePathToIndex.end() && seIt_->second < audioSEs.size())
    {
        audioSEs[seIt_->second]->SetPitch(pitch_);
    }
}

void AudioManager::SetPanByPath(const std::string& path_, float pan_)
{
    auto bgmIt_ = bgmPathToIndex.find(path_);
    if (bgmIt_ != bgmPathToIndex.end() && bgmIt_->second < audioBGMs.size())
    {
        audioBGMs[bgmIt_->second]->SetPan(pan_);
    }

    auto seIt_ = sePathToIndex.find(path_);
    if (seIt_ != sePathToIndex.end() && seIt_->second < audioSEs.size())
    {
        audioSEs[seIt_->second]->SetPan(pan_);
    }
}

void AudioManager::SetMuffledByPath(const std::string& path_, bool muffled_, float cutoff_)
{
    auto bgmIt_ = bgmPathToIndex.find(path_);
    if (bgmIt_ != bgmPathToIndex.end() && bgmIt_->second < audioBGMs.size())
    {
        audioBGMs[bgmIt_->second]->SetLowPassCutoff(cutoff_);
        audioBGMs[bgmIt_->second]->SetMuffled(muffled_);
    }

    auto seIt_ = sePathToIndex.find(path_);
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
    if (!file_.is_open()) return false;

    nlohmann::json json_;
    file_ >> json_;

    // 別Workspaceを読み込むため、前回のプリセット内容と検索テーブルを初期化する。
    workspacePresets.clear();
    workspacePresetToIndex.clear();

    if (!json_.contains("Presets") || !json_["Presets"].is_array()) return false;

    for (const auto& presetJson_ : json_["Presets"])
    {
        // JSON 1要素を再生可能なプリセット構造体へ変換する。
        AudioWorkspacePreset preset_;
        preset_.name = presetJson_.value("Name", std::string("New Audio"));
        preset_.actionName = presetJson_.value("ActionName", preset_.name);
        preset_.path = presetJson_.value("Path", std::string{});
        preset_.isBGM = presetJson_.value("IsBGM", false);
        preset_.settings = ReadPlaybackSettings(presetJson_);
        if (preset_.path.empty()) continue;

        const size_t index_ = workspacePresets.size();
        workspacePresets.push_back(preset_);
        if (!preset_.name.empty()) workspacePresetToIndex[preset_.name] = index_;
        if (!preset_.actionName.empty()) workspacePresetToIndex[preset_.actionName] = index_;
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
        if (actionName_.empty()) continue;
        if (registry_.HasActionInCategory("Audio", actionName_)) continue;

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
        if (!LoadAudioWorkspace()) return false;
        it_ = workspacePresetToIndex.find(presetName_);
        if (it_ == workspacePresetToIndex.end()) return false;
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
        if (!LoadAudioWorkspace()) return false;
        it_ = workspacePresetToIndex.find(presetName_);
        if (it_ == workspacePresetToIndex.end()) return false;
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
                if (!entry_.is_regular_file()) continue;

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

    audioBGMs.clear();
    audioSEs.clear();
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
    if (path_.empty()) return INVALID_AUDIO_INDEX;

    if (isBGM_)
    {
        auto it_ = bgmPathToIndex.find(path_);
        if (it_ != bgmPathToIndex.end()) return it_->second;
    }
    else
    {
        auto it_ = sePathToIndex.find(path_);
        if (it_ != sePathToIndex.end()) return it_->second;
    }

    AudioSource* raw_ = Audio::Instance().LoadAudioSource(path_.c_str());
    if (!raw_) return INVALID_AUDIO_INDEX;

    if (isBGM_)
    {
        const size_t index_ = audioBGMs.size();
        audioBGMs.push_back(std::unique_ptr<AudioSource>(raw_));
        bgmPathToIndex[path_] = index_;
        loadedPaths.insert(path_);
        return index_;
    }

    const size_t index_ = audioSEs.size();
    audioSEs.push_back(std::unique_ptr<AudioSource>(raw_));
    sePathToIndex[path_] = index_;
    loadedPaths.insert(path_);
    return index_;
}


