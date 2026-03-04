#include "ComponentBase.h"
#include "ParticleEmitter.h"
#include <filesystem>

class ParticleComponent : public Component
{
private:
    struct EmitterTrack
    {
        std::unique_ptr<ParticleEmitter> emitter;
        ParticleSystemConstants settings; // 編集中の設定値
        std::string texturePath;
        std::string textureFileName; // コンボボックス表示用
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
    void Gui() override; // ImGui用

    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

    // ヘルパー
    void Play();
    void RefreshTextureList();

    // エミッタートラックのリストを取得
    const std::vector<EmitterTrack>& GetTracks() const { return tracks; }
    std::vector<EmitterTrack>& GetTracks() { return tracks; }

private:

    std::vector<EmitterTrack> tracks;
    std::vector<std::string> textureFiles; // フォルダ内のファイルリスト
    int selectedEmitterIndex = 0;          // 現在編集中のエミッター
    float timer = 0.0f;
    bool isPlaying = false;

    // ロード用ヘルパー
    bool LoadEmitterTexture(EmitterTrack& track, const std::string& fullPath);
};