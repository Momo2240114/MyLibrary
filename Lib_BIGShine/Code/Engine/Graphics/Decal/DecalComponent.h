#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "DecalManager.h"

#include <memory>
#include <string>

class VFXMaterialResource;

class DecalComponent : public Component
{
public:
    struct DecalConstantBuffer
    {
        DirectX::XMMATRIX decal_inverse_transform;
        DirectX::XMFLOAT4 decal_direction;
    };

    const std::string& GetName() const override { return componentName; }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture;
    DirectX::XMFLOAT4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4X4 invWorldMatrix;

    void Start() override;
    void SetTexture(const std::string& path_);
    void SetMaterial(const std::string& path_);
    const std::string& GetMaterialPath() const { return materialPath; }
    const std::string& GetTexturePath() const { return texturePath; }
    void SetLifeTime(float lifeTime_, float fadeOutDuration_ = 0.0f);
    void SetDestroyOnLifeEnd(bool destroyOnLifeEnd_)
    {
        destroyOnLifeEnd = destroyOnLifeEnd_;
    }
    void ResetLifeTime();
    void SubmitDecal(DecalManager& manager_);
    void Update(float elapsedTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

private:
    void ApplyMaterialToDecalData(DecalData& data_) const;

private:
    bool showDebugBounds = true;
    bool destroyOnLifeEnd = false;
    float lifeTime = 0.0f;
    float fadeOutDuration = 0.0f;
    float elapsedTime = 0.0f;
    float materialTime = 0.0f;
    float initialAlpha = 1.0f;
    static inline const std::string componentName = "DecalComponent";
    std::string texturePath;
    std::string materialPath;
    std::shared_ptr<VFXMaterialResource> materialResource;
};
