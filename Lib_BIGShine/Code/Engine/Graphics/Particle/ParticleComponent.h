#pragma once

#include "Engine/Common/StringId.h"
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include "ParticleEffectAsset.h"
#include "ParticleEmitter.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class VFXMaterialResource;

// GameObjectに付くParticle再生コンポーネント。
class ParticleComponent : public Component, public IActionProvider
{
public:
    // 1つのEmitter編集単位。実行時検索用に名前のIDも保持する。
    struct EmitterTrack
    {
        std::unique_ptr<ParticleEmitter> emitter;
        ParticleSystemConstants settings;

        std::string name = "Emitter";
        StringId nameId = StringIdUtility::Hash("Emitter");
        std::string materialPath;
        std::vector<std::string> materialPaths;
        std::string texturePath;
        std::string textureFileName;
        std::string maskTexturePath;
        std::string maskTextureFileName;
        std::string gradientTexturePath;
        std::string gradientTextureFileName;
        std::string shadeGradientTexturePath;
        std::string shadeGradientTextureFileName;
        std::shared_ptr<VFXMaterialResource> materialResource;
        std::vector<std::shared_ptr<VFXMaterialResource>> materialResources;
        std::vector<DirectX::XMFLOAT4> materialRandomColors;
        DirectX::XMFLOAT4 materialRandomColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        uint32_t materialRandomSeed = 0u;

        float delay = 0.0f;
        uint32_t meshVertexSampleCount = 2048;
        ParticleBlendMode blendMode = ParticleBlendMode::ALPHA;
        ParticleBlendMode runtimeBlendMode = ParticleBlendMode::ALPHA;
        ParticleSpawnSpace spawnSpace = ParticleSpawnSpace::OWNER_RELATIVE;
        // 直線攻撃用の一時補正をPool再利用時に戻し、元のParticle設定へ持ち越さない。
        bool hasBeamVisualOverride = false;
        ParticleSystemConstants beamVisualSettingsBackup;
        ParticleSpawnSpace beamVisualSpawnSpaceBackup = ParticleSpawnSpace::OWNER_RELATIVE;
        float beamVisualDelayBackup = 0.0f;
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

    void SetAsset(const ParticleEffectAsset& asset_);
    ParticleEffectAsset GetAsset() const;
    static bool LoadAssetDataOnly(
        const std::string& path_,
        ParticleEffectAsset& asset_);

    void Play();
    void Stop();
    // 保存済みEmitter設定を変更せず、演出中の連続発生量だけを補正する。
    void SetEmissionScale(float scale_);
    // 外周Particleの半径と色を、保存設定とは別の実行時補正として渡す。
    void SetRuntimeRingAppearance(float radius_, const DirectX::XMFLOAT4& color_);
    void ClearRuntimeRingAppearance() { runtimeRingEnabled = false; }
    void ResetRuntime();
    bool HasRuntimeResources() const;
    bool PlayEmitter(const std::string& emitterName_);
    bool StopEmitter(const std::string& emitterName_);
    bool TriggerEmitter(const std::string& emitterName_);
    bool SetEmitterPosition(
        const std::string& emitterName_,
        const DirectX::XMFLOAT3& position_,
        ParticleSpawnSpace spawnSpace_ = ParticleSpawnSpace::WORLD);
    bool SetEmitterLine(
        const std::string& emitterName_,
        const DirectX::XMFLOAT3& startPosition_,
        const DirectX::XMFLOAT3& endPosition_);
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
    bool LoadEmitterMaterial(EmitterTrack& track_, const std::string& materialPath_);
    bool LoadEmitterMaterialResource(EmitterTrack& track_, const std::string& materialPath_);
    bool ConvertEmitterToMaterial(EmitterTrack& track_);
    void ApplyEmitterMaterialToSettings(
        const EmitterTrack& track_,
        ParticleSystemConstants& settings_,
        float materialTime_) const;
    void ApplyEmitterMaterialTextures(EmitterTrack& track_);
    void UpdateMeshSpawnPositions(EmitterTrack& track_);
    void RebuildEmitterResources();
    void RefreshEmitterNameIndex();
    std::string BuildAssetPathFromName() const;
    std::string BuildEmitterAssetPathFromName(size_t emitterIndex_) const;
    EmitterTrack* FindEmitterTrack(const std::string& emitterName_);
    const EmitterTrack* FindEmitterTrack(const std::string& emitterName_) const;

private:
    std::string meshSourceObjectName;
    std::string attractionTargetName;
    bool emitDissolveBoundary = false;
    float dissolveBoundaryBand = 0.15f;
    std::vector<EmitterTrack> tracks;
    std::unordered_map<StringId, size_t> emitterTrackIndexByNameId;
    std::vector<std::string> textureFiles;
    std::vector<std::string> assetFiles;
    std::string assetPath;
    std::string emitterAssetPath;
    char assetNameBuffer[128] = "NewParticle";
    int selectedEmitterIndex = 0;
    int selectedAssetIndex = -1;
    float timer = 0.0f;
    double applyFeedbackUntil = 0.0;
    bool isPlaying = false;
    float emissionScale = 1.0f;
    bool runtimeRingEnabled = false;
    float runtimeRingRadius = 0.0f;
    DirectX::XMFLOAT4 runtimeRingColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    std::vector<std::string> registeredActionNames;
};
