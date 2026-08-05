#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"

#include <DirectXMath.h>
#include <string>
#include <vector>

class CameraComponent;

struct CameraShakePreset
{
    // ActionRegistryへ登録する名前。
    std::string actionName = "Light";

    // Shake時間と揺れの強さ。
    float duration = 0.2f;
    float positionStrength = 0.08f;
    float rotationStrength = 0.8f;
    float frequency = 25.0f;
    float fadeInRate = 0.0f;
    float fadeOutRate = 0.7f;

    // 高PriorityのShake中は低PriorityのShakeで上書きしない。
    int priority = 0;
    // Pause中も演出を進める想定の設定値。
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

    // Preset IndexからShakeを再生する。
    void PlayShakeByIndex(int index_);
    // Action名からPresetを探してShakeを再生する。
    void PlayShakeByName(const std::string& actionName_);
    // Presetを使わず、Runtime値でShakeを再生する。
    void PlayShake(
        float duration_,
        float positionStrength_,
        float rotationStrength_,
        float frequency_,
        int priority_);
    // Shakeを停止する。immediate=falseならFadeOut位置まで進める。
    void StopShake(bool immediate_);

private:
    // 同じGameObject上のCameraComponentを取得する。
    void ResolveCamera();
    // Action名に一致するPreset Indexを探す。
    int FindPresetIndex(const std::string& actionName_) const;
    // FadeIn/FadeOutを考慮した現在Shake倍率を返す。
    float CalculateEnvelope() const;
    // 時間から疑似Noiseの3軸Offsetを作る。
    DirectX::XMFLOAT3 CalculateOffset(float time_) const;
    // Cameraの注視点をYaw/Pitch方向に少し回転させる。
    DirectX::XMFLOAT3 RotateFocus(
        const DirectX::XMFLOAT3& eye_,
        const DirectX::XMFLOAT3& focus_,
        float yawDegrees_,
        float pitchDegrees_) const;

private:
    // Shakeを適用するCameraComponent。
    std::weak_ptr<CameraComponent> camera;
    // Editorで管理するShake Preset一覧。
    std::vector<CameraShakePreset> presets;
    // 現在再生中のPreset。
    CameraShakePreset activePreset;
    float elapsed = 0.0f;
    int activePriority = -1;
    bool playing = false;
};
