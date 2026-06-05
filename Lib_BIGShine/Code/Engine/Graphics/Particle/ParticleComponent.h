#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "ParticleEmitter.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class ParticleComponent : public Component
{
private:
    struct EmitterTrack
    {
        std::unique_ptr<ParticleEmitter> emitter;
        ParticleSystemConstants settings;

        std::string texturePath;
        std::string textureFileName;
        std::string maskTexturePath;
        std::string maskTextureFileName;
        std::string gradientTexturePath;
        std::string gradientTextureFileName;

        float delay = 0.0f;
        bool isStarted = false;

        void Reset() { isStarted = false; }
    };

public:
    ParticleComponent() = default;
    virtual ~ParticleComponent() = default;

    const std::string& GetName() const override { static std::string name = "ParticleComponent"; return name; }

    void Start() override;
    void Update(float deltaTime) override;
    void Gui() override;

    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

    void Play();
    void RefreshTextureList();

    const std::vector<EmitterTrack>& GetTracks() const { return tracks; }
    std::vector<EmitterTrack>& GetTracks() { return tracks; }

private:
    bool LoadEmitterTexture(EmitterTrack& track, const std::string& fullPath);
    bool LoadEmitterTextureSlot(EmitterTrack& track, const std::string& fullPath, int slot);

private:
    std::vector<EmitterTrack> tracks;
    std::vector<std::string> textureFiles;
    int selectedEmitterIndex = 0;
    float timer = 0.0f;
    bool isPlaying = false;
};
