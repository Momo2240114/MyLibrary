#include "PrecompiledHeader.h"

#include "MotionBlurDirector.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/System/TimeManager.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"

#include <algorithm>
#include <imgui.h>

void MotionBlurDirector::Update(float elapsedTime_)
{
    // 演出中でない場合はGraphicsDeviceManagerの設定を触らない。
    if (phase == PHASE::IDLE)
    {
        return;
    }

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    const std::shared_ptr<TimeManager> timeManager_ = owner_
        ? owner_->GetManager().GetTimeManager() : nullptr;
    // MotionBlur演出はPause中でも戻し切りたいので、RawDeltaを優先して進める。
    elapsed += timeManager_ ? timeManager_->GetLastRawDeltaTime() : elapsedTime_;

    if (phase == PHASE::HOLD)
    {
        if (elapsed < holdDuration)
        {
            return;
        }
        // Holdが終わったら現在値を始点にしてDefaultへ戻る補間へ切り替える。
        startParams = GraphicsDeviceManager::Instance().GetMotionBlurParams();
        targetParams = defaultParams;
        duration = returnDuration;
        elapsed = 0.0f;
        phase = PHASE::TO_DEFAULT;
    }

    const float ratio_ = duration > 0.0f ? std::clamp(elapsed / duration, 0.0f, 1.0f) : 1.0f;
    // 補間結果をGraphicsDeviceManagerへ反映し、RenderSystem側が次の描画で使う。
    GraphicsDeviceManager::Instance().GetMotionBlurParams() = Lerp(startParams, targetParams, ratio_);
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

void MotionBlurDirector::Play(const MotionBlurWayPoint& wayPoint_)
{
    // 現在のMotionBlur値を始点にすることで、演出の途中再生でも自然につなげる。
    startParams = GraphicsDeviceManager::Instance().GetMotionBlurParams();
    targetParams = wayPoint_.target;
    duration = (std::max)(wayPoint_.timeToTarget, 0.0f);
    holdDuration = (std::max)(wayPoint_.holdDuration, 0.0f);
    returnDuration = (std::max)(wayPoint_.timeToDefault, 0.0f);
    elapsed = 0.0f;
    phase = PHASE::TO_TARGET;
}

MotionBlurSystem::Parameters MotionBlurDirector::Lerp(
    const MotionBlurSystem::Parameters& from_,
    const MotionBlurSystem::Parameters& to_,
    float ratio_)
{
    const float ratioClamped_ = std::clamp(ratio_, 0.0f, 1.0f);
    // Scalar値は線形補間し、boolやSample数は中間状態で不安定にならないよう最後に切り替える。
    const auto value_ = [ratioClamped_](float fromValue_, float toValue_)
    {
        return fromValue_ + (toValue_ - fromValue_) * ratioClamped_;
    };

    MotionBlurSystem::Parameters result_;
    result_.enabled = ratioClamped_ < 1.0f ? from_.enabled || to_.enabled : to_.enabled;
    result_.useCameraVelocity = ratioClamped_ < 1.0f
        ? from_.useCameraVelocity || to_.useCameraVelocity
        : to_.useCameraVelocity;
    result_.sampleCount = ratioClamped_ < 1.0f ? from_.sampleCount : to_.sampleCount;
    result_.strength = value_(from_.strength, to_.strength);
    result_.maxVelocity = value_(from_.maxVelocity, to_.maxVelocity);
    return result_;
}

void MotionBlurDirector::ResetToDefault()
{
    // 演出終了後はDefault値を明示的に戻し、次回再生時の基準を安定させる。
    GraphicsDeviceManager::Instance().GetMotionBlurParams() = defaultParams;
    phase = PHASE::IDLE;
    elapsed = 0.0f;
}

void MotionBlurDirector::Gui()
{
    // Default値は演出終了後に戻る基準としてEditorで調整する。
    ImGui::Checkbox("Default Enabled", &defaultParams.enabled);
    ImGui::Checkbox("Default Camera Velocity", &defaultParams.useCameraVelocity);
    ImGui::SliderInt("Default Samples", &defaultParams.sampleCount, 2, 16);
    ImGui::DragFloat("Default Strength", &defaultParams.strength, 0.01f, 0.0f, 4.0f);
    ImGui::DragFloat("Default Max Velocity", &defaultParams.maxVelocity, 0.001f, 0.0f, 1.0f);

    if (ImGui::Button("Capture Motion Blur"))
    {
        // 現在のRenderSystem設定をWaypointとして保存し、Actionから再生できるようにする。
        MotionBlurWayPoint wayPoint_;
        wayPoint_.target = GraphicsDeviceManager::Instance().GetMotionBlurParams();
        editorWaypoints.push_back(wayPoint_);
        RegisterActions();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Motion Blur"))
    {
        ResetToDefault();
    }

    for (int index_ = 0; index_ < static_cast<int>(editorWaypoints.size()); ++index_)
    {
        ImGui::PushID(index_);
        MotionBlurWayPoint& wayPoint_ = editorWaypoints[index_];
        if (ImGui::TreeNode(("Motion Blur " + std::to_string(index_)).c_str()))
        {
            // Waypointごとに目標値と遷移時間を調整する。
            ImGui::Checkbox("Enabled", &wayPoint_.target.enabled);
            ImGui::Checkbox("Camera Velocity", &wayPoint_.target.useCameraVelocity);
            ImGui::SliderInt("Samples", &wayPoint_.target.sampleCount, 2, 16);
            ImGui::DragFloat("Strength", &wayPoint_.target.strength, 0.01f, 0.0f, 4.0f);
            ImGui::DragFloat("Max Velocity", &wayPoint_.target.maxVelocity, 0.001f, 0.0f, 1.0f);
            ImGui::DragFloat("Time To Target", &wayPoint_.timeToTarget, 0.01f, 0.0f, 60.0f);
            ImGui::DragFloat("Hold", &wayPoint_.holdDuration, 0.01f, 0.0f, 60.0f);
            ImGui::DragFloat("Time To Default", &wayPoint_.timeToDefault, 0.01f, 0.0f, 60.0f);
            if (ImGui::Button("Play"))
            {
                Play(wayPoint_);
            }
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

void MotionBlurDirector::RegisterActions()
{
    // PostProcessカテゴリにMotionBlur_0, MotionBlur_1...として登録する。
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }

    for (size_t index_ = 0; index_ < editorWaypoints.size(); ++index_)
    {
        owner_->RegisterGlobalAction("PostProcess", "MotionBlur_" + std::to_string(index_), [this, index_]()
        {
            if (index_ < editorWaypoints.size())
            {
                Play(editorWaypoints[index_]);
            }
        });
    }
}

void MotionBlurDirector::Serialize(nlohmann::json& json_) const
{
    // Parameters構造をJSONへ展開する小さな変換関数。
    const auto write_ = [](const MotionBlurSystem::Parameters& value_)
    {
        return nlohmann::json{ { "enabled", value_.enabled },
            { "useCameraVelocity", value_.useCameraVelocity },
            { "samples", value_.sampleCount },
            { "strength", value_.strength },
            { "maxVelocity", value_.maxVelocity } };
    };

    json_["Default"] = write_(defaultParams);
    json_["Waypoints"] = nlohmann::json::array();
    for (const MotionBlurWayPoint& wayPoint_ : editorWaypoints)
    {
        json_["Waypoints"].push_back({ { "target", write_(wayPoint_.target) },
            { "toTarget", wayPoint_.timeToTarget },
            { "hold", wayPoint_.holdDuration },
            { "toDefault", wayPoint_.timeToDefault } });
    }
}

void MotionBlurDirector::Deserialize(const nlohmann::json& json_)
{
    // 保存時と同じKeyからParametersを復元する。
    const auto read_ = [](const nlohmann::json& input_, MotionBlurSystem::Parameters& value_)
    {
        value_.enabled = input_.value("enabled", value_.enabled);
        value_.useCameraVelocity = input_.value("useCameraVelocity", value_.useCameraVelocity);
        value_.sampleCount = input_.value("samples", value_.sampleCount);
        value_.strength = input_.value("strength", value_.strength);
        value_.maxVelocity = input_.value("maxVelocity", value_.maxVelocity);
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
            MotionBlurWayPoint wayPoint_;
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
