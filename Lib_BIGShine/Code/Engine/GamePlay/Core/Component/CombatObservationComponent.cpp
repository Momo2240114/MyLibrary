#include "PrecompiledHeader.h"
#include "CombatObservationComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/System/CombatObservationSystem.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"

const std::string& CombatObservationComponent::GetName() const
{
    static const std::string NAME = "CombatObservationComponent";
    return NAME;
}

void CombatObservationComponent::Start()
{
    SetUpdateEnabled(false);
    SetLateUpdateEnabled(false);
}

void CombatObservationComponent::SetSystem(const std::shared_ptr<CombatObservationSystem>& system_)
{
    system = system_;
}

void CombatObservationComponent::Gui()
{
    BS_PROFILE_SCOPE("CombatObservationComponent::Gui");
    if (system) system->Gui();
}
 
REGISTER_COMPONENT(CombatObservationComponent, "CombatObservationComponent")
