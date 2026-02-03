
#include"PlayerWalkState.h"
#include"KeyInput.h"

void PlayerWalkState::Enter()
{
	m_pPlayerComponent = owner->GetComponent<PlayerComponent>();
	m_pEasingComponent = owner->GetComponent<EasingComponent>();
	m_pMovementComponent = owner->GetComponent<MovementComponent>();

	//m_pCameraComponent = owner->GetComponent<CameraComponent>();

	if (m_pEasingComponent.lock()->GetValue(0) < 5.0f)
	{
		m_pEasingComponent.lock()->SetStartValue(0,5.0f);
		m_pMovementComponent.lock()->SetMoveSpeed(5.0f);
	}
	else
	{
		m_pEasingComponent.lock()->PlayEasing(0, EasingType::Lerp, 1.0f, 5.0f);
	}

	//押し戻し処理をする
	m_pPlayerComponent.lock()->IsTrigger(false);


}

void PlayerWalkState::Execute(float et)
{
	//入力処理
	m_pMovementComponent.lock()->AddDirection(m_pPlayerComponent.lock()->Moving());


	if (m_pMovementComponent.lock()->GetMoveSpeed() >= 6.0f && m_pMovementComponent.lock()->GetDirection().Length() >= 0.1f)
	{
		if (KeyInput::KeyPressed(KeyInput::Key::K_Ctrl))
		{
			m_pPlayerComponent.lock()->GetStateMachine()->ChangeState("PlayerSliding");
			return;
		}
	}


	if (m_pMovementComponent.lock()->GetDirection().Length() != 0.0f)
	{
		//ダッシュ
		if (KeyInput::KeyPressed(KeyInput::Key::K_Shift))
		{
			m_pPlayerComponent.lock()->GetStateMachine()->ChangeState("PlayerRun");
		}

		//スニーク
		else if(KeyInput::KeyPressed(KeyInput::Key::K_Ctrl))
		{
			m_pPlayerComponent.lock()->GetStateMachine()->ChangeState("PlayerSneak");
		}	
	}

	//移動キーが押されていないときはアイドルに戻る
	if (m_pPlayerComponent.lock()->Moving().Length() <= 0.1f)
	{
		m_pPlayerComponent.lock()->GetStateMachine()->ChangeState("PlayerIdle");
	}
}

void PlayerWalkState::LateExecute(float et)
{
	m_pPlayerComponent.lock()->Collider();
}

void PlayerWalkState::Exit()
{

}