#include "PrecompiledHeader.h"

#include "GameModeDeathNotifierComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"

#include <algorithm>
#include <imgui.h>

namespace
{
    constexpr const char* ACTION_HEALTH_DEATH = "Health_Death";
    constexpr const char* ACTION_NOTIFY_PHASE_1_BOSS_DEFEATED = "GameMode.NotifyPhase1BossDefeated";
    constexpr const char* ACTION_NOTIFY_PHASE_2_BOSS_DEFEATED = "GameMode.NotifyPhase2BossDefeated";
}

void GameModeDeathNotifierComponent::Start()
{
    RegisterActions();
}

void GameModeDeathNotifierComponent::RegisterActions()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }

    // HealthComponentの死亡通知を、GameMode用Global Actionへ変換する。
    owner_->RegisterAction("Health", ACTION_HEALTH_DEATH, [this]()
    {
        ActionRegistry::Instance().Call(GetNotifyActionName());
    });
}

const char* GameModeDeathNotifierComponent::GetNotifyActionName() const
{
    switch (notifyTarget)
    {
    case GameModeDeathNotifyTarget::PHASE_2_BOSS:
        return ACTION_NOTIFY_PHASE_2_BOSS_DEFEATED;
    case GameModeDeathNotifyTarget::PHASE_1_BOSS:
    default:
        return ACTION_NOTIFY_PHASE_1_BOSS_DEFEATED;
    }
}

void GameModeDeathNotifierComponent::Gui()
{
    int targetIndex_ = static_cast<int>(notifyTarget);
    if (ImGui::Combo(
        "Notify Target",
        &targetIndex_,
        "PHASE_1_BOSS\0PHASE_2_BOSS\0"))
    {
        notifyTarget = static_cast<GameModeDeathNotifyTarget>(
            std::clamp(targetIndex_, 0, 1));
        RegisterActions();
    }

    ImGui::Text("Health_Death -> %s", GetNotifyActionName());
}

void GameModeDeathNotifierComponent::Serialize(nlohmann::json& json_) const
{
    // Scene側でBoss種別を選べるよう、通知先だけを保存する。
    json_["NotifyTarget"] = static_cast<int>(notifyTarget);
}

void GameModeDeathNotifierComponent::Deserialize(const nlohmann::json& json_)
{
    const int targetIndex_ = std::clamp(json_.value("NotifyTarget", 0), 0, 1);
    notifyTarget = static_cast<GameModeDeathNotifyTarget>(targetIndex_);
}

REGISTER_COMPONENT(GameModeDeathNotifierComponent, "GameModeDeathNotifierComponent")
