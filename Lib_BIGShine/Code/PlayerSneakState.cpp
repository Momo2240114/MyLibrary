#include "PlayerSneakState.h"
#include"KeyInput.h"
#include"BoxCollider.h"

void PlayerSneakState::Enter()
{
	//プレイヤーコンポーネント
	m_pPlayerComponent = owner->GetComponent<PlayerComponent>();

	//イージングコンポーネント
	m_pEasingComponent = owner->GetComponent<EasingComponent>();

	//移動コンポーネント
	m_pMovementComponent = owner->GetComponent<MovementComponent>();


	m_pEasingComponent.lock()->PlayEasing(0,EasingType::Lerp,1.0f,3.0f);


	//コリジョンのコンポーネントを取得する
	m_pCapsuleColliderComponent = owner->GetComponent<CapsuleCollider>();

	//前回のフレームで何にもあたっていなかった場合
	if (m_pCapsuleColliderComponent.lock()->currentTriggers.size() == 0)
	{
		m_pEasingComponent.lock()->SetStartValue(1, owner->transform->localPosition.y);
		m_pEasingComponent.lock()->PlayEasing(1, EasingType::OutExpo, 5.0f, 1.0f);
	}

	//押し戻し処理をしない
	m_pPlayerComponent.lock()->IsTrigger(false);

	//スニークオブジェクトの当たり判定を消す
	owner->m_Mask |= (uint32_t)GameObject::LayerType::SneakObject;
}

void PlayerSneakState::Execute(float et)
{
	//入力処理
	m_pMovementComponent.lock()->AddDirection(m_pPlayerComponent.lock()->Moving());


	//オブジェクトに当たっているときは遷移しない
	if (m_pCapsuleColliderComponent.lock()->currentTriggers.size() != 0)
	{
		return;
	}

	if (!KeyInput::KeyPressed(KeyInput::Key::K_Ctrl))
	{
		if (m_pMovementComponent.lock()->GetDirection().Length() != 0.0f)
		{
			if (KeyInput::KeyPressed(KeyInput::Key::K_Shift))
			{
				m_pPlayerComponent.lock()->GetStateMachine()->ChangeState("PlayerRun");
			}
			else
			{
				m_pPlayerComponent.lock()->GetStateMachine()->ChangeState("PlayerWalk");
			}
		}

		//アイドル
		else
		{
			m_pPlayerComponent.lock()->GetStateMachine()->ChangeState("PlayerIdle");
		}
	}
}

void PlayerSneakState::LateExecute(float et)
{
	bool isUnderSneakObject = false;

	static int count = 0;
	// 現在接触しているトリガーをすべてチェック
	for (std::shared_ptr<GameObject> gameObject : m_pCapsuleColliderComponent.lock()->currentTriggers)
	{
		// 1. レイヤーを確認 (SneakObjectレイヤーのみを対象にする)
		if (gameObject->layer == GameObject::LayerType::SneakObject)
		{
			// 2. BoxColliderを持っているか確認
			BoxCollider* box = gameObject->GetComponent<BoxCollider>().get();
			if (box)
			{
				//呼ばれた瞬間だけプレイヤーの位置を保存する
				if (count == 0)
				{
					m_pPlayerComponent.lock()->SetHidePosition(owner->transform->GetWorldPosition());
					count = 1;
				}



				//プレイヤーの状態を隠れている状態に設定
				m_pPlayerComponent.lock()->SetPlayerState(PlayerStateType::Hide);

				// 机の下（SneakObject）にいる時：高さを下げる
				m_pEasingComponent.lock()->PlayEasing(1, EasingType::OutExpo, 5.0f, 0.5f);
				return;
			}
		}
	}

	m_pEasingComponent.lock()->PlayEasing(1, EasingType::OutExpo, 5.0f, 1.0f);
	count = 0;


	//// 判定結果に基づいて高さを調整
	//if (isUnderSneakObject)
	//{
	//	// 机の下（SneakObject）にいる時：高さを下げる
	//	m_pEasingComponent.lock()->PlayEasing(1, EasingType::OutExpo, 5.0f, 0.5f);
	//}
	//else
	//{
	//	// 対象がない時：元の高さ（立ち上がり）に戻す
	//	m_pEasingComponent.lock()->PlayEasing(1, EasingType::OutExpo, 5.0f, 1.0f);
	//}

	//for (std::shared_ptr<GameObject> gameObject : m_pCapsuleColliderComponent.lock()->currentTriggers)
	//{
	//	BoxCollider* box = gameObject->GetComponent<BoxCollider>().get();

	//	//Boxの当たり判定があった場合
	//	if (box)
	//	{
	//		////ヒットしたオブジェクトの高さを求める
	//		//float height = gameObject->transform->localPosition.y;
	//		//height += box->halfSize.y + box->offset.y;


	//		////現在のほうがオブジェクトより低ければ高さを下げる必要はない
	//		//if (owner->transform->localPosition.y < height)return;

	//		m_pEasingComponent.lock()->PlayEasing(1, EasingType::OutExpo, 5.0f, 0.5f);
	//	}
	//	return;
	//}

	////もしもあたっていなければ元の高さに戻す
	//m_pEasingComponent.lock()->PlayEasing(1, EasingType::OutExpo, 5.0f, 1.0f);
}

void PlayerSneakState::Exit()
{
	m_pEasingComponent.lock()->PlayEasing(1, EasingType::OutExpo, 5.0f, 2.0f);

	//押し出し処理をするように戻す
	owner->m_Mask = (uint32_t)GameObject::LayerType::Default;

	//プレイヤーのステートをもとに戻す
	m_pPlayerComponent.lock()->SetPlayerState(PlayerStateType::Default);
}
