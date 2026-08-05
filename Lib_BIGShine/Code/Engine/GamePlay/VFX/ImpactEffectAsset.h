#pragma once

#include <DirectXMath.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

enum class IMPACT_EFFECT_ENTRY_TYPE
{
    // ParticleEffectAssetを生成する。
    PARTICLE,

    // MeshVFXAssetを生成する。
    MESH_VFX,

    // DecalComponentを生成して地面や壁へ貼る。
    DECAL,
};

struct ImpactEffectEntry
{
    // このEntryが生成する演出種類。
    IMPACT_EFFECT_ENTRY_TYPE type = IMPACT_EFFECT_ENTRY_TYPE::PARTICLE;
    bool enabled = true;

    // 参照するAssetパス。
    std::string assetPath;

    // Particleの場合、特定Emitterだけ再生したい時に指定する。
    std::string emitterName;

    // 生成時に適用する見た目の倍率と色。
    DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float lifeTime = 1.0f;

    // Decalなど、寿命終了前に薄く消したい場合のフェード時間。
    float fadeOutDuration = 0.0f;

    // Hit位置から法線方向へ少しずらし、めり込みを避ける。
    float normalOffset = 0.0f;
};

struct ImpactEffectAsset
{
    // 1つのImpactEffectで同時に生成する演出一覧。
    std::vector<ImpactEffectEntry> entries;
};

// ImpactEffectAssetをJSONへ保存/読み込みするための変換関数。
void to_json(nlohmann::json& json_, const ImpactEffectEntry& entry_);
void from_json(const nlohmann::json& json_, ImpactEffectEntry& entry_);
void to_json(nlohmann::json& json_, const ImpactEffectAsset& asset_);
void from_json(const nlohmann::json& json_, ImpactEffectAsset& asset_);
