#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"

#include <DirectXMath.h>
#include <string>
#include <vector>

class CameraComponent;

struct CameraShakePreset
{
    std::string actionName = "Light";

    float duration = 0.2f;
    float positionStrength = 0.08f;
    float rotationStrength = 0.8f;
    float frequency = 25.0f;
    float fadeInRate = 0.0f;
    float fadeOutRate = 0.7f;

    int priority = 0;
    bool useUnscaledTime = true;
};

class CameraShakeComponent : public Component, public IActionProvider
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "CameraShakeComponent";
        return name_;
    }

    void Start() override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    void RegisterActions() override;

    void PlayShakeByIndex(int index_);
    void PlayShakeByName(const std::string& actionName_);
    void PlayShake(
        float duration_,
        float positionStrength_,
        float rotationStrength_,
        float frequency_,
        int priority_);
    void StopShake(bool immediate_);

private:
    void ResolveCamera();
    int FindPresetIndex(const std::string& actionName_) const;
    float CalculateEnvelope() const;
    DirectX::XMFLOAT3 CalculateOffset(float time_) const;
    DirectX::XMFLOAT3 RotateFocus(
        const DirectX::XMFLOAT3& eye_,
        const DirectX::XMFLOAT3& focus_,
        float yawDegrees_,
        float pitchDegrees_) const;

private:
    std::weak_ptr<CameraComponent> camera;
    std::vector<CameraShakePreset> presets;
    CameraShakePreset activePreset;
    float elapsed = 0.0f;
    int activePriority = -1;
    bool playing = false;
};
