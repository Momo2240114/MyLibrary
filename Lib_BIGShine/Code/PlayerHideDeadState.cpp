#include "PlayerHideDeadState.h"

#include"CapsuleCollider.h"
#include"TransformDirector.h"
#include"ActionRegistry.h"
#include"MovementComponent.h"

void PlayerHideDeadState::Enter()
{
	m_pCapsuleCollider = owner->GetComponent<CapsuleCollider>();
	m_pTransformDirector = owner->GetComponent<TransformDirector>();
	m_pMovementComponent = owner->GetComponent<MovementComponent>();

	m_pCapsuleCollider.lock()->SetEnabled(false);
	m_pMovementComponent.lock()->SetEnabled(false);

	m_Step = 0;

}

void PlayerHideDeadState::Execute(float et)
{
	switch (m_Step)
	{
	case 0:
		m_pTransformDirector.lock()->PlayMoveByIndex(4);
		m_Step++;
		break;
	}
}
