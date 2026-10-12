#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <string>

// GameObjectへ音声再生機能を追加するComponent。
// 単一ファイルまたはAudioWorkspaceプリセットを再生し、EventSystem用Actionの登録も担当する。
class AudioComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name = "AudioComponent";
        return name;
    }

    void Start() override;
    void OnDestroy() override { Stop(); }
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    // 現在の設定に従って音声ファイルまたはWorkspaceプリセットを再生する。
    void Play();
    // 現在設定されている音声またはWorkspaceプリセットを停止する。
    void Stop();
    void SetMuffled(bool muffled_);
    std::string GetPlayActionName() const;
    std::string GetStopActionName() const;
    std::string GetMuffleActionName() const;
    std::string GetClearMuffleActionName() const;

private:
    // 再生・停止・こもり切替をOwnerのLocal ActionRegistryへ登録する。
    void RegisterActions();
    // 任意の接頭辞と基本Action名を連結し、実際の登録名を生成する。
    std::string ResolveActionName(const std::string& actionName_) const;

private:
    // Workspaceを使わない場合に直接再生する音声ファイルパス。
    std::string audioPath = "Data/Audio/SE/TitleSelect.wav";
    // プリセット情報を読み込むAudioWorkspace JSONのパス。
    std::string workspacePath = "Data/Audio/AudioWorkspace.json";
    // 再生対象として選択しているWorkspaceプリセット名またはAction名。
    std::string workspacePresetName;
    // 同一GameObject上でAction名が重複しないよう付加する接頭辞。
    std::string actionPrefix;
    std::string playActionName = "PlayAudio";
    std::string stopActionName = "StopAudio";
    std::string muffleActionName = "MuffleAudio";
    std::string clearMuffleActionName = "ClearMuffleAudio";
    // 直接指定音声をBGMとして管理するか。
    bool isBGM = false;
    // 直接指定音声を繰り返し再生するか。
    bool loop = false;
    // Component開始時に自動再生するか。
    bool playOnStart = false;
    // audioPathではなくWorkspaceプリセットを再生に使用するか。
    bool useWorkspacePreset = false;
    // Workspaceプリセットの同時重複再生を許可するか。
    bool allowOverlap = true;
    // 直接指定音声へ適用する再生音量。
    float volume = 1.0f;
    // こもり表現に使用するローパスフィルターのカットオフ周波数。
    float muffledCutoff = 1200.0f;
    // 直接指定音声へこもり効果を適用するか。
    bool muffled = false;
};
