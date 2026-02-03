#include "PlayerIdleState.h"
#include"KeyInput.h"

void PlayerIdleState::Enter()
{
	m_pPlayerComponent = owner->GetComponent<PlayerComponent>();
	m_pMovementComponent = owner->GetComponent<MovementComponent>();

	m_pMovementComponent.lock()->SetMoveSpeed(0.0f);
	m_pEasingComponent = owner->GetComponent<EasingComponent>();

	//インデックス0番目のイージングのStartValueを0にする
	m_pEasingComponent.lock()->SetStartValue(0,0.0f);

	//押し戻し処理をする
	m_pPlayerComponent.lock()->IsTrigger(false);

}

void PlayerIdleState::Execute(float et)
{
	if (m_pPlayerComponent.lock()->Moving().Length() != 0)
	{
		//ダッシュ
		if (KeyInput::KeyPressed(KeyInput::Key::K_Shift))
		{
			m_pPlayerComponent.lock()->GetStateMachine()->ChangeState("PlayerRun");
		}

		else
		{
			m_pPlayerComponent.lock()->GetStateMachine()->ChangeState("PlayerWalk");
		}
	}

	//しゃがみ移動
	if (KeyInput::KeyPressed(KeyInput::Key::K_Ctrl))
	{
		m_pPlayerComponent.lock()->GetStateMachine()->ChangeState("PlayerSneak");
	}
}

void PlayerIdleState::Exit()
{

}
