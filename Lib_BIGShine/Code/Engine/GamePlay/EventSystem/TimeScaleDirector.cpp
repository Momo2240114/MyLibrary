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
        // TimeScale演出の標準対象はPlayerを除いた反応側Layer。
        // 回避や攻撃入力の操作感を残しつつ、敵や弾、環境だけをスローにできる。
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

        // Default/All/空指定はTimeManager全体へ影響が広すぎるため、
        // Directorでは演出用の標準Layerへ丸めて扱う。
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
    // GlobalActionやEditorのPlayボタンから呼ばれる再生入口。
    // 範囲外IndexやTimeManager未取得時は、演出を発火せず安全に抜ける。
    if (index_ >= editorWaypoints.size())
    {
        return;
    }

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    const std::shared_ptr<TimeManager> timeManager_ = owner_
        ? owner_->GetManager().GetTimeManager()
        : nullptr;

    if (!timeManager_)
    {
        return;
    }

    const TimeScaleWayPoint& wayPoint_ = editorWaypoints[index_];

    // Layer指定が空の演出は意図しない全体停止にならないよう再生しない。
    if (wayPoint_.layerMask == 0u)
    {
        return;
    }

    timeManager_->PlayTimedScale(
        wayPoint_.layerMask,
        wayPoint_.targetScale,
        wayPoint_.timeToTarget,
        wayPoint_.holdDuration,
        wayPoint_.timeToDefault,
        wayPoint_.defaultScale);
}

void TimeScaleDirector::ResetTimeScale()
{
    BS_PROFILE_SCOPE("TimeScaleDirector::ResetTimeScale");
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->manager)
    {
        return;
    }
    const auto timeManager_ = owner_->GetManager().GetTimeManager();
    if (!timeManager_)
    {
        return;
    }
    uint32_t layerMask_ = 0u;
    for (const auto& waypoint_ : editorWaypoints)
    {
        layerMask_ |= SanitizeTimeScaleLayerMask(waypoint_.layerMask);
    }
    if (layerMask_ != 0u)
    {
        timeManager_->ResetToDefaults(layerMask_);
    }
}

void TimeScaleDirector::RegisterActions()
{
    // Waypointごとに TimeScale_0, TimeScale_1... のGlobalActionを登録する。
    // Timelineや他Componentから名前でTimeScale演出を呼び出せるようにする。
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }
    owner_->RegisterGlobalAction("TimeScale", "ResetTimeScale", [this]()
    {
        ResetTimeScale();
    });
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
        // 新規Waypointは標準反応Layerを対象にして、追加直後から安全に試せる状態にする。
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
            // 1つのWaypointで「Scaleへ移動 -> 維持 -> 戻す」までの一連の時間を調整する。
            ImGui::SliderFloat("Target Scale", &wayPoint_.targetScale, 0.0f, 2.0f);
            ImGui::DragFloat("Time To Target", &wayPoint_.timeToTarget, 0.01f, 0.0f, 60.0f);
            ImGui::DragFloat("Hold Duration", &wayPoint_.holdDuration, 0.01f, 0.0f, 60.0f);
            ImGui::DragFloat("Time To Default", &wayPoint_.timeToDefault, 0.01f, 0.0f, 60.0f);
            ImGui::SliderFloat("Default Scale", &wayPoint_.defaultScale, 0.0f, 2.0f);

            wayPoint_.layerMask = SanitizeTimeScaleLayerMask(wayPoint_.layerMask);

            // Layerはbit maskで保持するため、CheckboxのON/OFFをbit操作へ変換する。
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
                // Player以外を止める基本演出へ戻す。
                wayPoint_.layerMask = GetDefaultTimeScaleLayerMask();
            }

            ImGui::SameLine();

            if (ImGui::Button("Clear Layers"))
            {
                // 完全な空MaskはPlay時に無効になるため、Editor上では標準Layerへ戻す。
                wayPoint_.layerMask = GetDefaultTimeScaleLayerMask();
            }

            ImGui::Text("Layer Mask: %u", wayPoint_.layerMask);
            ImGui::TextDisabled("Default and All are blocked in TimeScaleDirector.");

            ImGui::Separator();

            ImGui::TextDisabled("Global Action: TimeScale_%d", index_);

            if (ImGui::Button("Play"))
            {
                // Editor上で設定値をその場で確認するための手動再生。
                Play(static_cast<size_t>(index_));
            }

            ImGui::SameLine();

            if (ImGui::Button("Remove") && editorWaypoints.size() > 1)
            {
                // 最低1つは残し、Action登録やEditor表示が空になりすぎないようにする。
                editorWaypoints.erase(editorWaypoints.begin() + index_--);
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }
}

void TimeScaleDirector::Serialize(nlohmann::json& json_) const
{
    // Waypoint配列として保存し、Scene読み込み後もEditor設定とGlobalActionを復元できるようにする。
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
    // 読み込み前にEditor側の一時状態を消し、JSONの内容から作り直す。
    editorWaypoints.clear();

    const uint32_t allLayerMask_ = static_cast<uint32_t>(TarGetLayerType::All);
    const uint32_t playerLayerMask_ = static_cast<uint32_t>(TarGetLayerType::Player);
    const uint32_t defaultNonPlayerLayerMask_ = allLayerMask_ & ~playerLayerMask_;

    if (json_.contains("Waypoints") && json_["Waypoints"].is_array())
    {
        for (const nlohmann::json& item_ : json_["Waypoints"])
        {
            // 古いSceneに項目が無い場合でも既定値で補完し、負の時間やScaleは0以上に丸める。
            TimeScaleWayPoint wayPoint_;

            wayPoint_.targetScale = (std::max)(item_.value("TargetScale", 0.4f), 0.0f);
            wayPoint_.timeToTarget = (std::max)(item_.value("TimeToTarget", 0.05f), 0.0f);
            wayPoint_.holdDuration = (std::max)(item_.value("HoldDuration", 10.0f), 0.0f);
            wayPoint_.timeToDefault = (std::max)(item_.value("TimeToDefault", 0.15f), 0.0f);
            wayPoint_.defaultScale = (std::max)(item_.value("DefaultScale", 1.0f), 0.0f);

            wayPoint_.layerMask = item_.value("LayerMask", defaultNonPlayerLayerMask_);

            if (wayPoint_.layerMask == allLayerMask_)
            {
                // 過去データのAll指定はPlayerまで止めないよう、Player以外のLayerへ変換する。
                wayPoint_.layerMask = defaultNonPlayerLayerMask_;
            }

            editorWaypoints.push_back(wayPoint_);
        }
    }

    if (editorWaypoints.empty())
    {
        // JSONに有効なWaypointが無い場合でも、Editorで即編集できるよう1件だけ用意する。
        editorWaypoints.emplace_back();
        editorWaypoints.back().layerMask = defaultNonPlayerLayerMask_;
    }
}
