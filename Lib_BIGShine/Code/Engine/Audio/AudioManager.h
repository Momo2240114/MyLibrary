#pragma once

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <DirectXMath.h>

#include "Audio.h"

class CameraComponent;

// AudioWorkspace.jsonから読み込む、名前付き音声プリセット1件分の設定。
struct AudioWorkspacePreset
{
    // Editor上で表示・検索するプリセット名。
    std::string name;
    // ActionRegistryへ登録し、EventSystemから呼び出す名前。
    std::string actionName;
    // 再生対象の音声ファイルパス。
    std::string path;
    // BGM用配列へ登録するか、SE用配列へ登録するか。
    bool isBGM = false;
    // 再生時に適用する音量、Loop、3D音響などの設定。
    AudioPlaybackSettings settings;
};

// Engine内のBGM・SE・AudioWorkspaceプリセットを一元管理するシングルトンクラス。
// パス単位の重複読み込み防止、再生制御、3D Listener更新、Action登録を担当する。
class AudioManager
{
public:
    static AudioManager& Instance()
    {
        static AudioManager ins;
        return ins;
    }

    // FMODを初期化し、Engineで標準使用するBGMとSEを事前ロードする。
    void LoadFile();
    void Update();
    void Finalize();
    // Cameraの位置・向き・移動速度をFMODの3D Listenerへ反映する。
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
    // JSONから名前付き音声プリセットを読み込み、検索テーブルと音声リソースを構築する。
    bool LoadAudioWorkspace(const std::string& path_ = "Data/Audio/AudioWorkspace.json");
    // Workspaceの各プリセットをActionRegistryへ登録し、EventSystemから再生可能にする。
    void RegisterAudioWorkspaceActions(const std::string& path_ = "Data/Audio/AudioWorkspace.json");
    bool PlayWorkspacePreset(const std::string& presetName_, bool allowOverlap_ = true);
    bool StopWorkspacePreset(const std::string& presetName_);
    std::vector<std::string> GetWorkspacePresetNames() const;

    void ClearAll();

    // 音声ファイルをBGMまたはSEとして読み込み、登録先配列のIndexを返す。
    size_t AddAudioFromFile(const std::string& path_, bool isBGM_);
    void DrawGUI();

private:
    ~AudioManager() = default;
    AudioManager() = default;
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    // 読み込み済みBGMのAudioSource一覧。
    std::vector<std::unique_ptr<AudioSource>> audioBGMs;
    // 読み込み済みSEのAudioSource一覧。
    std::vector<std::unique_ptr<AudioSource>> audioSEs;
    // BGM・SEを問わず、読み込み済みとして扱う音声パス集合。
    std::set<std::string> loadedPaths;
    // BGMのファイルパスからaudioBGMs上のIndexを引く検索テーブル。
    std::unordered_map<std::string, size_t> bgmPathToIndex;
    // SEのファイルパスからaudioSEs上のIndexを引く検索テーブル。
    std::unordered_map<std::string, size_t> sePathToIndex;
    // 現在読み込まれているWorkspaceプリセット一覧。
    std::vector<AudioWorkspacePreset> workspacePresets;
    // 再登録時に古いActionを解除するため保持する登録済みAction名一覧。
    std::vector<std::string> registeredWorkspaceActionNames;
    // プリセット名またはAction名からworkspacePresetsのIndexを引く検索テーブル。
    std::unordered_map<std::string, size_t> workspacePresetToIndex;
    // Listener速度を差分計算するため保持する前フレームのCamera位置。
    DirectX::XMFLOAT3 previousListenerPosition = { 0.0f, 0.0f, 0.0f };
    // 前フレーム位置が有効で、Listener速度を計算可能かを示す。
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


