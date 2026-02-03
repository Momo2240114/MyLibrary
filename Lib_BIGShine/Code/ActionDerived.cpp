#include "ActionDerived.h"

#include"GameObject.h"
#include"EnemyComponent.h"
#include"MovementComponent.h"
#include"AgentComponent.h"

#include"GameObjectManager.h"

#include <stdlib.h>

#include"AreaComponent.h"

#include"RandomHelper.h"
#include"AnimationController.h"
#include"PlayerComponent.h"
#include"EventManager.h"
#include"CapsuleCollider.h"
#include"TransformDirector.h"
#include"GraphicsDeviceManager.h"
#include"AudioManager.h"
ActionBase::State IdleAction::Run(float et)
{
	switch (step)
	{
	case 0:
		m_pEnemyComponent = m_pOwner->GetComponent<EnemyComponent>();
		m_pAgentComponent = m_pOwner->GetComponent<AgentComponent>();
		m_pAnimationControllerComponent = m_pOwner->GetComponent<AnimationController>();
		m_pAnimationControllerComponent.lock()->Play(2);

		m_RunTimer = 5.0f;

		step++;
	case 1:
		//タイマー更新
		m_RunTimer -= et;

		if (m_RunTimer <= 0.0f)
		{
			int index = RandomRangeCreat(0, 5);
			//static int index = 0;
			//index %= 6;
			//0～5までのランダムなデータを取得する
			GameObject* gameObject= GameObjectManager::Instance().Find("SearchObject" + std::to_string(index)).get();
			m_pAreaComponent = gameObject->GetComponent<AreaComponent>();

			//位置を取得する
			Vector3 position = m_pAreaComponent.lock()->GetPosition();
			float radius = m_pAreaComponent.lock()->GetRadius();

			//ターゲットの範囲内にターゲットがセットされたらアクションノードの遷移を行わない
			bool radiusFlag = m_pAgentComponent.lock()->SetRandomTargetPosition(position,radius);
			if (radiusFlag)
			{
				step = 0;
				index++;
				//処理を終了
				return State::Complete;
			}
		}

		dtCrowdAgent* ag = GameObjectManager::Instance().GetNaviMeshManager()->GetEditableAgent(m_pAgentComponent.lock()->GetAgentIndex());

		//プレイヤーを視認できたら
		if (m_pEnemyComponent.lock()->Visibility() && !m_pEnemyComponent.lock()->StateCheck(PlayerStateType::Hide))
		{
			//壁と隠れるオブジェクト
			uint32_t layerMask = (uint32_t)GameObject::LayerType::Wall | (uint32_t)GameObject::LayerType::SneakObject;

			//自分の位置を取得する
			Vector3 start = m_pOwner->transform->GetWorldPosition();

			//プレイヤーの位置を取得する
			Vector3 end = m_pEnemyComponent.lock()->GetTargetPosition();

			Vector3 hitPosition;
			Vector3 hitNormal;

			//自分とプレイヤーの間に障害物がなければ
			if (GameObjectManager::Instance().GetRayCastManager()->RayCast(start, end, layerMask, hitPosition, hitNormal) == nullptr)
			{
				//すぐに止めるためにこれを使っている
				ag->vel[0] = 0;
				ag->vel[1] = 0;
				ag->vel[2] = 0;

				ag->dvel[0] = 0;
				ag->dvel[1] = 0;
				ag->dvel[2] = 0;

				step = 0;
				return State::Complete;
			}
		}

		//エネミーが特定の範囲内でプレイヤーが走ったとき
		if (m_pEnemyComponent.lock()->Caution())
		{

			m_pAgentComponent.lock()->Reset();

			//すぐに止めるためにこれを使っている
			ag->vel[0] = 0;
			ag->vel[1] = 0;
			ag->vel[2] = 0;

			ag->dvel[0] = 0;
			ag->dvel[1] = 0;
			ag->dvel[2] = 0;

			////エリアのゲームオブジェクトを取得する
			//std::weak_ptr<GameObject> areaObject = GameObjectManager::Instance().GetSearchAreaManager()->GetNearPoint(m_pEnemyComponent.lock()->GetTargetPosition());
			//
			//std::weak_ptr<AreaComponent> areaComponent = areaObject.lock()->GetComponent<AreaComponent>();

			//m_pAgentComponent.lock()->SetRandomTargetPosition(areaComponent.lock()->GetPosition(),areaComponent.lock()->GetRadius());


			step = 0;

			return State::Complete;
		}
		break;
	}
}


ActionBase::State WanderAction::Run(float et)
{
	switch (step)
	{
	case 0:
		m_pEnemyComponent = m_pOwner->GetComponent<EnemyComponent>();
		m_pMovementComponent = m_pOwner->GetComponent<MovementComponent>();
		m_pAgentComponent = m_pOwner->GetComponent<AgentComponent>();
		m_pAnimationControllerComponent = m_pOwner->GetComponent<AnimationController>();
		m_pAnimationControllerComponent.lock()->Play(0);

		//パラメータをもとに戻す
		m_pAgentComponent.lock()->SetMaxSpeed(3.5f);
		m_pAgentComponent.lock()->SetMaxAcceleration(8.0f);

		step++;
	case 1:

		dtCrowdAgent* ag = GameObjectManager::Instance().GetNaviMeshManager()->GetEditableAgent(m_pAgentComponent.lock()->GetAgentIndex());
		//目標地点の半径Rudius以内に近づいたら処理を終了
		if (m_pAgentComponent.lock()->GetIsArrive())
		{
			m_pAgentComponent.lock()->Reset();

			Vector3 slide = { ag->vel[0],ag->vel[1],ag->vel[2]};

			if (slide.Length() < 0.1f)
			{
				//すぐに止めるためにこれを使っている
				ag->vel[0] = 0;
				ag->vel[1] = 0;
				ag->vel[2] = 0;

				ag->dvel[0] = 0;
				ag->dvel[1] = 0;
				ag->dvel[2] = 0;


				step = 0;
				return State::Complete;
			}
		}

		//プレイヤーを視認できたら
		if (m_pEnemyComponent.lock()->Visibility() && !m_pEnemyComponent.lock()->StateCheck(PlayerStateType::Hide))
		{
			//壁と隠れるオブジェクト
			uint32_t layerMask = (uint32_t)GameObject::LayerType::Wall | (uint32_t)GameObject::LayerType::SneakObject;

			//自分の位置を取得する
			Vector3 start = m_pOwner->transform->GetWorldPosition();

			//プレイヤーの位置を取得する
			Vector3 end = m_pEnemyComponent.lock()->GetTargetPosition();

			Vector3 hitPosition;
			Vector3 hitNormal;

			//自分とプレイヤーの間に障害物がなければ
			if (GameObjectManager::Instance().GetRayCastManager()->RayCast(start,end,layerMask,hitPosition,hitNormal) == nullptr)
			{
				//すぐに止めるためにこれを使っている
				ag->vel[0] = 0;
				ag->vel[1] = 0;
				ag->vel[2] = 0;

				ag->dvel[0] = 0;
				ag->dvel[1] = 0;
				ag->dvel[2] = 0;

				step = 0;
				return State::Complete;
			}
		}

		//エネミーが特定の範囲内でプレイヤーが走ったとき
		if (m_pEnemyComponent.lock()->Caution())
		{

			m_pAgentComponent.lock()->Reset();

			//すぐに止めるためにこれを使っている
			ag->vel[0] = 0;
			ag->vel[1] = 0;
			ag->vel[2] = 0;

			ag->dvel[0] = 0;
			ag->dvel[1] = 0;
			ag->dvel[2] = 0;

			////エリアのゲームオブジェクトを取得する
			//std::weak_ptr<GameObject> areaObject = GameObjectManager::Instance().GetSearchAreaManager()->GetNearPoint(m_pEnemyComponent.lock()->GetTargetPosition());
			//
			//std::weak_ptr<AreaComponent> areaComponent = areaObject.lock()->GetComponent<AreaComponent>();

			//m_pAgentComponent.lock()->SetRandomTargetPosition(areaComponent.lock()->GetPosition(),areaComponent.lock()->GetRadius());


			step = 0;

			return State::Complete;
		}
		break;
	}
	return State::Run;
}


ActionBase::State ChaseAction::Run(float et)
{
	switch (step)
	{
	case 0:
		m_pEnemyComponent = m_pOwner->GetComponent<EnemyComponent>();
		m_pMovementComponent = m_pOwner->GetComponent<MovementComponent>();
		m_pAgentComponent = m_pOwner->GetComponent<AgentComponent>();
		m_pAnimationControllerComponent = m_pOwner->GetComponent<AnimationController>();
		m_pAnimationControllerComponent.lock()->Play(1);

		//プレイヤーを発見したときに音を鳴らす
		AudioManager::Instance().StopSE(SE_DISCOVERY);
		AudioManager::Instance().PlaySE(SE_DISCOVERY);
		step++;

	case 1:

		dtCrowdAgent* ag = GameObjectManager::Instance().GetNaviMeshManager()->GetEditableAgent(m_pAgentComponent.lock()->GetAgentIndex());

		//プレイヤーが隠れていないときに判定する
		if (!m_pEnemyComponent.lock()->StateCheck(PlayerStateType::Hide))
		{
			//壁のオブジェクト
			uint32_t layerMask = (uint32_t)GameObject::LayerType::Wall;
			m_pEnemyComponent.lock()->IsSeePlayer(layerMask);

			//プレイヤーを常に追いかける
			m_pEnemyComponent.lock()->PlayerChase();
		}


		//プレイヤーが隠れたら
		if (m_pEnemyComponent.lock()->StateCheck(PlayerStateType::Hide))
		{

			//ターゲットをリセットする
			m_pAgentComponent.lock()->Reset();

			//プレイヤーの隠れた位置を保存する
			//m_pEnemyComponent.lock()->PlayerHidePosition();

			Vector3 slide = { ag->vel[0],ag->vel[1],ag->vel[2] };

			if (slide.Length() < 0.1f)
			{
				//すぐに止めるためにこれを使っている
				ag->vel[0] = 0;
				ag->vel[1] = 0;
				ag->vel[2] = 0;

				ag->dvel[0] = 0;
				ag->dvel[1] = 0;
				ag->dvel[2] = 0;

				step = 0;
				return State::Complete;
			}
		}

		//攻撃範囲に入ったのなら
		if (m_pEnemyComponent.lock()->AttackRange() && !m_pEnemyComponent.lock()->StateCheck(PlayerStateType::Hide))
		{
			//敵の移動をすぐに止める
			m_pAgentComponent.lock()->Reset();

			//すぐに止めるためにこれを使っている
			ag->vel[0] = 0;
			ag->vel[1] = 0;
			ag->vel[2] = 0;

			ag->dvel[0] = 0;
			ag->dvel[1] = 0;
			ag->dvel[2] = 0;

			//これをしないと1フレームの違いでエネミーがかくついてしまう
			m_pEnemyComponent.lock()->SetAttackFlag(true);
			step = 0; 
			return State::Complete;
		}

		m_RunTimer -= et;


		//スピードを上げる
		m_pAgentComponent.lock()->SetMaxSpeed(10.0f);

		//加速度を上げる
		m_pAgentComponent.lock()->SetMaxAcceleration(40.0f);

	}
	return State::Run;
}

ActionBase::State AttackAction::Run(float et)
{
	switch (step)
	{
	case 0:
		{
			m_pAnimationControllerComponent = m_pOwner->GetComponent<AnimationController>();
			m_pAgentComponent = m_pOwner->GetComponent<AgentComponent>();
			m_pAnimationControllerComponent.lock()->Play(3);
			m_pEnemyComponent = m_pOwner->GetComponent<EnemyComponent>();
			//m_pMovementComponent = m_pOwner->GetComponent<MovementComponent>();
			// 
			//加速度を40にする
			m_pAgentComponent.lock()->SetMaxAcceleration(40.0f);

				//スピードを上げる
			m_pAgentComponent.lock()->SetMaxSpeed(10.0f);

			m_AnimationTime = 1.5f;
			step++;
		}
	case 1:
		//プレイヤーを常に追いかける
		m_pEnemyComponent.lock()->PlayerChase();

		if (m_pEnemyComponent.lock()->HitPlayer() && !m_PlayerHitFlag)
		{
			//攻撃可能状態を元に戻す
			m_pEnemyComponent.lock()->SetAttackFlag(false);

			EventManager::Trigger("PlayerDead");

			step = 0;
			m_PlayerHitFlag = true;
			return State::Complete;
		}

		//アニメーションが終了したとき
		if (m_pAnimationControllerComponent.lock()->IsAnimationFinished())
		{
			//攻撃可能状態を元に戻す
			m_pEnemyComponent.lock()->SetAttackFlag(false);

			step = 0;
			return State::Complete;
		}
		m_AnimationTime -= et;
	}
	return State::Run;
}

ActionBase::State CautionAction::Run(float et)
{
	switch (step)
	{
	case 0:
	{
		m_pAnimationControllerComponent = m_pOwner->GetComponent<AnimationController>();
		m_pAgentComponent = m_pOwner->GetComponent<AgentComponent>();

		m_pEnemyComponent = m_pOwner->GetComponent<EnemyComponent>();

		//ターンをするアニメーション
		m_pAnimationControllerComponent.lock()->Play(4);
		step++;
	}
	case 1:

		dtCrowdAgent* ag = GameObjectManager::Instance().GetNaviMeshManager()->GetEditableAgent(m_pAgentComponent.lock()->GetAgentIndex());

		//プレイヤーを視認できたら
		if (m_pEnemyComponent.lock()->Visibility() && !m_pEnemyComponent.lock()->StateCheck(PlayerStateType::Hide))
		{
			//壁と隠れるオブジェクト
			uint32_t layerMask = (uint32_t)GameObject::LayerType::Wall | (uint32_t)GameObject::LayerType::SneakObject;

			//自分の位置を取得する
			Vector3 start = m_pOwner->transform->GetWorldPosition();

			//プレイヤーの位置を取得する
			Vector3 end = m_pEnemyComponent.lock()->GetTargetPosition();

			Vector3 hitPosition;
			Vector3 hitNormal;

			//自分とプレイヤーの間に障害物がなければ
			if (GameObjectManager::Instance().GetRayCastManager()->RayCast(start, end, layerMask, hitPosition, hitNormal) == nullptr)
			{
				//すぐに止めるためにこれを使っている
				ag->vel[0] = 0;
				ag->vel[1] = 0;
				ag->vel[2] = 0;

				ag->dvel[0] = 0;
				ag->dvel[1] = 0;
				ag->dvel[2] = 0;

				step = 0;
				return State::Complete;
			}
		}


		//アニメーションが終了したとき
		if (m_pAnimationControllerComponent.lock()->IsAnimationFinished())
		{
			//攻撃可能状態を元に戻す
			//攻撃可能状態を元に戻す
			m_pEnemyComponent.lock()->SetTurnFlag(false);

			step = 0;
			return State::Complete;
		}
	}
	return State::Run;
}


ActionBase::State CheckPointMoveAction::Run(float et)
{
	switch (step)
	{
	case 0:
	{
		m_pAnimationControllerComponent = m_pOwner->GetComponent<AnimationController>();
		m_pAgentComponent = m_pOwner->GetComponent<AgentComponent>();

		m_pEnemyComponent = m_pOwner->GetComponent<EnemyComponent>();

		m_pMovementComponent = m_pOwner->GetComponent<MovementComponent>();

		//歩くアニメーションを再生する
		m_pAnimationControllerComponent.lock()->Play(0);

		//プレイヤーが隠れた位置まで移動する
		m_pAgentComponent.lock()->SetTargetPosition(m_pEnemyComponent.lock()->GetHidePosition());

		//スピードを上げる
		m_pAgentComponent.lock()->SetMaxSpeed(3.5f);
		m_pAgentComponent.lock()->SetMaxAcceleration(10.0);
		step++;
	}
	case 1:

		//目標地点の半径Rudius以内に近づいたら処理を終了
		if (m_pAgentComponent.lock()->GetIsArrive())
		{
			m_pAgentComponent.lock()->Reset();

			dtCrowdAgent* ag = GameObjectManager::Instance().GetNaviMeshManager()->GetEditableAgent(m_pAgentComponent.lock()->GetAgentIndex());
			Vector3 slide = { ag->vel[0],ag->vel[1],ag->vel[2] };

			if (slide.Length() < 0.1f)
			{
				//すぐに止めるためにこれを使っている
				ag->vel[0] = 0;
				ag->vel[1] = 0;
				ag->vel[2] = 0;

				ag->dvel[0] = 0;
				ag->dvel[1] = 0;
				ag->dvel[2] = 0;

				//step = 0;
				//return State::Complete;
				step++;
			}
		}
		break;
	case 2:
	{
		// 1. weak_ptrのリストを取得
		const auto& sneakWeakObjects = GameObjectManager::Instance().GetSneakObjects();

		// エネミーの位置を取得する
		Vector3 enemyPosition = m_pOwner->transform->GetWorldPosition();
		float minLengthSq = FLT_MAX; // 比較用に距離の二乗を保持（高速化）
		bool found = false;

		// 2. エネミーから最も近いスニークオブジェクトを探索
		for (const auto& weakObj : sneakWeakObjects)
		{
			// weak_ptrからshared_ptrを取得（生存確認）
			if (auto obj = weakObj.lock())
			{
				// レイヤーのチェック（GetSneakObjects側で保証されているなら省略可）
				if (obj->GetLayer() == GameObject::LayerType::SneakObject)
				{
					Vector3 diff = obj->transform->GetWorldPosition() - enemyPosition;
					float currentLengthSq = diff.LengthSquared(); // 平方根計算を避けるためLengthSqを使用

					if (currentLengthSq < minLengthSq)
					{
						minLengthSq = currentLengthSq;
						m_NearSneakObjectPosition = obj->transform->GetWorldPosition();
						m_pEnemyComponent.lock()->SetSneakObjectName(obj->GetName());
						found = true;
					}
				}
			}
		}

		if (found) {
			//m_NearSneakObjectPosition = m_pEnemyComponent.lock()->GetTargetPosition();
			step++;
		}
		break;
	}
	case 3:
	{
		Vector4 color = { 1,0.5f,0,1 };
		GraphicsDeviceManager::Instance().GetShapeRendererSystem().DrawSphere(m_NearSneakObjectPosition, 1.0f, color);

		//m_pEnemyComponent.lock()->SetUseNavMeshFlag(false);

		// エネミーの位置
		Vector3 enemyPosition = m_pOwner->transform->GetWorldPosition();

		// ターゲットの位置（これが正しいか確認！）
		Vector3 targetPos = m_NearSneakObjectPosition;

		// 方向計算
		Vector3 direction = targetPos - enemyPosition;
		Vector3 front = m_pOwner->transform->GetFront();

		// ★高さを無視（これがないと変な方向を向く原因になる）
		direction.y = 0;
		front.y = 0;

		// 正規化
		if (direction.LengthSquared() > 0.0001f) direction.Normalize();
		front.Normalize();

		float dot = front.Dot(direction);

		// ==========================================
		// ★デバッグ出力をここに追加
		// ==========================================
		char buffer[512];
		sprintf_s(buffer,
			"\n[DEBUG] EnemyPos(%.2f, %.2f, %.2f) -> TargetPos(%.2f, %.2f, %.2f)\n"
			"[DEBUG] Dir(%.2f, %.2f, %.2f) | Front(%.2f, %.2f, %.2f) | Dot: %.4f\n\n",
			enemyPosition.x, enemyPosition.y, enemyPosition.z,
			targetPos.x, targetPos.y, targetPos.z,
			direction.x, direction.y, direction.z,
			front.x, front.y, front.z,
			dot);

		// Visual Studioの「出力」タブに文字が出る関数
		OutputDebugStringA(buffer);
		// ==========================================


		// 移動コンポーネントに方向を入力
		if (auto movement = m_pMovementComponent.lock()) {
			movement->AddDirection(direction);

			// ★ここも修正：0だと回転しない可能性があるので、わずかに入れる
			movement->SetMoveSpeed(0.01f);
		}

		// 判定
		if (dot > 0.99f)
		{
			//step++;
			step = 0;
			return State::Complete;
		}

		break;
	}
	case 4:
		//覗き込むアニメーション
		m_pAnimationControllerComponent.lock()->Play(5);
		step++;

		break;
	case 5:
		Vector4 color = { 1,0.5f,0,1 };
		GraphicsDeviceManager::Instance().GetShapeRendererSystem().DrawSphere(m_NearSneakObjectPosition, 1.0f, color);
		static float count = 0;

		count += et;
		if(count >= 60)
		{
			step = 0;
			return State::Complete;
		}
		break;
	}
	return State::Run;
}




ActionBase::State PeekAction::Run(float et)
{
	switch (step)
	{
	case 0:
	{
		m_pAnimationControllerComponent = m_pOwner->GetComponent<AnimationController>();
		m_pAgentComponent = m_pOwner->GetComponent<AgentComponent>();

		m_pMovementComponent = m_pOwner->GetComponent<MovementComponent>();

		m_pEnemyComponent = m_pOwner->GetComponent<EnemyComponent>();

		m_pTransformDirector = m_pOwner->GetComponent<TransformDirector>();

		step++;
		break;

	//エネミーを下に下げる処理と回転の処理がかぶってしまうと厄介なので分ける
	case 1:
		//覗き込むアニメーション
		m_pAnimationControllerComponent.lock()->Play(5);

		//エネミーを無理やり沈み込ませて目線を合わせる
		m_pTransformDirector.lock()->PlayMoveByIndex(1);
		step++;

		m_Time = 0.0f;
		break;
	}
	case 2:
		//アニメーション終了時
		//if (m_pAnimationControllerComponent.lock()->IsAnimationFinished())
		//{
		//	//プレイヤーを発見したとき
		//	if (m_pEnemyComponent.lock()->Visibility())
		//	{
		//		//死亡演出のイベントを呼ぶ(プレイヤーを固定する)
		//		EventManager::Trigger("Death production");


		//		//プレイヤーを発見したときに音を鳴らす
		//		AudioManager::Instance().StopSE(SE_DISCOVERY);
		//		AudioManager::Instance().PlaySE(SE_DISCOVERY);

		//		return State::Complete;
		//	}
		//	else
		//	{
		//		m_pEnemyComponent.lock()->SetPlayerSeeFlag(false);
		//		step = 0;

		//		//エネミーの高さをもとに戻す
		//		m_pTransformDirector.lock()->PlayMoveByIndex(2);
		//		return State::Failed;
		//	}
		//}

		if (m_Time > 1.0f)
		{
			//プレイヤーを発見したとき
			if (m_pEnemyComponent.lock()->Visibility())
			{
				//死亡演出のイベントを呼ぶ(プレイヤーを固定する)
				EventManager::Trigger("Death production");


				//プレイヤーを発見したときに音を鳴らす
				AudioManager::Instance().StopSE(SE_DISCOVERY);
				AudioManager::Instance().PlaySE(SE_DISCOVERY);

				return State::Complete;
			}
			else
			{
				m_pEnemyComponent.lock()->SetPlayerSeeFlag(false);
				step = 0;

				//エネミーの高さをもとに戻す
				m_pTransformDirector.lock()->PlayMoveByIndex(2);
				return State::Failed;
			}
		}

		m_Time += et;
	}
	return State::Run;
}


ActionBase::State KillMotionAction::Run(float et)
{
	switch (step)
	{
	case 0:
		m_pAnimationControllerComponent = m_pOwner->GetComponent<AnimationController>();
		m_pAgentComponent = m_pOwner->GetComponent<AgentComponent>();

		m_pMovementComponent = m_pOwner->GetComponent<MovementComponent>();

		m_pEnemyComponent = m_pOwner->GetComponent<EnemyComponent>();

		m_pCapsuleColliderComponent = m_pOwner->GetComponent<CapsuleCollider>();


		//張って進むアニメーションを再生
		m_pAnimationControllerComponent.lock()->Play(6);

		m_pEnemyComponent.lock()->SetUseNavMeshFlag(false);

		m_pCapsuleColliderComponent.lock()->SetEnabled(false);

		step++;
		break;
	case 1:
	{
		Vector3 targetPosition = m_pEnemyComponent.lock()->GetTargetPosition();
		Vector3 enemyPosition = m_pOwner->transform->GetWorldPosition();
		targetPosition.y = 0;
		enemyPosition.y = 0;

		Vector3 direction = targetPosition - enemyPosition;

		float len = direction.Length();
		//一定の範囲内に近づいたら
		if (len < 0.1f)
		{
			//キル演出のアニメーションを再生させる
			m_pAnimationControllerComponent.lock()->Play(7);

			//プレイヤーが押し倒されるような処理をするイベントを呼ぶ
			EventManager::Trigger("PlayerHideDead");

			step++;
		}

		direction.Normalize();

		m_pMovementComponent.lock()->AddDirection(direction);
		m_pMovementComponent.lock()->SetMoveSpeed(1.0f);
		break;
	}
	case 2:

		//3秒経過後にゲームオーバーのイベントを呼ぶ
		if (m_Time > 3.0f)
		{
			ActionRegistry::Instance().Call("GameOver");
			m_Time = 0.0f;
		}
		m_Time += et;
	}
	return State::Run;
}
