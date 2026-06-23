#pragma once

#include "MeshVFXComponent.h"

class MaterialOverlayComponent : public MeshVFXComponent
{
public:
    MaterialOverlayComponent();

    const std::string& GetName() const override
    {
        static std::string name = "MaterialOverlayComponent";
        return name;
    }

    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    bool UsesOwnerModel() const override { return false; }
    bool IsOverlayActive() const { return overlayActive || overlayWeight > 0.0f; }
    float GetOverlayWeight() const;
    void StartOverlay();
    void StopOverlay();
    void ResetOverlay();

private:
    void RegisterActions();

    bool overlayActive = false;
    float overlayWeight = 0.0f;
    float fadeInDuration = 0.08f;
    float fadeOutDuration = 0.25f;
};
