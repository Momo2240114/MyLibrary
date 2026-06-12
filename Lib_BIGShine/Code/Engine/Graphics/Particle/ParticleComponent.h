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

        std::string name = "Emitter";
        std::string texturePath;
        std::string textureFileName;
        std::string maskTexturePath;
        std::string maskTextureFileName;
        std::string gradientTexturePath;
        std::string gradientTextureFileName;

        float delay = 0.0f;
        uint32_t meshVertexSampleCount = 2048;
        ParticleBlendMode blendMode = ParticleBlendMode::ALPHA;
        ParticleSpawnSpace spawnSpace = ParticleSpawnSpace::OWNER_RELATIVE;
        bool isPlaybackEnabled = false;
        bool isStarted = false;

        void Reset()
        {
            isPlaybackEnabled = false;
            isStarted = false;
            if (emitter) emitter->SetEmissionEnabled(false);
        }
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
    bool PlayEmitter(const std::string& emitterName_);
    bool StopEmitter(const std::string& emitterName_);
    bool TriggerEmitter(const std::string& emitterName_);
    bool SetEmitterPosition(
        const std::string& emitterName_,
        const DirectX::XMFLOAT3& position_,
        ParticleSpawnSpace spawnSpace_ = ParticleSpawnSpace::WORLD);
    bool TriggerEmitterAtPosition(
        const std::string& emitterName_,
        const DirectX::XMFLOAT3& worldPosition_);
    void RefreshTextureList();
    void RefreshAssetList();
    bool SaveAsset(const std::string& path_);
    bool LoadAsset(const std::string& path_);
    bool SaveEmitterAsset(size_t emitterIndex_, const std::string& path_);
    bool LoadEmitterAsset(const std::string& path_);
    const std::string& GetAssetPath() const { return assetPath; }

    const std::vector<EmitterTrack>& GetTracks() const { return tracks; }
    std::vector<EmitterTrack>& GetTracks() { return tracks; }

private:
    bool LoadEmitterTexture(EmitterTrack& track, const std::string& fullPath);
    bool LoadEmitterTextureSlot(EmitterTrack& track, const std::string& fullPath, int slot);
    void UpdateMeshSpawnPositions(EmitterTrack& track_);
    void RebuildEmitterResources();
    std::string BuildAssetPathFromName() const;
    std::string BuildEmitterAssetPathFromName(size_t emitterIndex_) const;
    EmitterTrack* FindEmitterTrack(const std::string& emitterName_);
    const EmitterTrack* FindEmitterTrack(const std::string& emitterName_) const;

private:
    std::vector<EmitterTrack> tracks;
    std::vector<std::string> textureFiles;
    std::vector<std::string> assetFiles;
    std::string assetPath;
    char assetNameBuffer[128] = "NewParticle";
    int selectedEmitterIndex = 0;
    int selectedAssetIndex = -1;
    float timer = 0.0f;
    double applyFeedbackUntil = 0.0;
    bool isPlaying = false;
    std::vector<std::string> registeredActionNames;
};
