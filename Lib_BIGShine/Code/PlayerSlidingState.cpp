

#include"PlayerSlidingState.h"
#include"BoxCollider.h"

#include"KeyInput.h"

void PlayerSlidingState::Enter()
{
	m_pPlayerComponent = owner->GetComponent<PlayerComponent>();

	m_pMovementComponent = owner->GetComponent<MovementComponent>();

	m_pEasingComponent = owner->GetComponent<EasingComponent>();

	m_pEasingComponent.lock()->PlayEasing(0, EasingType::Lerp, 1.0f, 3.0f);

	m_pEasingComponent.lock()->SetStartValue(1, 2.0f);
	m_pEasingComponent.lock()->PlayEasing(1, EasingType::OutExpo, 5.0f, 0.5f);

	m_Direction = m_pMovementComponent.lock()->GetDirection();

	//押し戻し処理をする
	m_pPlayerComponent.lock()->IsTrigger(false);

	m_pCapsuleColliderComponent = owner->GetComponent<CapsuleCollider>();

	//スニークオブジェクトの当たり判定を消す
	owner->m_Mask |= (uint32_t)GameObject::LayerType::SneakObject;
}

void PlayerSlidingState::Execute(float et)
{
	m_pMovementComponent.lock()->AddDirection(m_Direction);

	//オブジェクトに当たっているときはスニークのみ遷移する
	if (m_pCapsuleColliderComponent.lock()->currentTriggers.size() != 0)
	{
		if (m_pEasingComponent.lock()->GetEndFlag(0))
		{
			m_pPlayerComponent.lock()->GetStateMachine()->ChangeState("PlayerSneak");
		}
		return;
	}

	//キーを離したときに
	if (!KeyInput::KeyPressed(KeyInput::Key::K_Ctrl))
	{
		//入力されていたら
		if (m_pPlayerComponent.lock()->Moving().Length() != 0)
		{
			//走り
			if (KeyInput::KeyPressed(KeyInput::Key::K_Shift))
			{
				m_pPlayerComponent.lock()->GetStateMachine()->ChangeState("PlayerRun");
				m_pEasingComponent.lock()->PlayEasing(1, EasingType::OutExpo, 5.0f, 2.0f);
				return;
			}

			//歩き
			else
			{
				m_pPlayerComponent.lock()->GetStateMachine()->ChangeState("PlayerWalk");
				m_pEasingComponent.lock()->PlayEasing(1, EasingType::OutExpo, 5.0f, 2.0f);
				return;
			}
		}

		//アイドル
		m_pPlayerComponent.lock()->GetStateMachine()->ChangeState("PlayerIdle");
		m_pEasingComponent.lock()->PlayEasing(1, EasingType::OutExpo, 5.0f, 2.0f);

	}

	//スライディングが終了時
	//スニーク
	else if (m_pEasingComponent.lock()->GetEndFlag(0))
	{
		m_pPlayerComponent.lock()->GetStateMachine()->ChangeState("PlayerSneak");
	}
}

void PlayerSlidingState::Exit()
{
	//m_pEasingComponent.lock()->PlayEasing(1, EasingType::OutExpo, 5.0f, 2.0f);

	//プレイヤーのステートをもとに戻す
	m_pPlayerComponent.lock()->SetPlayerState(PlayerStateType::Default);


	//押し出し処理をするように戻す
	owner->m_Mask = (uint32_t)GameObject::LayerType::Default;
}

void PlayerSlidingState::LateExecute(float et)
{
	static int count = 0;
	for (std::shared_ptr<GameObject> gameObject : m_pCapsuleColliderComponent.lock()->currentTriggers)
	{
		BoxCollider* box = gameObject->GetComponent<BoxCollider>().get();

		//Boxの当たり判定があった場合
		if (box)
		{
			//呼ばれた瞬間だけプレイヤーの位置を保存する
			if (count == 0)
			{
				m_pPlayerComponent.lock()->SetHidePosition(owner->transform->GetWorldPosition());
				count = 1;
			}


			//CTRLキーを離したときプレイヤーの移動をなくす
			if (!KeyInput::KeyPressed(KeyInput::Key::K_Ctrl))
			{
				m_pPlayerComponent.lock()->GetStateMachine()->ChangeState("PlayerSneak");
			}


			//プレイヤーの状態を隠れている状態に設定
			m_pPlayerComponent.lock()->SetPlayerState(PlayerStateType::Hide);

			//ヒットしたオブジェクトの高さを求める
			float height = gameObject->transform->localPosition.y;
			height += box->halfSize.y + box->offset.y;


			//現在のほうがオブジェクトより低ければ高さを下げる必要はない
			if (owner->transform->localPosition.y < height)return;

			m_pEasingComponent.lock()->PlayEasing(1, EasingType::OutExpo, 5.0f, 0.5f);
		}
		return;
	}

	//もしもあたっていなければ元の高さに戻す
	m_pEasingComponent.lock()->PlayEasing(1, EasingType::OutExpo, 5.0f, 0.5f);

	count = 0;
}