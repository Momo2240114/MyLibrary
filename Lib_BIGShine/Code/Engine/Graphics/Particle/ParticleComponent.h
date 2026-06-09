#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include "ParticleEmitter.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class ParticleComponent : public Component, public IActionProvider
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
        uint32_t meshVertexSampleCount = 2048;
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
    void RegisterActions() override;

    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

    void Play();
    void Stop();
    void RefreshTextureList();
    void RefreshAssetList();
    bool SaveAsset(const std::string& path_);
    bool LoadAsset(const std::string& path_);
    const std::string& GetAssetPath() const { return assetPath; }

    const std::vector<EmitterTrack>& GetTracks() const { return tracks; }
    std::vector<EmitterTrack>& GetTracks() { return tracks; }

private:
    bool LoadEmitterTexture(EmitterTrack& track, const std::string& fullPath);
    bool LoadEmitterTextureSlot(EmitterTrack& track, const std::string& fullPath, int slot);
    void UpdateMeshSpawnPositions(EmitterTrack& track_);
    void RebuildEmitterResources();
    std::string BuildAssetPathFromName() const;

private:
    std::vector<EmitterTrack> tracks;
    std::vector<std::string> textureFiles;
    std::vector<std::string> assetFiles;
    std::string assetPath;
    std::string textureDialogKey;
    std::string textureBrowserDirectory = "Data/Texture/Particle";
    std::string textureBrowserSelection;
    char assetNameBuffer[128] = "NewParticle";
    int selectedEmitterIndex = 0;
    int selectedAssetIndex = -1;
    int pendingTextureSlot = -1;
    float timer = 0.0f;
    double applyFeedbackUntil = 0.0;
    bool isPlaying = false;
};
