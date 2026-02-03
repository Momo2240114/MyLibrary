#include "PlayerRunState.h"
#include"KeyInput.h"

#include"CameraComponent.h"
#include"PlayerComponent.h"
#include"CameraComponent.h"
#include"EasingComponent.h"
#include"MovementComponent.h"


void PlayerRunState::Enter()
{
	//プレイヤーコンポーネント
	m_pPlayerComponent = owner->GetComponent<PlayerComponent>();

	//イージングコンポーネント
	m_pEasingComponent = owner->GetComponent<EasingComponent>();

	//移動コンポーネント
	m_pMovementComponent = owner->GetComponent<MovementComponent>();

	m_pEasingComponent.lock()->PlayEasing(0, EasingType::Lerp, 1.0f, 10.0f);

	//押し戻し処理をする
	m_pPlayerComponent.lock()->IsTrigger(false);


	m_pCameraComponent = owner->GetComponent<CameraComponent>();

	//Fov
	m_pEasingComponent.lock()->PlayEasing(2, EasingType::Lerp, 1.5f,75);

	m_pPlayerComponent.lock()->SetPlayerState(PlayerStateType::Run);
}

void PlayerRunState::Execute(float et)
{
	//入力処理
	m_pMovementComponent.lock()->AddDirection(m_pPlayerComponent.lock()->Moving());

	if (KeyInput::KeyPressedOut(KeyInput::Key::K_Shift))
	{
		//歩く
		if(m_pMovementComponent.lock()->GetDirection().Length() != 0.0f)
		{
			m_pPlayerComponent.lock()->GetStateMachine()->ChangeState("PlayerWalk");
		}

		//アイドル
		else
		{
			m_pPlayerComponent.lock()->GetStateMachine()->ChangeState("PlayerIdle");
		}
	}
	

}

void PlayerRunState::LateExecute(float et)
{
	m_pPlayerComponent.lock()->Collider();

}

void PlayerRunState::Exit()
{
	m_pEasingComponent.lock()->PlayEasing(2, EasingType::Lerp, 1.5f,60);
	m_pPlayerComponent.lock()->SetPlayerState(PlayerStateType::Default);
}
