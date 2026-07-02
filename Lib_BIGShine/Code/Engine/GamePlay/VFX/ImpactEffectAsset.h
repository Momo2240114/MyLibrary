#pragma once

#include <DirectXMath.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

enum class IMPACT_EFFECT_ENTRY_TYPE
{
    PARTICLE,
    MESH_VFX,
    DECAL,
};

struct ImpactEffectEntry
{
    IMPACT_EFFECT_ENTRY_TYPE type = IMPACT_EFFECT_ENTRY_TYPE::PARTICLE;
    bool enabled = true;
    std::string assetPath;
    std::string emitterName;
    DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float lifeTime = 1.0f;
    float fadeOutDuration = 0.0f;
    float normalOffset = 0.0f;
};

struct ImpactEffectAsset
{
    std::vector<ImpactEffectEntry> entries;
};

void to_json(nlohmann::json& json_, const ImpactEffectEntry& entry_);
void from_json(const nlohmann::json& json_, ImpactEffectEntry& entry_);
void to_json(nlohmann::json& json_, const ImpactEffectAsset& asset_);
void from_json(const nlohmann::json& json_, ImpactEffectAsset& asset_);
