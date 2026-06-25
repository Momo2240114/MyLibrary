#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <string>

class AudioComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name = "AudioComponent";
        return name;
    }

    void Start() override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void Play();
    void Stop();
    void SetMuffled(bool muffled_);
    std::string GetPlayActionName() const;
    std::string GetStopActionName() const;
    std::string GetMuffleActionName() const;
    std::string GetClearMuffleActionName() const;

private:
    void RegisterActions();
    std::string ResolveActionName(const std::string& actionName_) const;

private:
    std::string audioPath = "Data/Audio/SE/TitleSelect.wav";
    std::string workspacePath = "Data/Audio/AudioWorkspace.json";
    std::string workspacePresetName;
    std::string actionPrefix;
    std::string playActionName = "PlayAudio";
    std::string stopActionName = "StopAudio";
    std::string muffleActionName = "MuffleAudio";
    std::string clearMuffleActionName = "ClearMuffleAudio";
    bool isBGM = false;
    bool loop = false;
    bool playOnStart = false;
    bool useWorkspacePreset = false;
    bool allowOverlap = true;
    float volume = 1.0f;
    float muffledCutoff = 1200.0f;
    bool muffled = false;
};
