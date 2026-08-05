#pragma once

#include "AttackSystem.h"
#include "Engine/Graphics/Particle/ParticleEffectAsset.h"
#include "Engine/Graphics/VFX/MeshVFXResource.h"

#include <DirectXMath.h>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class GameObject;
class GameObjectManager;

struct ParticleSpawnRequest
{
    // Assetキャッシュ名。例: Data/Particle/Hit/Spark.jsonなら"Hit/Spark"。
    std::string assetName;

    // 互換用フィールド。旧形式のパス、またはAsset名風の文字列を受け取る。
    std::string assetPath;

    // ParticleEffectAsset 内の特定 Emitter だけを再生したい場合に指定する。
    std::string emitterName;

    // 発生位置と衝突面法線。normalOffset と組み合わせてめり込みを防ぐ。
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 normal = { 0.0f, 1.0f, 0.0f };
    float lifeTime = 1.0f;
    float normalOffset = 0.0f;
};

struct MeshVFXSpawnRequest
{
    // Assetキャッシュ名。例: Data/VFX/Slash/Impact.jsonなら"Slash/Impact"。
    std::string assetName;

    // 互換用フィールド。旧形式のパス、またはAsset名風の文字列を受け取る。
    std::string assetPath;

    // MeshVFX の生成位置と、ヒット面に沿って少し浮かせるための法線。
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 normal = { 0.0f, 1.0f, 0.0f };
    float lifeTime = 1.0f;
    float normalOffset = 0.0f;
};

class VFXManager
{
public:
    // GameObject 生成先の Manager を受け取り、以後の Spawn で使う。
    void Initialize(GameObjectManager* gameObjectManager_);

    // 寿命付きで生成した VFX を監視し、時間切れなら GameObject を破棄する。
    void Update(float deltaTime_);
    void Clear();

    // Data 配下の Particle/MeshVFX JSON を再スキャンしてキャッシュし直す。
    static void ReloadAssetCache();

    static const std::vector<std::string>& GetParticleAssetNames();
    static const std::vector<std::string>& GetMeshVFXAssetNames();

    static const ParticleEffectAsset* FindParticleAsset(
        const std::string& assetName_);
    static const MeshVFXAsset* FindMeshVFXAsset(
        const std::string& assetName_);

    std::shared_ptr<GameObject> SpawnParticle(
        const ParticleSpawnRequest& request_);
    std::shared_ptr<GameObject> SpawnMeshVFX(
        const MeshVFXSpawnRequest& request_);

    // AttackSystem のヒットイベントから、Particle と MeshVFX の両方をまとめて生成する。
    void SpawnForHit(const AttackHitEvent& event_);

private:
    struct ManagedEffect
    {
        // 生成した VFX GameObject。外部で消されてもよいよう weak_ptr で監視する。
        std::weak_ptr<GameObject> object;

        // lifeTime までの残り時間。
        float lifeTimer = 0.0f;
    };

private:
    static void LoadParticleAssetsFromDirectory(
        const std::string& rootDirectory_);
    static void LoadMeshVFXAssetsFromDirectory(
        const std::string& rootDirectory_);

    static std::string BuildAssetName(
        const std::filesystem::path& path_,
        const std::filesystem::path& rootDirectory_);

    // 旧 assetPath 指定を、キャッシュキーとして使う AssetName へ正規化する。
    static std::string BuildAssetNameFromRequestPath(
        const std::string& assetPath_,
        const std::string& rootDirectory_);

    static bool IsJsonFile(const std::filesystem::path& path_);

    // Editor 用や派生フォルダを除外したい時、特定ディレクトリ名配下か判定する。
    static bool IsUnderDirectoryName(
        const std::filesystem::path& path_,
        const std::string& directoryName_);

    static const ParticleEffectAsset* FindParticleAssetFromRequest(
        const ParticleSpawnRequest& request_);
    static const MeshVFXAsset* FindMeshVFXAssetFromRequest(
        const MeshVFXSpawnRequest& request_);

private:
    // normal を正規化して position に normalOffset 分だけ足す。
    DirectX::XMFLOAT3 ApplyNormalOffset(
        const DirectX::XMFLOAT3& position_,
        const DirectX::XMFLOAT3& normal_,
        float normalOffset_) const;
    void RegisterManagedEffect(
        const std::shared_ptr<GameObject>& object_,
        float lifeTime_);

private:
    // VFX Asset は頻繁に使うため、名前から即参照できる静的キャッシュにまとめる。
    static bool assetCacheLoaded;
    static std::unordered_map<std::string, ParticleEffectAsset> particleAssetMap;
    static std::unordered_map<std::string, MeshVFXAsset> meshVFXAssetMap;
    static std::vector<std::string> particleAssetNames;
    static std::vector<std::string> meshVFXAssetNames;

private:
    // VFX GameObject の生成・破棄先。
    GameObjectManager* gameObjectManager = nullptr;

    // 自動破棄対象の VFX 一覧。
    std::vector<ManagedEffect> managedEffects;
};
