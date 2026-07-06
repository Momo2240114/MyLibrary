#include "RadialBlurDirector.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/System/TimeManager.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"

#include <algorithm>
#include <imgui.h>

void RadialBlurDirector::Update(float elapsedTime_)
{
    if (phase == PHASE::IDLE) return;
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    const std::shared_ptr<TimeManager> timeManager_ = owner_
        ? owner_->GetManager().GetTimeManager() : nullptr;
    elapsed += timeManager_ ? timeManager_->GetLastRawDeltaTime() : elapsedTime_;
    if (phase == PHASE::HOLD)
    {
        if (elapsed < holdDuration) return;
        startParams = GraphicsDeviceManager::Instance().GetRadialBlurParams();
        targetParams = defaultParams;
        duration = returnDuration;
        elapsed = 0.0f;
        phase = PHASE::TO_DEFAULT;
    }
    const float ratio_ = duration > 0.0f ? std::clamp(elapsed / duration, 0.0f, 1.0f) : 1.0f;
    GraphicsDeviceManager::Instance().GetRadialBlurParams() = Lerp(startParams, targetParams, ratio_);
    if (ratio_ < 1.0f) return;
    if (phase == PHASE::TO_TARGET)
    {
        phase = PHASE::HOLD;
        elapsed = 0.0f;
    }
    else ResetToDefault();
}

void RadialBlurDirector::Play(const RadialBlurWayPoint& wayPoint_)
{
    startParams = GraphicsDeviceManager::Instance().GetRadialBlurParams();
    targetParams = wayPoint_.target;
    duration = (std::max)(wayPoint_.timeToTarget, 0.0f);
    holdDuration = (std::max)(wayPoint_.holdDuration, 0.0f);
    returnDuration = (std::max)(wayPoint_.timeToDefault, 0.0f);
    elapsed = 0.0f;
    phase = PHASE::TO_TARGET;
}

RadialBlurSystem::Parameters RadialBlurDirector::Lerp(
    const RadialBlurSystem::Parameters& from_,
    const RadialBlurSystem::Parameters& to_, float ratio_)
{
    const auto value_ = [ratio_](float fromValue_, float toValue_)
    { return fromValue_ + (toValue_ - fromValue_) * ratio_; };
    RadialBlurSystem::Parameters result_;
    result_.blurRadius = value_(from_.blurRadius, to_.blurRadius);
    result_.blurSamplingCount = ratio_ < 1.0f ? from_.blurSamplingCount : to_.blurSamplingCount;
    result_.blurCenter = { value_(from_.blurCenter.x, to_.blurCenter.x),
        value_(from_.blurCenter.y, to_.blurCenter.y) };
    result_.blurMaskRadius = value_(from_.blurMaskRadius, to_.blurMaskRadius);
    return result_;
}

void RadialBlurDirector::ResetToDefault()
{
    GraphicsDeviceManager::Instance().GetRadialBlurParams() = defaultParams;
    phase = PHASE::IDLE;
    elapsed = 0.0f;
}

void RadialBlurDirector::Gui()
{
    ImGui::DragFloat("Default Radius", &defaultParams.blurRadius, 0.001f, 0.0f, 0.5f);
    if (ImGui::Button("Capture Radial Blur"))
    {
        RadialBlurWayPoint wayPoint_;
        wayPoint_.target = GraphicsDeviceManager::Instance().GetRadialBlurParams();
        editorWaypoints.push_back(wayPoint_);
        RegisterActions();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Radial Blur")) ResetToDefault();
    for (int index_ = 0; index_ < static_cast<int>(editorWaypoints.size()); ++index_)
    {
        ImGui::PushID(index_);
        RadialBlurWayPoint& wayPoint_ = editorWaypoints[index_];
        if (ImGui::TreeNode(("Radial Blur " + std::to_string(index_)).c_str()))
        {
            ImGui::DragFloat("Radius", &wayPoint_.target.blurRadius, 0.001f, 0.0f, 0.5f);
            ImGui::SliderInt("Samples", &wayPoint_.target.blurSamplingCount, 4, 32);
            ImGui::DragFloat2("Center", &wayPoint_.target.blurCenter.x, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Mask Radius", &wayPoint_.target.blurMaskRadius, 0.01f, 0.0f, 2.0f);
            ImGui::DragFloat("Time To Target", &wayPoint_.timeToTarget, 0.01f, 0.0f, 60.0f);
            ImGui::DragFloat("Hold", &wayPoint_.holdDuration, 0.01f, 0.0f, 60.0f);
            ImGui::DragFloat("Time To Default", &wayPoint_.timeToDefault, 0.01f, 0.0f, 60.0f);
            if (ImGui::Button("Play")) Play(wayPoint_);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void RadialBlurDirector::RegisterActions()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;
    for (size_t index_ = 0; index_ < editorWaypoints.size(); ++index_)
    {
        owner_->RegisterGlobalAction("PostProcess", "RadialBlur_" + std::to_string(index_), [this, index_]()
        { if (index_ < editorWaypoints.size()) Play(editorWaypoints[index_]); });
    }
}

void RadialBlurDirector::Serialize(nlohmann::json& json_) const
{
    const auto write_ = [](const RadialBlurSystem::Parameters& value_)
    { return nlohmann::json{ { "radius", value_.blurRadius }, { "samples", value_.blurSamplingCount },
        { "center", { value_.blurCenter.x, value_.blurCenter.y } }, { "maskRadius", value_.blurMaskRadius } }; };
    json_["Default"] = write_(defaultParams);
    json_["Waypoints"] = nlohmann::json::array();
    for (const RadialBlurWayPoint& wayPoint_ : editorWaypoints)
    {
        json_["Waypoints"].push_back({ { "target", write_(wayPoint_.target) },
            { "toTarget", wayPoint_.timeToTarget }, { "hold", wayPoint_.holdDuration },
            { "toDefault", wayPoint_.timeToDefault } });
    }
}

void RadialBlurDirector::Deserialize(const nlohmann::json& json_)
{
    const auto read_ = [](const nlohmann::json& input_, RadialBlurSystem::Parameters& value_)
    {
        value_.blurRadius = input_.value("radius", value_.blurRadius);
        value_.blurSamplingCount = input_.value("samples", value_.blurSamplingCount);
        value_.blurMaskRadius = input_.value("maskRadius", value_.blurMaskRadius);
        if (input_.contains("center")) value_.blurCenter = { input_["center"][0], input_["center"][1] };
    };
    if (json_.contains("Default")) read_(json_["Default"], defaultParams);
    editorWaypoints.clear();
    if (json_.contains("Waypoints")) for (const nlohmann::json& item_ : json_["Waypoints"])
    {
        RadialBlurWayPoint wayPoint_;
        if (item_.contains("target")) read_(item_["target"], wayPoint_.target);
        wayPoint_.timeToTarget = item_.value("toTarget", 0.1f);
        wayPoint_.holdDuration = item_.value("hold", 0.0f);
        wayPoint_.timeToDefault = item_.value("toDefault", 0.1f);
        editorWaypoints.push_back(wayPoint_);
    }
    ResetToDefault();
}
