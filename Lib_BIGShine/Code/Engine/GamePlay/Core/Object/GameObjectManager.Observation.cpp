#include "PrecompiledHeader.h"
#include "GameObjectManager.h"
#include "Engine/GamePlay/System/CombatObservationSystem.h"
#include "Engine/GamePlay/Core/Component/CombatObservationComponent.h"

std::shared_ptr<CombatObservationSystem> GameObjectManager::GetCombatObservationSystem() const
{
    return combatObservationSystem;
}

void GameObjectManager::EnsureCombatObservationRuntime()
{
    BS_PROFILE_SCOPE("GameObjectManager::EnsureCombatObservationRuntime");
    const bool firstInitialization_ = !combatObservationSystem;
    if (firstInitialization_)
    {
        combatObservationSystem = std::make_shared<CombatObservationSystem>();
        combatObservationSystem->BindCombatEvents(*attackSystem, *guardSystem, *justGuardSystem);
    }
    auto monitor_ = combatObservationObject.lock();
    if (!monitor_ || !combatObservationSystem->IsRegisteredObject(monitor_))
    {
        // Scene直下の独立Objectにし、Dragon破棄時の子階層削除へ巻き込まない。
        monitor_ = Create("CombatObservation");
        monitor_->SetNoSaveObject();
        combatObservationObject = monitor_;
        const auto component_ = monitor_->GetOrAddComponent<CombatObservationComponent>(false);
        component_->SetUpdateEnabled(false);
        component_->SetLateUpdateEnabled(false);
        component_->SetSystem(combatObservationSystem);
    }
    // Inspector操作でもBossの子にはせず、常にScene直下の監視Objectとして扱う。
    if (monitor_->transform && !monitor_->transform->parent.expired()) monitor_->SetParent(nullptr);
    if (firstInitialization_)
    {
        // 初期化前に作られたObjectだけを一度登録し、以降は生成・構成変更通知で追加する。
        for (const auto& object_ : objects) combatObservationSystem->RegisterObject(object_);
    }
}
