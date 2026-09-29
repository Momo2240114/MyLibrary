#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

// 死亡通知をどのBoss撃破イベントとして扱うかを指定する。
enum class GameModeDeathNotifyTarget
{
    PHASE_1_BOSS,
    PHASE_2_BOSS
};

// Health_DeathをGameModeManagerのBoss撃破通知へ橋渡しするComponent。
class GameModeDeathNotifierComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "GameModeDeathNotifierComponent";
        return name_;
    }

    void Start() override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

private:
    // Health_Deathを受け取るLocal Actionを登録する。
    void RegisterActions();
    // 通知先に応じたGameMode用Global Action名を返す。
    const char* GetNotifyActionName() const;

private:
    // このObjectの死亡をどのBoss撃破通知として扱うか。
    GameModeDeathNotifyTarget notifyTarget = GameModeDeathNotifyTarget::PHASE_1_BOSS;
};
