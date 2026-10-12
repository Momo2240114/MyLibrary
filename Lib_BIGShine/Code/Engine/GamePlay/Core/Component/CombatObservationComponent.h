#pragma once

#include "ComponentBase.h"
#include <memory>
class CombatObservationSystem;

// Scene直下の専用Objectから独立監視SystemのInspectorを表示するComponent。
class CombatObservationComponent : public Component
{
public:
    const std::string& GetName() const override;
    // 監視時刻はManagerの更新境界で進めるため、Object更新から二重実行しない。
    void Start() override;
    void Gui() override;
    // Bossとは別寿命の共有Systemを接続し、Scene内の参照を所有しない。
    void SetSystem(const std::shared_ptr<CombatObservationSystem>& system_);
private:
    std::shared_ptr<CombatObservationSystem> system;
};
