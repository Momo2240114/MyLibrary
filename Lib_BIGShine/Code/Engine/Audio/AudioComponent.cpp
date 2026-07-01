#include "AudioComponent.h"

#include "Engine/Audio/AudioManager.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"

#include <algorithm>
#include <cstring>
#include <imgui.h>

void AudioComponent::Start()
{
    if (useWorkspacePreset)
    {
        AudioManager::Instance().LoadAudioWorkspace(workspacePath);
        AudioManager::Instance().RegisterAudioWorkspaceActions(workspacePath);
    }
    else
    {
        AudioManager::Instance().AddAudioFromFile(audioPath, isBGM);
    }
    RegisterActions();
    if (playOnStart) Play();
}

void AudioComponent::Play()
{
    if (useWorkspacePreset)
    {
        AudioManager::Instance().LoadAudioWorkspace(workspacePath);
        AudioManager::Instance().PlayWorkspacePreset(workspacePresetName, allowOverlap);
        return;
    }

    if (audioPath.empty()) return;

    AudioManager& audioManager_ = AudioManager::Instance();
    audioManager_.AddAudioFromFile(audioPath, isBGM);
    audioManager_.SetVolumeByPath(audioPath, volume);
    audioManager_.SetMuffledByPath(audioPath, muffled, muffledCutoff);

    if (isBGM)
    {
        audioManager_.PlayBGMByPath(audioPath, loop || isBGM);
    }
    else
    {
        audioManager_.PlaySEByPath(audioPath, loop);
    }
}

void AudioComponent::Stop()
{
    if (useWorkspacePreset)
    {
        AudioManager::Instance().StopWorkspacePreset(workspacePresetName);
        return;
    }

    if (audioPath.empty()) return;
    AudioManager::Instance().StopByPath(audioPath);
}

void AudioComponent::SetMuffled(bool muffled_)
{
    muffled = muffled_;
    if (useWorkspacePreset) return;
    if (audioPath.empty()) return;
    AudioManager::Instance().SetMuffledByPath(audioPath, muffled, muffledCutoff);
}

std::string AudioComponent::GetPlayActionName() const
{
    return ResolveActionName(playActionName);
}

std::string AudioComponent::GetStopActionName() const
{
    return ResolveActionName(stopActionName);
}

std::string AudioComponent::GetMuffleActionName() const
{
    return ResolveActionName(muffleActionName);
}

std::string AudioComponent::GetClearMuffleActionName() const
{
    return ResolveActionName(clearMuffleActionName);
}

void AudioComponent::RegisterActions()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    owner_->RegisterAction("Audio", GetPlayActionName(), [this]()
        {
            Play();
        });
    owner_->RegisterAction("Audio", GetStopActionName(), [this]()
        {
            Stop();
        });
    owner_->RegisterAction("Audio", GetMuffleActionName(), [this]()
        {
            SetMuffled(true);
        });
    owner_->RegisterAction("Audio", GetClearMuffleActionName(), [this]()
        {
            SetMuffled(false);
        });
}

std::string AudioComponent::ResolveActionName(const std::string& actionName_) const
{
    if (actionName_.empty()) return {};
    if (actionPrefix.empty()) return actionName_;
    return actionPrefix + "_" + actionName_;
}

void AudioComponent::Gui()
{
    ImGui::Checkbox(IM_U8("ワークスペースプリセットを使用##AudioComponent_UseWorkspacePresetCheckbox"), &useWorkspacePreset);

    if (useWorkspacePreset)
    {
        char workspacePathBuffer_[256];
        strncpy_s(workspacePathBuffer_, workspacePath.c_str(), sizeof(workspacePathBuffer_));
        if (ImGui::InputText(IM_U8("ワークスペースJSON##AudioComponent_WorkspaceJsonInput"), workspacePathBuffer_, sizeof(workspacePathBuffer_)))
        {
            workspacePath = workspacePathBuffer_;
        }

        char presetNameBuffer_[128];
        strncpy_s(presetNameBuffer_, workspacePresetName.c_str(), sizeof(presetNameBuffer_));
        if (ImGui::InputText(IM_U8("プリセット / アクション名##AudioComponent_PresetActionNameInput"), presetNameBuffer_, sizeof(presetNameBuffer_)))
        {
            workspacePresetName = presetNameBuffer_;
        }

        if (ImGui::Button(IM_U8("ワークスペースプリセット読み込み##AudioComponent_LoadWorkspacePresetsButton")))
        {
            AudioManager::Instance().LoadAudioWorkspace(workspacePath);
            AudioManager::Instance().RegisterAudioWorkspaceActions(workspacePath);
        }

        ImGui::SameLine();
        ImGui::Checkbox(IM_U8("重複再生を許可##AudioComponent_AllowOverlapCheckbox"), &allowOverlap);

        const std::vector<std::string> presetNames_ = AudioManager::Instance().GetWorkspacePresetNames();
        if (ImGui::BeginCombo(IM_U8("ワークスペースプリセット##AudioComponent_WorkspacePresetCombo"), workspacePresetName.empty() ? IM_U8("プリセットを選択") : workspacePresetName.c_str()))
        {
            for (const std::string& presetName_ : presetNames_)
            {
                if (ImGui::Selectable(presetName_.c_str(), workspacePresetName == presetName_))
                {
                    workspacePresetName = presetName_;
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button(IM_U8("再生##AudioComponent_PlayButton"))) Play();
        ImGui::SameLine();
        if (ImGui::Button(IM_U8("停止##AudioComponent_StopButton"))) Stop();

        ImGui::SeparatorText(IM_U8("アクション##AudioComponent_ActionsHeader"));
        char prefixBuffer_[64];
        strncpy_s(prefixBuffer_, actionPrefix.c_str(), sizeof(prefixBuffer_));
        if (ImGui::InputText(IM_U8("アクション接頭辞##AudioComponent_ActionPrefixInput"), prefixBuffer_, sizeof(prefixBuffer_)))
        {
            actionPrefix = prefixBuffer_;
        }

        char playBuffer_[64];
        strncpy_s(playBuffer_, playActionName.c_str(), sizeof(playBuffer_));
        if (ImGui::InputText(IM_U8("再生アクション##AudioComponent_PlayActionInput"), playBuffer_, sizeof(playBuffer_))) playActionName = playBuffer_;

        char stopBuffer_[64];
        strncpy_s(stopBuffer_, stopActionName.c_str(), sizeof(stopBuffer_));
        if (ImGui::InputText(IM_U8("停止アクション##AudioComponent_StopActionInput"), stopBuffer_, sizeof(stopBuffer_))) stopActionName = stopBuffer_;

        ImGui::Text(IM_U8("再生: %s"), GetPlayActionName().c_str());
        ImGui::Text(IM_U8("停止: %s"), GetStopActionName().c_str());
        if (ImGui::Button(IM_U8("オーディオアクション登録##AudioComponent_RegisterAudioActionsButton"))) RegisterActions();
        return;
    }

    char pathBuffer_[256];
    strncpy_s(pathBuffer_, audioPath.c_str(), sizeof(pathBuffer_));
    if (ImGui::InputText(IM_U8("オーディオパス##AudioComponent_AudioPathInput"), pathBuffer_, sizeof(pathBuffer_)))
    {
        audioPath = pathBuffer_;
        AudioManager::Instance().AddAudioFromFile(audioPath, isBGM);
    }

    ImGui::Checkbox(IM_U8("BGM##AudioComponent_IsBgmCheckbox"), &isBGM);
    ImGui::Checkbox(IM_U8("ループ##AudioComponent_LoopCheckbox"), &loop);
    ImGui::Checkbox(IM_U8("開始時に再生##AudioComponent_PlayOnStartCheckbox"), &playOnStart);
    if (ImGui::DragFloat(IM_U8("音量##AudioComponent_VolumeDrag"), &volume, 0.01f, 0.0f, 1.0f))
    {
        volume = std::clamp(volume, 0.0f, 1.0f);
        AudioManager::Instance().SetVolumeByPath(audioPath, volume);
    }

    ImGui::SeparatorText(IM_U8("こもり##AudioComponent_MuffleHeader"));
    if (ImGui::Checkbox(IM_U8("こもり有効##AudioComponent_MuffledCheckbox"), &muffled))
    {
        SetMuffled(muffled);
    }
    if (ImGui::DragFloat(IM_U8("ローパスカットオフ##AudioComponent_LowPassCutoffDrag"), &muffledCutoff, 10.0f, 10.0f, 22000.0f))
    {
        muffledCutoff = std::clamp(muffledCutoff, 10.0f, 22000.0f);
        AudioManager::Instance().SetMuffledByPath(audioPath, muffled, muffledCutoff);
    }

    if (ImGui::Button(IM_U8("再生##AudioComponent_PlayButton_Manual"))) Play();
    ImGui::SameLine();
    if (ImGui::Button(IM_U8("停止##AudioComponent_StopButton_Manual"))) Stop();

    ImGui::SeparatorText(IM_U8("アクション##AudioComponent_ActionsHeader_Manual"));
    char prefixBuffer_[64];
    strncpy_s(prefixBuffer_, actionPrefix.c_str(), sizeof(prefixBuffer_));
    if (ImGui::InputText(IM_U8("アクション接頭辞##AudioComponent_ActionPrefixInput"), prefixBuffer_, sizeof(prefixBuffer_)))
    {
        actionPrefix = prefixBuffer_;
    }

    char playBuffer_[64];
    strncpy_s(playBuffer_, playActionName.c_str(), sizeof(playBuffer_));
    if (ImGui::InputText(IM_U8("再生アクション##AudioComponent_PlayActionInput"), playBuffer_, sizeof(playBuffer_)))
    {
        playActionName = playBuffer_;
    }

    char stopBuffer_[64];
    strncpy_s(stopBuffer_, stopActionName.c_str(), sizeof(stopBuffer_));
    if (ImGui::InputText(IM_U8("停止アクション##AudioComponent_StopActionInput"), stopBuffer_, sizeof(stopBuffer_)))
    {
        stopActionName = stopBuffer_;
    }

    char muffleBuffer_[64];
    strncpy_s(muffleBuffer_, muffleActionName.c_str(), sizeof(muffleBuffer_));
    if (ImGui::InputText(IM_U8("こもりアクション##AudioComponent_MuffleActionInput"), muffleBuffer_, sizeof(muffleBuffer_)))
    {
        muffleActionName = muffleBuffer_;
    }

    char clearBuffer_[64];
    strncpy_s(clearBuffer_, clearMuffleActionName.c_str(), sizeof(clearBuffer_));
    if (ImGui::InputText(IM_U8("こもり解除アクション##AudioComponent_ClearMuffleActionInput"), clearBuffer_, sizeof(clearBuffer_)))
    {
        clearMuffleActionName = clearBuffer_;
    }

    ImGui::Text(IM_U8("再生  : %s"), GetPlayActionName().c_str());
    ImGui::Text(IM_U8("停止  : %s"), GetStopActionName().c_str());
    ImGui::Text(IM_U8("こもり: %s"), GetMuffleActionName().c_str());
    ImGui::Text(IM_U8("解除  : %s"), GetClearMuffleActionName().c_str());
    if (ImGui::Button(IM_U8("オーディオアクション登録##AudioComponent_RegisterAudioActionsButton_Manual")))
    {
        RegisterActions();
    }
}

void AudioComponent::Serialize(nlohmann::json& json_) const
{
    json_["AudioPath"] = audioPath;
    json_["WorkspacePath"] = workspacePath;
    json_["WorkspacePresetName"] = workspacePresetName;
    json_["ActionPrefix"] = actionPrefix;
    json_["PlayActionName"] = playActionName;
    json_["StopActionName"] = stopActionName;
    json_["MuffleActionName"] = muffleActionName;
    json_["ClearMuffleActionName"] = clearMuffleActionName;
    json_["IsBGM"] = isBGM;
    json_["Loop"] = loop;
    json_["PlayOnStart"] = playOnStart;
    json_["UseWorkspacePreset"] = useWorkspacePreset;
    json_["AllowOverlap"] = allowOverlap;
    json_["Volume"] = volume;
    json_["MuffledCutoff"] = muffledCutoff;
    json_["Muffled"] = muffled;
}

void AudioComponent::Deserialize(const nlohmann::json& json_)
{
    audioPath = json_.value("AudioPath", std::string("Data/Audio/SE/TitleSelect.wav"));
    workspacePath = json_.value("WorkspacePath", std::string("Data/Audio/AudioWorkspace.json"));
    workspacePresetName = json_.value("WorkspacePresetName", std::string{});
    actionPrefix = json_.value("ActionPrefix", std::string{});
    playActionName = json_.value("PlayActionName", std::string("PlayAudio"));
    stopActionName = json_.value("StopActionName", std::string("StopAudio"));
    muffleActionName = json_.value("MuffleActionName", std::string("MuffleAudio"));
    clearMuffleActionName = json_.value("ClearMuffleActionName", std::string("ClearMuffleAudio"));
    isBGM = json_.value("IsBGM", false);
    loop = json_.value("Loop", false);
    playOnStart = json_.value("PlayOnStart", false);
    useWorkspacePreset = json_.value("UseWorkspacePreset", false);
    allowOverlap = json_.value("AllowOverlap", true);
    volume = std::clamp(json_.value("Volume", 1.0f), 0.0f, 1.0f);
    muffledCutoff = std::clamp(json_.value("MuffledCutoff", 1200.0f), 10.0f, 22000.0f);
    muffled = json_.value("Muffled", false);
}
