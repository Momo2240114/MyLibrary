#include "PrecompiledHeader.h"

#include "VignetteDirector.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/System/TimeManager.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"

#include <algorithm>
#include <imgui.h>

void VignetteDirector::Update(float elapsedTime_)
{
    // 演出中でない場合はGraphicsDeviceManagerの設定を触らない。
    if (phase == PHASE::IDLE)
    {
        return;
    }
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    const std::shared_ptr<TimeManager> timeManager_ = owner_ ? owner_->GetManager().GetTimeManager() : nullptr;
    // PostEffect演出はPause中にも完了してほしいため、RawDeltaを優先して進める。
    elapsed += timeManager_ ? timeManager_->GetLastRawDeltaTime() : elapsedTime_;
    if (phase == PHASE::HOLD)
    {
        if (elapsed < holdDuration)
        {
            return;
        }
        // Holdが終わったら現在値を始点にしてDefaultへ戻る補間へ切り替える。
        startParams = GraphicsDeviceManager::Instance().GetVignetteParams();
        targetParams = defaultParams;
        duration = returnDuration;
        elapsed = 0.0f;
        phase = PHASE::TO_DEFAULT;
    }
    const float ratio_ = duration > 0.0f ? std::clamp(elapsed / duration, 0.0f, 1.0f) : 1.0f;
    // 補間結果をGraphicsDeviceManagerへ反映し、RenderSystem側が次の描画で使う。
    GraphicsDeviceManager::Instance().GetVignetteParams() = Lerp(startParams, targetParams, ratio_);
    if (ratio_ < 1.0f)
    {
        return;
    }
    if (phase == PHASE::TO_TARGET)
    {
        phase = PHASE::HOLD;
        elapsed = 0.0f;
    }
    else
    {
        ResetToDefault();
    }
}

void VignetteDirector::Play(const VignetteWayPoint& wayPoint_)
{
    // 現在のVignette値を始点にすることで、演出の重ね掛け時も自然につなげる。
    startParams = GraphicsDeviceManager::Instance().GetVignetteParams();
    targetParams = wayPoint_.target;
    duration = (std::max)(wayPoint_.timeToTarget, 0.0f);
    holdDuration = (std::max)(wayPoint_.holdDuration, 0.0f);
    returnDuration = (std::max)(wayPoint_.timeToDefault, 0.0f);
    elapsed = 0.0f;
    phase = PHASE::TO_TARGET;
}

VignetteSystem::Parameters VignetteDirector::Lerp(
    const VignetteSystem::Parameters& from_, const VignetteSystem::Parameters& to_, float ratio_)
{
    // Scalar/Color/Centerは線形補間し、enabledは補間中に片方でもtrueなら有効として扱う。
    const auto value_ = [ratio_](float fromValue_, float toValue_)
    {
        return fromValue_ + (toValue_ - fromValue_) * ratio_;
    };
    VignetteSystem::Parameters result_ = to_;
    result_.enabled = ratio_ < 1.0f ? from_.enabled || to_.enabled : to_.enabled;
    result_.color = { value_(from_.color.x, to_.color.x), value_(from_.color.y, to_.color.y),
        value_(from_.color.z, to_.color.z), value_(from_.color.w, to_.color.w) };
    result_.center = { value_(from_.center.x, to_.center.x), value_(from_.center.y, to_.center.y) };
    result_.intensity = value_(from_.intensity, to_.intensity);
    result_.smoothness = value_(from_.smoothness, to_.smoothness);
    result_.rounded = value_(from_.rounded, to_.rounded);
    result_.roundness = value_(from_.roundness, to_.roundness);
    result_.opacity = value_(from_.opacity, to_.opacity);
    return result_;
}

void VignetteDirector::ResetToDefault()
{
    // 演出終了後はDefault値を明示的に戻し、次回再生時の基準を安定させる。
    GraphicsDeviceManager::Instance().GetVignetteParams() = defaultParams;
    phase = PHASE::IDLE;
    elapsed = 0.0f;
}

void VignetteDirector::Gui()
{
    // Default値は演出終了後に戻る基準としてEditorで調整する。
    ImGui::Checkbox("Default Enabled", &defaultParams.enabled);
    ImGui::ColorEdit4("Default Color", &defaultParams.color.x);
    ImGui::DragFloat("Default Intensity", &defaultParams.intensity, 0.01f, 0.0f, 4.0f);
    if (ImGui::Button("Capture Vignette"))
    {
        // 現在のRenderSystem設定をWaypointとして保存し、Actionから再生できるようにする。
        VignetteWayPoint wayPoint_;
        wayPoint_.target = GraphicsDeviceManager::Instance().GetVignetteParams();
        editorWaypoints.push_back(wayPoint_);
        RegisterActions();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Vignette"))
    {
        ResetToDefault();
    }
    for (int index_ = 0; index_ < static_cast<int>(editorWaypoints.size()); ++index_)
    {
        ImGui::PushID(index_);
        VignetteWayPoint& wayPoint_ = editorWaypoints[index_];
        if (ImGui::TreeNode(("Vignette " + std::to_string(index_)).c_str()))
        {
            // Waypointごとに目標値と遷移時間を調整する。
            ImGui::Checkbox("Enabled", &wayPoint_.target.enabled);
            ImGui::ColorEdit4("Color", &wayPoint_.target.color.x);
            ImGui::DragFloat2("Center", &wayPoint_.target.center.x, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Intensity", &wayPoint_.target.intensity, 0.01f, 0.0f, 4.0f);
            ImGui::DragFloat("Smoothness", &wayPoint_.target.smoothness, 0.01f, 0.01f, 4.0f);
            ImGui::SliderFloat("Opacity", &wayPoint_.target.opacity, 0.0f, 1.0f);
            ImGui::DragFloat("Time To Target", &wayPoint_.timeToTarget, 0.01f, 0.0f, 60.0f);
            ImGui::DragFloat("Hold", &wayPoint_.holdDuration, 0.01f, 0.0f, 60.0f);
            ImGui::DragFloat("Time To Default", &wayPoint_.timeToDefault, 0.01f, 0.0f, 60.0f);
            if (ImGui::Button("Play"))
            {
                Play(wayPoint_);
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void VignetteDirector::RegisterActions()
{
    // PostProcessカテゴリにVignette_0, Vignette_1...として登録する。
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }
    for (size_t index_ = 0; index_ < editorWaypoints.size(); ++index_)
    {
        owner_->RegisterGlobalAction("PostProcess", "Vignette_" + std::to_string(index_), [this, index_]()
        {
            if (index_ < editorWaypoints.size())
            {
                Play(editorWaypoints[index_]);
            }
        });
    }
}

void VignetteDirector::Serialize(nlohmann::json& json_) const
{
    // Parameters構造をJSONへ展開する小さな変換関数。
    const auto write_ = [](const VignetteSystem::Parameters& value_)
    {
        return nlohmann::json{ { "enabled", value_.enabled },
        { "color", { value_.color.x, value_.color.y, value_.color.z, value_.color.w } },
        { "center", { value_.center.x, value_.center.y } }, { "intensity", value_.intensity },
        { "smoothness", value_.smoothness }, { "rounded", value_.rounded },
        { "roundness", value_.roundness }, { "opacity", value_.opacity } };
    };
    json_["Default"] = write_(defaultParams);
    json_["Waypoints"] = nlohmann::json::array();
    for (const VignetteWayPoint& wayPoint_ : editorWaypoints)
    {
        json_["Waypoints"].push_back({ { "target", write_(wayPoint_.target) },
            { "toTarget", wayPoint_.timeToTarget }, { "hold", wayPoint_.holdDuration },
            { "toDefault", wayPoint_.timeToDefault } });
    }
}

void VignetteDirector::Deserialize(const nlohmann::json& json_)
{
    // 保存時と同じKeyからParametersを復元する。
    const auto read_ = [](const nlohmann::json& input_, VignetteSystem::Parameters& value_)
    {
        value_.enabled = input_.value("enabled", value_.enabled);
        value_.intensity = input_.value("intensity", value_.intensity);
        value_.smoothness = input_.value("smoothness", value_.smoothness);
        value_.rounded = input_.value("rounded", value_.rounded);
        value_.roundness = input_.value("roundness", value_.roundness);
        value_.opacity = input_.value("opacity", value_.opacity);
        if (input_.contains("color"))
        {
            value_.color = { input_["color"][0], input_["color"][1], input_["color"][2], input_["color"][3] };
        }
        if (input_.contains("center"))
        {
            value_.center = { input_["center"][0], input_["center"][1] };
        }
    };
    if (json_.contains("Default"))
    {
        read_(json_["Default"], defaultParams);
    }
    editorWaypoints.clear();
    if (json_.contains("Waypoints"))
    {
        for (const nlohmann::json& item_ : json_["Waypoints"])
    {
        // Waypointが欠けている古いデータでも既定値で補完する。
        VignetteWayPoint wayPoint_;
        if (item_.contains("target"))
            {
                read_(item_["target"], wayPoint_.target);
            }
            wayPoint_.timeToTarget = item_.value("toTarget", 0.1f);
        wayPoint_.holdDuration = item_.value("hold", 0.0f);
        wayPoint_.timeToDefault = item_.value("toDefault", 0.1f);
        editorWaypoints.push_back(wayPoint_);
    }
    }
    ResetToDefault();
}
