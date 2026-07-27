#include "PrecompiledHeader.h"

#include "CameraShakeComponent.h"

#include "CameraComponent.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
    DirectX::XMFLOAT3 AddFloat3(
        const DirectX::XMFLOAT3& left_,
        const DirectX::XMFLOAT3& right_)
    {
        return { left_.x + right_.x, left_.y + right_.y, left_.z + right_.z };
    }
}

void CameraShakeComponent::Start()
{
    BS_PROFILE_SCOPE("CameraShakeComponent::Start");
    ResolveCamera();
    if (presets.empty())
    {
        CameraShakePreset light_;
        light_.actionName = "Light";
        light_.positionStrength = 0.035f;
        light_.rotationStrength = 0.25f;
        light_.priority = 0;
        presets.push_back(light_);

        CameraShakePreset heavy_;
        heavy_.actionName = "Heavy";
        heavy_.duration = 0.28f;
        heavy_.positionStrength = 0.12f;
        heavy_.rotationStrength = 1.2f;
        heavy_.frequency = 22.0f;
        heavy_.priority = 10;
        presets.push_back(heavy_);

        CameraShakePreset justGuard_;
        justGuard_.actionName = "JustGuard";
        justGuard_.duration = 0.18f;
        justGuard_.positionStrength = 0.07f;
        justGuard_.rotationStrength = 0.7f;
        justGuard_.frequency = 30.0f;
        justGuard_.priority = 20;
        presets.push_back(justGuard_);
    }
    RegisterActions();
}

void CameraShakeComponent::LateUpdate(float deltaTime_)
{
    BS_PROFILE_SCOPE("CameraShakeComponent::LateUpdate");
    ResolveCamera();
    if (!playing) return;

    const std::shared_ptr<CameraComponent> camera_ = camera.lock();
    if (!camera_)
    {
        StopShake(true);
        return;
    }

    elapsed += deltaTime_;
    const float envelope_ = CalculateEnvelope();
    const DirectX::XMFLOAT3 offset_ =
        CalculateOffset(elapsed * activePreset.frequency);

    const DirectX::XMFLOAT3 cameraPosition_ = AddFloat3(
        camera_->GetEye(),
        {
            offset_.x * activePreset.positionStrength * envelope_,
            offset_.y * activePreset.positionStrength * envelope_,
            offset_.z * activePreset.positionStrength * envelope_
        });

    const DirectX::XMFLOAT3 focusPosition_ = RotateFocus(
        cameraPosition_,
        camera_->GetFocus(),
        offset_.x * activePreset.rotationStrength * envelope_,
        offset_.y * activePreset.rotationStrength * envelope_);

    camera_->SetLookAt(
        cameraPosition_,
        focusPosition_,
        { 0.0f, 1.0f, 0.0f });

    if (elapsed >= activePreset.duration)
    {
        StopShake(true);
    }
}

void CameraShakeComponent::PlayShakeByIndex(int index_)
{
    BS_PROFILE_SCOPE("CameraShakeComponent::PlayShakeByIndex");
    if (index_ < 0 || index_ >= static_cast<int>(presets.size())) return;
    if (playing && presets[index_].priority < activePriority) return;

    activePreset = presets[index_];
    activePriority = activePreset.priority;
    elapsed = 0.0f;
    playing = activePreset.duration > 0.0001f;
}

void CameraShakeComponent::PlayShakeByName(const std::string& actionName_)
{
    PlayShakeByIndex(FindPresetIndex(actionName_));
}

void CameraShakeComponent::PlayShake(
    float duration_,
    float positionStrength_,
    float rotationStrength_,
    float frequency_,
    int priority_)
{
    BS_PROFILE_SCOPE("CameraShakeComponent::PlayShake");
    if (playing && priority_ < activePriority) return;

    CameraShakePreset preset_;
    preset_.actionName = "Runtime";
    preset_.duration = (std::max)(duration_, 0.0f);
    preset_.positionStrength = (std::max)(positionStrength_, 0.0f);
    preset_.rotationStrength = (std::max)(rotationStrength_, 0.0f);
    preset_.frequency = (std::max)(frequency_, 0.0f);
    preset_.priority = priority_;

    activePreset = preset_;
    activePriority = activePreset.priority;
    elapsed = 0.0f;
    playing = activePreset.duration > 0.0001f &&
        (activePreset.positionStrength > 0.0001f ||
            activePreset.rotationStrength > 0.0001f);
}

void CameraShakeComponent::StopShake(bool immediate_)
{
    if (!immediate_ && playing)
    {
        elapsed = (std::max)(elapsed, activePreset.duration * activePreset.fadeOutRate);
        return;
    }

    playing = false;
    elapsed = 0.0f;
    activePriority = -1;
}

void CameraShakeComponent::RegisterActions()
{
    ActionRegistry& registry_ = ActionRegistry::Instance();
    for (const CameraShakePreset& preset_ : presets)
    {
        registry_.RemoveAction("CameraShake", preset_.actionName);
        registry_.RegisterAction("CameraShake", preset_.actionName, [this, actionName_ = preset_.actionName]()
        {
            PlayShakeByName(actionName_);
        });
    }
}

void CameraShakeComponent::ResolveCamera()
{
    if (!camera.expired()) return;
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (owner_) camera = owner_->GetComponent<CameraComponent>();
}

int CameraShakeComponent::FindPresetIndex(const std::string& actionName_) const
{
    for (int index_ = 0; index_ < static_cast<int>(presets.size()); ++index_)
    {
        if (presets[index_].actionName == actionName_) return index_;
    }
    return -1;
}

float CameraShakeComponent::CalculateEnvelope() const
{
    if (activePreset.duration <= 0.0001f) return 0.0f;

    const float rate_ = std::clamp(elapsed / activePreset.duration, 0.0f, 1.0f);
    float envelope_ = 1.0f;
    if (activePreset.fadeInRate > 0.0001f &&
        rate_ < activePreset.fadeInRate)
    {
        envelope_ *= rate_ / activePreset.fadeInRate;
    }
    if (activePreset.fadeOutRate < 0.9999f &&
        rate_ > activePreset.fadeOutRate)
    {
        envelope_ *= 1.0f -
            (rate_ - activePreset.fadeOutRate) /
            (1.0f - activePreset.fadeOutRate);
    }
    return std::clamp(envelope_, 0.0f, 1.0f);
}

DirectX::XMFLOAT3 CameraShakeComponent::CalculateOffset(float time_) const
{
    return
    {
        std::sin(time_ * 6.2831853f),
        std::sin((time_ * 1.71f + 0.35f) * 6.2831853f),
        std::sin((time_ * 2.13f + 0.62f) * 6.2831853f)
    };
}

DirectX::XMFLOAT3 CameraShakeComponent::RotateFocus(
    const DirectX::XMFLOAT3& eye_,
    const DirectX::XMFLOAT3& focus_,
    float yawDegrees_,
    float pitchDegrees_) const
{
    DirectX::XMVECTOR direction_ = DirectX::XMVectorSubtract(
        DirectX::XMLoadFloat3(&focus_),
        DirectX::XMLoadFloat3(&eye_));
    const DirectX::XMMATRIX rotation_ = DirectX::XMMatrixRotationRollPitchYaw(
        DirectX::XMConvertToRadians(pitchDegrees_),
        DirectX::XMConvertToRadians(yawDegrees_),
        0.0f);
    direction_ = DirectX::XMVector3TransformNormal(direction_, rotation_);
    DirectX::XMFLOAT3 result_{};
    DirectX::XMStoreFloat3(
        &result_,
        DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&eye_), direction_));
    return result_;
}

void CameraShakeComponent::Gui()
{
    ImGui::Text(playing ? "Playing" : "Idle");
    ImGui::Text("Elapsed: %.3f", elapsed);
    if (ImGui::Button("Add Shake"))
    {
        CameraShakePreset preset_;
        preset_.actionName = "NewShake";
        presets.push_back(preset_);
        RegisterActions();
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop"))
    {
        StopShake(false);
    }

    for (int index_ = 0; index_ < static_cast<int>(presets.size()); ++index_)
    {
        CameraShakePreset& preset_ = presets[index_];
        ImGui::PushID(index_);
        if (ImGui::TreeNode(preset_.actionName.c_str()))
        {
            char nameBuffer_[128]{};
            strcpy_s(nameBuffer_, preset_.actionName.c_str());
            if (ImGui::InputText("Action Name", nameBuffer_, sizeof(nameBuffer_)))
            {
                preset_.actionName = nameBuffer_;
                RegisterActions();
            }
            ImGui::DragFloat("Duration", &preset_.duration, 0.01f, 0.0f, 5.0f);
            ImGui::DragFloat("Position Strength", &preset_.positionStrength, 0.005f, 0.0f, 2.0f);
            ImGui::DragFloat("Rotation Strength", &preset_.rotationStrength, 0.01f, 0.0f, 20.0f);
            ImGui::DragFloat("Frequency", &preset_.frequency, 0.1f, 0.0f, 120.0f);
            ImGui::DragFloat("Fade In Rate", &preset_.fadeInRate, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Fade Out Rate", &preset_.fadeOutRate, 0.01f, 0.0f, 1.0f);
            ImGui::DragInt("Priority", &preset_.priority);
            ImGui::Checkbox("Use Unscaled Time", &preset_.useUnscaledTime);
            if (ImGui::Button("Play Preview"))
            {
                PlayShakeByIndex(index_);
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete") && presets.size() > 1)
            {
                const std::string actionName_ = preset_.actionName;
                presets.erase(presets.begin() + index_);
                ActionRegistry::Instance().RemoveAction("CameraShake", actionName_);
                ImGui::TreePop();
                ImGui::PopID();
                break;
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void CameraShakeComponent::Serialize(nlohmann::json& json_) const
{
    json_["presets"] = nlohmann::json::array();
    for (const CameraShakePreset& preset_ : presets)
    {
        nlohmann::json presetJson_;
        presetJson_["actionName"] = preset_.actionName;
        presetJson_["duration"] = preset_.duration;
        presetJson_["positionStrength"] = preset_.positionStrength;
        presetJson_["rotationStrength"] = preset_.rotationStrength;
        presetJson_["frequency"] = preset_.frequency;
        presetJson_["fadeInRate"] = preset_.fadeInRate;
        presetJson_["fadeOutRate"] = preset_.fadeOutRate;
        presetJson_["priority"] = preset_.priority;
        presetJson_["useUnscaledTime"] = preset_.useUnscaledTime;
        json_["presets"].push_back(presetJson_);
    }
}

void CameraShakeComponent::Deserialize(const nlohmann::json& json_)
{
    presets.clear();
    if (!json_.contains("presets") || !json_["presets"].is_array()) return;

    for (const nlohmann::json& presetJson_ : json_["presets"])
    {
        CameraShakePreset preset_;
        preset_.actionName =
            presetJson_.value("actionName", std::string{ "CameraShake" });
        preset_.duration = presetJson_.value("duration", 0.2f);
        preset_.positionStrength = presetJson_.value("positionStrength", 0.08f);
        preset_.rotationStrength = presetJson_.value("rotationStrength", 0.8f);
        preset_.frequency = presetJson_.value("frequency", 25.0f);
        preset_.fadeInRate = presetJson_.value("fadeInRate", 0.0f);
        preset_.fadeOutRate = presetJson_.value("fadeOutRate", 0.7f);
        preset_.priority = presetJson_.value("priority", 0);
        preset_.useUnscaledTime = presetJson_.value("useUnscaledTime", true);
        presets.push_back(preset_);
    }
}

REGISTER_COMPONENT(CameraShakeComponent, "CameraShakeComponent")
