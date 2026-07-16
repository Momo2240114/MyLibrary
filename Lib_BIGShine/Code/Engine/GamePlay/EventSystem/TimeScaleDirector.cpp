#include "PrecompiledHeader.h"

#include "TimeScaleDirector.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/System/TimeManager.h"

#include <algorithm>
#include <imgui.h>

namespace
{
    uint32_t ToMask(TarGetLayerType layerType_)
    {
        return static_cast<uint32_t>(layerType_);
    }

    uint32_t GetDefaultTimeScaleLayerMask()
    {
        return
            ToMask(TarGetLayerType::Enemy) |
            ToMask(TarGetLayerType::EnemyBullet) |
            ToMask(TarGetLayerType::Environment) |
            ToMask(TarGetLayerType::SneakObject) |
            ToMask(TarGetLayerType::Terrain) |
            ToMask(TarGetLayerType::Wall) |
            ToMask(TarGetLayerType::Item);
    }

    uint32_t SanitizeTimeScaleLayerMask(uint32_t layerMask_)
    {
        constexpr uint32_t defaultMask_ = static_cast<uint32_t>(TarGetLayerType::Default);
        constexpr uint32_t allMask_ = static_cast<uint32_t>(TarGetLayerType::All);

        if (layerMask_ == defaultMask_ || layerMask_ == allMask_)
        {
            return GetDefaultTimeScaleLayerMask();
        }

        if (layerMask_ == 0u)
        {
            return GetDefaultTimeScaleLayerMask();
        }

        return layerMask_;
    }
}

void TimeScaleDirector::Play(size_t index_)
{
    if (index_ >= editorWaypoints.size()) return;

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    const std::shared_ptr<TimeManager> timeManager_ = owner_
        ? owner_->GetManager().GetTimeManager()
        : nullptr;

    if (!timeManager_) return;

    const TimeScaleWayPoint& wayPoint_ = editorWaypoints[index_];

    if (wayPoint_.layerMask == 0u) return;

    timeManager_->PlayTimedScale(
        wayPoint_.layerMask,
        wayPoint_.targetScale,
        wayPoint_.timeToTarget,
        wayPoint_.holdDuration,
        wayPoint_.timeToDefault,
        wayPoint_.defaultScale);
}

void TimeScaleDirector::RegisterActions()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;
    for (size_t index_ = 0; index_ < editorWaypoints.size(); ++index_)
    {
        owner_->RegisterGlobalAction(
            "TimeScale", "TimeScale_" + std::to_string(index_), [this, index_]()
        {
            Play(index_);
        });
    }
}

void TimeScaleDirector::Gui()
{
    if (ImGui::Button("Add Time Scale"))
    {
        editorWaypoints.emplace_back();
        editorWaypoints.back().layerMask = GetDefaultTimeScaleLayerMask();
        RegisterActions();
    }

    for (int index_ = 0; index_ < static_cast<int>(editorWaypoints.size()); ++index_)
    {
        ImGui::PushID(index_);

        TimeScaleWayPoint& wayPoint_ = editorWaypoints[index_];

        if (ImGui::TreeNode(("Time Scale " + std::to_string(index_)).c_str()))
        {
            ImGui::SliderFloat("Target Scale", &wayPoint_.targetScale, 0.0f, 2.0f);
            ImGui::DragFloat("Time To Target", &wayPoint_.timeToTarget, 0.01f, 0.0f, 60.0f);
            ImGui::DragFloat("Hold Duration", &wayPoint_.holdDuration, 0.01f, 0.0f, 60.0f);
            ImGui::DragFloat("Time To Default", &wayPoint_.timeToDefault, 0.01f, 0.0f, 60.0f);
            ImGui::SliderFloat("Default Scale", &wayPoint_.defaultScale, 0.0f, 2.0f);

            wayPoint_.layerMask = SanitizeTimeScaleLayerMask(wayPoint_.layerMask);

            ImGui::SeparatorText("Target Layers");

            auto LayerCheckBox_ = [&](const char* label_, TarGetLayerType layerType_)
                {
                    const uint32_t mask_ = static_cast<uint32_t>(layerType_);
                    bool enabled_ = (wayPoint_.layerMask & mask_) != 0u;

                    if (ImGui::Checkbox(label_, &enabled_))
                    {
                        if (enabled_)
                        {
                            wayPoint_.layerMask |= mask_;
                        }
                        else
                        {
                            wayPoint_.layerMask &= ~mask_;
                        }

                        wayPoint_.layerMask = SanitizeTimeScaleLayerMask(wayPoint_.layerMask);
                    }
                };

            LayerCheckBox_("Player", TarGetLayerType::Player);
            LayerCheckBox_("Enemy", TarGetLayerType::Enemy);
            LayerCheckBox_("PlayerBullet", TarGetLayerType::PlayerBullet);
            LayerCheckBox_("EnemyBullet", TarGetLayerType::EnemyBullet);
            LayerCheckBox_("UI", TarGetLayerType::UI);
            LayerCheckBox_("Environment", TarGetLayerType::Environment);
            LayerCheckBox_("SneakObject", TarGetLayerType::SneakObject);
            LayerCheckBox_("Terrain", TarGetLayerType::Terrain);
            LayerCheckBox_("Wall", TarGetLayerType::Wall);
            LayerCheckBox_("Item", TarGetLayerType::Item);

            if (ImGui::Button("Use Default Reaction Layers"))
            {
                wayPoint_.layerMask = GetDefaultTimeScaleLayerMask();
            }

            ImGui::SameLine();

            if (ImGui::Button("Clear Layers"))
            {
                wayPoint_.layerMask = GetDefaultTimeScaleLayerMask();
            }

            ImGui::Text("Layer Mask: %u", wayPoint_.layerMask);
            ImGui::TextDisabled("Default and All are blocked in TimeScaleDirector.");

            ImGui::Separator();

            ImGui::TextDisabled("Global Action: TimeScale_%d", index_);

            if (ImGui::Button("Play"))
            {
                Play(static_cast<size_t>(index_));
            }

            ImGui::SameLine();

            if (ImGui::Button("Remove") && editorWaypoints.size() > 1)
            {
                editorWaypoints.erase(editorWaypoints.begin() + index_--);
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }
}

void TimeScaleDirector::Serialize(nlohmann::json& json_) const
{
    json_["Waypoints"] = nlohmann::json::array();

    for (const TimeScaleWayPoint& wayPoint_ : editorWaypoints)
    {
        json_["Waypoints"].push_back({
            { "TargetScale", wayPoint_.targetScale },
            { "TimeToTarget", wayPoint_.timeToTarget },
            { "HoldDuration", wayPoint_.holdDuration },
            { "TimeToDefault", wayPoint_.timeToDefault },
            { "DefaultScale", wayPoint_.defaultScale },
            { "LayerMask", SanitizeTimeScaleLayerMask(wayPoint_.layerMask) }
            });
    }
}

void TimeScaleDirector::Deserialize(const nlohmann::json& json_)
{
    editorWaypoints.clear();

    const uint32_t allLayerMask_ = static_cast<uint32_t>(TarGetLayerType::All);
    const uint32_t playerLayerMask_ = static_cast<uint32_t>(TarGetLayerType::Player);
    const uint32_t defaultNonPlayerLayerMask_ = allLayerMask_ & ~playerLayerMask_;

    if (json_.contains("Waypoints") && json_["Waypoints"].is_array())
    {
        for (const nlohmann::json& item_ : json_["Waypoints"])
        {
            TimeScaleWayPoint wayPoint_;

            wayPoint_.targetScale = (std::max)(item_.value("TargetScale", 0.4f), 0.0f);
            wayPoint_.timeToTarget = (std::max)(item_.value("TimeToTarget", 0.05f), 0.0f);
            wayPoint_.holdDuration = (std::max)(item_.value("HoldDuration", 10.0f), 0.0f);
            wayPoint_.timeToDefault = (std::max)(item_.value("TimeToDefault", 0.15f), 0.0f);
            wayPoint_.defaultScale = (std::max)(item_.value("DefaultScale", 1.0f), 0.0f);

            wayPoint_.layerMask = item_.value("LayerMask", defaultNonPlayerLayerMask_);

            if (wayPoint_.layerMask == allLayerMask_)
            {
                wayPoint_.layerMask = defaultNonPlayerLayerMask_;
            }

            editorWaypoints.push_back(wayPoint_);
        }
    }

    if (editorWaypoints.empty())
    {
        editorWaypoints.emplace_back();
        editorWaypoints.back().layerMask = defaultNonPlayerLayerMask_;
    }
}