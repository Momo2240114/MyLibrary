#pragma once

#include "TrailMeshComponent.h"

class AfterimageTrailComponent : public TrailMeshComponent
{
public:
    AfterimageTrailComponent();

    const std::string& GetName() const override
    {
        static std::string name = "AfterimageTrailComponent";
        return name;
    }

    void Start() override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

private:
    void RegisterActions();
    bool GetTrailSegment(
        DirectX::XMFLOAT3& leftPosition_,
        DirectX::XMFLOAT3& rightPosition_) const;

    float trailWidth = 1.8f;
    float verticalOffset = 0.8f;
    float wobbleAmplitude = 0.35f;
    float wobbleSpeed = 18.0f;
};
