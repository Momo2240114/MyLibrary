#include "PrecompiledHeader.h"

#include "ColorFilterDirector.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/System/TimeManager.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"

#include <algorithm>
#include <imgui.h>

void ColorFilterDirector::Start()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;
    const std::shared_ptr<EasingComponent> easing_ = owner_->GetOrAddComponent<EasingComponent>();
    easing_->SetUseUnscaledTime(true);
    easingIndex = easing_->AddEasingAndGetIndex();
    ResetToDefault();
}

void ColorFilterDirector::Update(float elapsedTime_)
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    const std::shared_ptr<EasingComponent> easing_ = owner_
        ? owner_->GetComponent<EasingComponent>() : nullptr;
    if (!easing_ || phase == PHASE::IDLE) return;

    if (phase == PHASE::HOLD)
    {
        const std::shared_ptr<TimeManager> timeManager_ = owner_->GetManager().GetTimeManager();
        holdElapsed += timeManager_ ? timeManager_->GetLastRawDeltaTime() : elapsedTime_;
        if (holdElapsed >= holdDuration) BeginTransitionToDefault();
        return;
    }

    ApplyInterpolated(easing_->GetValue(easingIndex));
    if (!easing_->GetEndFlag(easingIndex)) return;
    if (phase == PHASE::TO_TARGET)
    {
        phase = PHASE::HOLD;
        holdElapsed = 0.0f;
    }
    else
    {
        ResetToDefault();
    }
}

void ColorFilterDirector::PlayFilter(const ColorFilterWayPoint& wayPoint_)
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    const std::shared_ptr<EasingComponent> easing_ = owner_
        ? owner_->GetOrAddComponent<EasingComponent>() : nullptr;
    if (!easing_) return;

    startParams = GraphicsDeviceManager::Instance().GetFilterParams();
    targetParams = wayPoint_.filter;
    holdDuration = (std::max)(wayPoint_.holdDuration, 0.0f);
    returnDuration = (std::max)(wayPoint_.returnDuration, 0.0f);
    activeEasing = wayPoint_.easing == EasingType::None ? EasingType::Lerp : wayPoint_.easing;
    easing_->SetStartValue(easingIndex, 0.0f);
    easing_->PlayEasing(easingIndex, activeEasing,
        1.0f / (std::max)(wayPoint_.duration, 0.0001f), 1.0f);
    phase = PHASE::TO_TARGET;
}

void ColorFilterDirector::ApplyInterpolated(float ratio_)
{
    GraphicsDeviceManager::Instance().GetFilterParams() =
        LerpFilter(startParams, targetParams, ratio_);
}

void ColorFilterDirector::BeginTransitionToDefault()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    const std::shared_ptr<EasingComponent> easing_ = owner_
        ? owner_->GetComponent<EasingComponent>() : nullptr;
    if (!easing_)
    {
        ResetToDefault();
        return;
    }
    startParams = GraphicsDeviceManager::Instance().GetFilterParams();
    targetParams = defaultParams;
    easing_->SetStartValue(easingIndex, 0.0f);
    easing_->PlayEasing(easingIndex, activeEasing,
        1.0f / (std::max)(returnDuration, 0.0001f), 1.0f);
    phase = PHASE::TO_DEFAULT;
}

void ColorFilterDirector::ResetToDefault()
{
    GraphicsDeviceManager::Instance().GetFilterParams() = defaultParams;
    phase = PHASE::IDLE;
    holdElapsed = 0.0f;
}

BloomSystem::CbColorFilter ColorFilterDirector::LerpFilter(
    const BloomSystem::CbColorFilter& from_,
    const BloomSystem::CbColorFilter& to_,
    float ratio_)
{
    const float ratioClamped_ = std::clamp(ratio_, 0.0f, 1.0f);
    const auto lerp_ = [ratioClamped_](float fromValue_, float toValue_)
    {
        return fromValue_ + (toValue_ - fromValue_) * ratioClamped_;
    };
    BloomSystem::CbColorFilter result_{};
    result_.hueShift = lerp_(from_.hueShift, to_.hueShift);
    result_.saturation = lerp_(from_.saturation, to_.saturation);
    result_.brightness = lerp_(from_.brightness, to_.brightness);
    result_.contrast = lerp_(from_.contrast, to_.contrast);
    result_.colorBalance = { lerp_(from_.colorBalance.x, to_.colorBalance.x),
        lerp_(from_.colorBalance.y, to_.colorBalance.y),
        lerp_(from_.colorBalance.z, to_.colorBalance.z) };
    result_.filterIntensity = lerp_(from_.filterIntensity, to_.filterIntensity);
    return result_;
}

void ColorFilterDirector::Gui()
{
    ImGui::SliderFloat("Default Hue", &defaultParams.hueShift, -180.0f, 180.0f);
    ImGui::SliderFloat("Default Saturation", &defaultParams.saturation, 0.0f, 2.0f);
    ImGui::SliderFloat("Default Brightness", &defaultParams.brightness, 0.0f, 2.0f);
    ImGui::SliderFloat("Default Contrast", &defaultParams.contrast, 0.0f, 2.0f);
    ImGui::ColorEdit3("Default Balance", &defaultParams.colorBalance.x);
    ImGui::SliderFloat("Default Intensity", &defaultParams.filterIntensity, 0.0f, 1.0f);
    if (ImGui::Button("Capture Color Filter"))
    {
        ColorFilterWayPoint wayPoint_;
        wayPoint_.filter = GraphicsDeviceManager::Instance().GetFilterParams();
        editorWaypoints.push_back(wayPoint_);
        RegisterActions();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Color Filter")) ResetToDefault();

    for (int index_ = 0; index_ < static_cast<int>(editorWaypoints.size()); ++index_)
    {
        ImGui::PushID(index_);
        ColorFilterWayPoint& wayPoint_ = editorWaypoints[index_];
        if (ImGui::TreeNode(("Color Filter " + std::to_string(index_)).c_str()))
        {
            ImGui::SliderFloat("Hue", &wayPoint_.filter.hueShift, -180.0f, 180.0f);
            ImGui::SliderFloat("Saturation", &wayPoint_.filter.saturation, 0.0f, 2.0f);
            ImGui::SliderFloat("Brightness", &wayPoint_.filter.brightness, 0.0f, 2.0f);
            ImGui::SliderFloat("Contrast", &wayPoint_.filter.contrast, 0.0f, 2.0f);
            ImGui::ColorEdit3("Balance", &wayPoint_.filter.colorBalance.x);
            ImGui::SliderFloat("Intensity", &wayPoint_.filter.filterIntensity, 0.0f, 1.0f);
            ImGui::DragFloat("Time To Target", &wayPoint_.duration, 0.01f, 0.0f, 60.0f);
            ImGui::DragFloat("Hold", &wayPoint_.holdDuration, 0.01f, 0.0f, 60.0f);
            ImGui::DragFloat("Time To Default", &wayPoint_.returnDuration, 0.01f, 0.0f, 60.0f);
            if (ImGui::Button("Play")) PlayFilter(wayPoint_);
            ImGui::SameLine();
            if (ImGui::Button("Remove"))
            {
                editorWaypoints.erase(editorWaypoints.begin() + index_--);
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void ColorFilterDirector::RegisterActions()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;
    for (size_t index_ = 0; index_ < editorWaypoints.size(); ++index_)
    {
        owner_->RegisterGlobalAction("PostProcess", "Color_" + std::to_string(index_), [this, index_]()
        {
            if (index_ < editorWaypoints.size()) PlayFilter(editorWaypoints[index_]);
        });
    }
}

void ColorFilterDirector::Serialize(nlohmann::json& json_) const
{
    const auto write_ = [](const BloomSystem::CbColorFilter& value_)
    {
        return nlohmann::json{ { "hue", value_.hueShift }, { "sat", value_.saturation },
            { "bri", value_.brightness }, { "con", value_.contrast },
            { "balance", { value_.colorBalance.x, value_.colorBalance.y, value_.colorBalance.z } },
            { "intensity", value_.filterIntensity } };
    };
    json_["DefaultFilter"] = write_(defaultParams);
    json_["ColorFilterWaypoints"] = nlohmann::json::array();
    for (const ColorFilterWayPoint& wayPoint_ : editorWaypoints)
    {
        nlohmann::json item_ = write_(wayPoint_.filter);
        item_["dur"] = wayPoint_.duration;
        item_["holdDur"] = wayPoint_.holdDuration;
        item_["returnDur"] = wayPoint_.returnDuration;
        item_["ease"] = static_cast<int>(wayPoint_.easing);
        json_["ColorFilterWaypoints"].push_back(item_);
    }
}

void ColorFilterDirector::Deserialize(const nlohmann::json& json_)
{
    const auto read_ = [](const nlohmann::json& input_, BloomSystem::CbColorFilter& value_)
    {
        value_.hueShift = input_.value("hue", value_.hueShift);
        value_.saturation = input_.value("sat", value_.saturation);
        value_.brightness = input_.value("bri", value_.brightness);
        value_.contrast = input_.value("con", value_.contrast);
        value_.filterIntensity = input_.value("intensity", value_.filterIntensity);
        if (input_.contains("balance") && input_["balance"].size() >= 3)
        {
            value_.colorBalance = { input_["balance"][0], input_["balance"][1], input_["balance"][2] };
        }
    };
    if (json_.contains("DefaultFilter")) read_(json_["DefaultFilter"], defaultParams);
    editorWaypoints.clear();
    if (json_.contains("ColorFilterWaypoints") && json_["ColorFilterWaypoints"].is_array())
    {
        for (const nlohmann::json& item_ : json_["ColorFilterWaypoints"])
        {
            ColorFilterWayPoint wayPoint_;
            read_(item_, wayPoint_.filter);
            wayPoint_.duration = item_.value("dur", 1.0f);
            wayPoint_.holdDuration = item_.value("holdDur", 0.0f);
            wayPoint_.returnDuration = item_.value("returnDur", 1.0f);
            wayPoint_.easing = static_cast<EasingType>(item_.value("ease", static_cast<int>(EasingType::Lerp)));
            editorWaypoints.push_back(wayPoint_);
        }
    }
    ResetToDefault();
}
