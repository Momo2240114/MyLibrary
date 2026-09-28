#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <string>
#include <vector>

// シーン開始時に指定Prefabを配置するためのComponent。
class PrefabSpawnComponent : public Component
{
public:
    struct SpawnEntry
    {
        // 生成するPrefab名。Data/Prefabs内の拡張子なしファイル名を指定する。
        std::string prefabName;
        DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 rotation = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
        bool enabled = true;
        bool selectAfterSpawn = false;
    };

    const std::string& GetName() const override;
    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

private:
    // 指定されたPrefabを生成し、シーン上の初期Transformを適用する。
    void SpawnEntryNow(const SpawnEntry& entry_);

private:
    std::vector<SpawnEntry> entries;
    bool spawnOnStart = true;
    bool hasSpawned = false;
};
