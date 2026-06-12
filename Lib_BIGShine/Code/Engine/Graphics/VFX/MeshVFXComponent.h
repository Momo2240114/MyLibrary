#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "MeshVFXResource.h"

#include <nlohmann/json_fwd.hpp>
#include <string>
#include <vector>

class MeshVFXComponent : public Component
{
public:
    MeshVFXComponent() = default;


    const std::string& GetName() const override
    {
        static std::string name = "MeshVFXComponent";
        return name;
    }

    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void Play();
    void Stop();
    void Reset();

    MeshVFXAsset& GetAsset() { return asset; }
    const MeshVFXAsset& GetAsset() const { return asset; }
    const std::vector<MeshVFXRuntimePart>& GetRuntimeParts() const { return runtimeParts; }

    float GetTime() const { return time; }
    bool IsPlaying() const { return isPlaying; }

private:
    void RefreshAssetLists();
    void LoadPartResources();
    void LoadPartResource(size_t partIndex_);
    void LoadTexture(
        ID3D11Device* device_,
        const std::string& path_,
        UINT fallbackColor_,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& texture_);

private:
    MeshVFXAsset asset;
    std::vector<MeshVFXRuntimePart> runtimeParts;
    std::vector<std::string> modelAssetPaths;
    std::vector<std::string> textureAssetPaths;
    float time = 0.0f;
    bool isPlaying = true;
};
