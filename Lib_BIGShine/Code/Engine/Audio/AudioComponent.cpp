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
    ImGui::Checkbox("Use Workspace Preset", &useWorkspacePreset);

    if (useWorkspacePreset)
    {
        char workspacePathBuffer_[256];
        strncpy_s(workspacePathBuffer_, workspacePath.c_str(), sizeof(workspacePathBuffer_));
        if (ImGui::InputText("Workspace JSON", workspacePathBuffer_, sizeof(workspacePathBuffer_)))
        {
            workspacePath = workspacePathBuffer_;
        }

        char presetNameBuffer_[128];
        strncpy_s(presetNameBuffer_, workspacePresetName.c_str(), sizeof(presetNameBuffer_));
        if (ImGui::InputText("Preset / Action Name", presetNameBuffer_, sizeof(presetNameBuffer_)))
        {
            workspacePresetName = presetNameBuffer_;
        }

        if (ImGui::Button("Load Workspace Presets"))
        {
            AudioManager::Instance().LoadAudioWorkspace(workspacePath);
            AudioManager::Instance().RegisterAudioWorkspaceActions(workspacePath);
        }

        ImGui::SameLine();
        ImGui::Checkbox("Allow Overlap", &allowOverlap);

        const std::vector<std::string> presetNames_ = AudioManager::Instance().GetWorkspacePresetNames();
        if (ImGui::BeginCombo("Workspace Preset", workspacePresetName.empty() ? "Select Preset" : workspacePresetName.c_str()))
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

        if (ImGui::Button("Play")) Play();
        ImGui::SameLine();
        if (ImGui::Button("Stop")) Stop();

        ImGui::SeparatorText("Actions");
        char prefixBuffer_[64];
        strncpy_s(prefixBuffer_, actionPrefix.c_str(), sizeof(prefixBuffer_));
        if (ImGui::InputText("Action Prefix", prefixBuffer_, sizeof(prefixBuffer_)))
        {
            actionPrefix = prefixBuffer_;
        }

        char playBuffer_[64];
        strncpy_s(playBuffer_, playActionName.c_str(), sizeof(playBuffer_));
        if (ImGui::InputText("Play Action", playBuffer_, sizeof(playBuffer_))) playActionName = playBuffer_;

        char stopBuffer_[64];
        strncpy_s(stopBuffer_, stopActionName.c_str(), sizeof(stopBuffer_));
        if (ImGui::InputText("Stop Action", stopBuffer_, sizeof(stopBuffer_))) stopActionName = stopBuffer_;

        ImGui::Text("Play: %s", GetPlayActionName().c_str());
        ImGui::Text("Stop: %s", GetStopActionName().c_str());
        if (ImGui::Button("Register Audio Actions")) RegisterActions();
        return;
    }

    char pathBuffer_[256];
    strncpy_s(pathBuffer_, audioPath.c_str(), sizeof(pathBuffer_));
    if (ImGui::InputText("Audio Path", pathBuffer_, sizeof(pathBuffer_)))
    {
        audioPath = pathBuffer_;
        AudioManager::Instance().AddAudioFromFile(audioPath, isBGM);
    }

    ImGui::Checkbox("Is BGM", &isBGM);
    ImGui::Checkbox("Loop", &loop);
    ImGui::Checkbox("Play On Start", &playOnStart);
    if (ImGui::DragFloat("Volume", &volume, 0.01f, 0.0f, 1.0f))
    {
        volume = std::clamp(volume, 0.0f, 1.0f);
        AudioManager::Instance().SetVolumeByPath(audioPath, volume);
    }

    ImGui::SeparatorText("Muffle");
    if (ImGui::Checkbox("Muffled", &muffled))
    {
        SetMuffled(muffled);
    }
    if (ImGui::DragFloat("Low Pass Cutoff", &muffledCutoff, 10.0f, 10.0f, 22000.0f))
    {
        muffledCutoff = std::clamp(muffledCutoff, 10.0f, 22000.0f);
        AudioManager::Instance().SetMuffledByPath(audioPath, muffled, muffledCutoff);
    }

    if (ImGui::Button("Play")) Play();
    ImGui::SameLine();
    if (ImGui::Button("Stop")) Stop();

    ImGui::SeparatorText("Actions");
    char prefixBuffer_[64];
    strncpy_s(prefixBuffer_, actionPrefix.c_str(), sizeof(prefixBuffer_));
    if (ImGui::InputText("Action Prefix", prefixBuffer_, sizeof(prefixBuffer_)))
    {
        actionPrefix = prefixBuffer_;
    }

    char playBuffer_[64];
    strncpy_s(playBuffer_, playActionName.c_str(), sizeof(playBuffer_));
    if (ImGui::InputText("Play Action", playBuffer_, sizeof(playBuffer_)))
    {
        playActionName = playBuffer_;
    }

    char stopBuffer_[64];
    strncpy_s(stopBuffer_, stopActionName.c_str(), sizeof(stopBuffer_));
    if (ImGui::InputText("Stop Action", stopBuffer_, sizeof(stopBuffer_)))
    {
        stopActionName = stopBuffer_;
    }

    char muffleBuffer_[64];
    strncpy_s(muffleBuffer_, muffleActionName.c_str(), sizeof(muffleBuffer_));
    if (ImGui::InputText("Muffle Action", muffleBuffer_, sizeof(muffleBuffer_)))
    {
        muffleActionName = muffleBuffer_;
    }

    char clearBuffer_[64];
    strncpy_s(clearBuffer_, clearMuffleActionName.c_str(), sizeof(clearBuffer_));
    if (ImGui::InputText("Clear Muffle Action", clearBuffer_, sizeof(clearBuffer_)))
    {
        clearMuffleActionName = clearBuffer_;
    }

    ImGui::Text("Play  : %s", GetPlayActionName().c_str());
    ImGui::Text("Stop  : %s", GetStopActionName().c_str());
    ImGui::Text("Muffle: %s", GetMuffleActionName().c_str());
    ImGui::Text("Clear : %s", GetClearMuffleActionName().c_str());
    if (ImGui::Button("Register Audio Actions"))
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
