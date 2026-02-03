#include "PlayerDeadState.h"

#include"CapsuleCollider.h"
#include"TransformDirector.h"
#include"ActionRegistry.h"
#include"MovementComponent.h"

void PlayerDeadState::Enter()
{
	m_pCapsuleCollider = owner->GetComponent<CapsuleCollider>();
	m_pTransformDirector = owner->GetComponent<TransformDirector>();
	m_pMovementComponent = owner->GetComponent<MovementComponent>();

	m_pCapsuleCollider.lock()->SetEnabled(false);
	m_pMovementComponent.lock()->SetEnabled(false);

	m_Step = 0;

}

void PlayerDeadState::Execute(float et)
{
	switch (m_Step)
	{
	case 0:
		m_pTransformDirector.lock()->PlayMoveByIndex(1);
		m_Step++;
		break;
	case 1:
		if (!m_pTransformDirector.lock()->GetIsMoving())
		{
			m_pTransformDirector.lock()->PlayMoveByIndex(2);
			m_Step++;
		}
		break;
	case 2:
		if (m_pTransformDirector.lock()->GetIsMoving())
		{
			//ここでUIのイベントを呼び出す処理をする
			ActionRegistry::Instance().Call("GameOver");
			m_Step++;
		}
		break;
	case 3:
		break;
	}
}
