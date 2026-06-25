#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include "ParticleEmitter.h"
#include "ParticleEffectAsset.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// GameObjectに付けるParticleの入口コンポーネント
// 複数Emitterの編集、再生制御、JSON保存、イベントAction登録をまとめて担当する
class ParticleComponent : public Component, public IActionProvider
{
private:
    // 1つのEmitterスロットEditor上ではこの単位で名前、Texture、発生設定を編集する
    struct EmitterTrack
    {
        std::unique_ptr<ParticleEmitter> emitter;  // 実際にGPU更新・描画を行う低レベルEmitter
        ParticleSystemConstants settings;          // GUI/JSONで編集する発生・描画設定

        std::string name = "Emitter";              // ActionやTimelineから指定するEmitter名
        std::string texturePath;                   // 色Textureの保存パス
        std::string textureFileName;               // GUI表示用の色Textureファイル名
        std::string maskTexturePath;               // Mask Textureの保存パス
        std::string maskTextureFileName;           // GUI表示用のMask Textureファイル名
        std::string gradientTexturePath;           // Gradient Textureの保存パス
        std::string gradientTextureFileName;       // GUI表示用のGradient Textureファイル名

        float delay = 0.0f;                        // 再生開始からEmitterを有効化するまでの待ち時間
        uint32_t meshVertexSampleCount = 2048;     // Mesh Vertices発生で使う頂点サンプル最大数
        ParticleBlendMode blendMode = ParticleBlendMode::ALPHA; // 描画時の合成方法
        ParticleSpawnSpace spawnSpace = ParticleSpawnSpace::OWNER_RELATIVE; // 発生位置の座標空間
        bool isPlaybackEnabled = false;            // Play/Triggerで再生対象になっているか
        bool isStarted = false;                    // delay経過後、実際にEmitterが開始済みか

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
    void UpdateMeshSpawnPositions(EmitterTrack& track_);
    void RebuildEmitterResources();
    std::string BuildAssetPathFromName() const;
    std::string BuildEmitterAssetPathFromName(size_t emitterIndex_) const;
    EmitterTrack* FindEmitterTrack(const std::string& emitterName_);
    const EmitterTrack* FindEmitterTrack(const std::string& emitterName_) const;

private:
    std::vector<EmitterTrack> tracks;              // このComponentが持つEmitter一覧
    std::vector<std::string> textureFiles;         // Asset検索で見つけたTexture候補
    std::vector<std::string> assetFiles;           // Asset検索で見つけたParticle JSON候補
    std::string assetPath;                         // 現在読み書きしているParticle Assetのパス
    std::string emitterAssetPath;                  // 単体Emitter Assetの読み書きパス
    char assetNameBuffer[128] = "NewParticle";     // 新規保存時のAsset名入力バッファ
    int selectedEmitterIndex = 0;                  // GUIで選択中のEmitter
    int selectedAssetIndex = -1;                   // GUIで選択中のParticle Asset
    float timer = 0.0f;                            // 再生中のComponent内時間
    double applyFeedbackUntil = 0.0;               // Apply後のGUI通知を表示する終了時刻
    bool isPlaying = false;                        // Component全体が再生中か
    std::vector<std::string> registeredActionNames;// ActionRegistryへ登録した名前再登録時の解除に使う
};
