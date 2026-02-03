

#include"PlayerComponent.h"
#include"GameObject.h"

#include"windows.h"
#include"Vector.h"
#include"KeyInput.h"
#include"MouseInput.h"

#include"PlayerIdleState.h"
#include"PlayerRunState.h"
#include"PlayerWalkState.h"
#include"PlayerSneakState.h"
#include"PlayerSlidingState.h"

#include"CapsuleCollider.h"
#include"LightManager.h"

#include"GameObjectManager.h"
#include"ItemComponent.h"

#include"Sprite.h"
#include"EventManager.h"
#include"PlayerDeadState.h"
#include"PlayerHideDeadState.h"

#include"AudioManager.h"

void PlayerComponent::Start()
{
	//イージングのコンポーネント
	m_pEasingComponent = GetGameObject()->GetOrAddComponent<EasingComponent>();

	//イージング
	m_pEasingComponent->AddEasing();
	m_pEasingComponent->AddEasing();
	m_pEasingComponent->AddEasing();

	m_pEasingComponent->SetStartValue(1, 2.0f);
	
	//コライダーコンポーネント追加
	m_pCapsuleColliderComponent = GetGameObject()->GetOrAddComponent<CapsuleCollider>();
	m_pCapsuleColliderComponent->isTrigger = true;



	m_pLightComponent = GetGameObject()->GetOrAddComponent<LightComponent>();

	// 設定の初期化（ロード済みであれば上書きしない、などの制御も可能）
	if (m_pLightComponent) {
		m_pLightComponent->type = LightComponent::Type::Spot;
		m_pLightComponent->range = 30;		
	}

	//こいつは最後にAddComponent
	m_pMovementComponent = GetGameObject()->GetOrAddComponent<MovementComponent>();

	//ステートマシン作成
	m_pStateMachine = std::make_shared<StateMachine>();

	m_pStateMachine->RegisterState(new PlayerIdleState(GetGameObject().get()));
	m_pStateMachine->RegisterState(new PlayerRunState(GetGameObject().get()));
	m_pStateMachine->RegisterState(new PlayerWalkState(GetGameObject().get()));
	m_pStateMachine->RegisterState(new PlayerSneakState(GetGameObject().get()));
	m_pStateMachine->RegisterState(new PlayerSlidingState(GetGameObject().get()));
	m_pStateMachine->RegisterState(new PlayerDeadState(GetGameObject().get()));
	m_pStateMachine->RegisterState(new PlayerHideDeadState(GetGameObject().get()));

	m_pStateMachine->ChangeState("PlayerIdle");


	m_pCameraComponent = GetGameObject()->GetOrAddComponent<CameraComponent>();
	m_Fov = m_pCameraComponent.lock()->GetFovY();
	m_pEasingComponent->SetStartValue(2,m_Fov);
	

	//死んだイベントが来たときにステートを切り替える処理をセットする
	EventManager::Subscribe("PlayerDead", [this]()
		{
		m_pStateMachine->ChangeState("PlayerDeadState");
		});

	EventManager::Subscribe("PlayerHideDead", [this]()
		{
			m_pStateMachine->ChangeState("PlayerHideDeadState");
		});


	//プレイヤーが死んだときにイベントをセットする
	EventManager::Subscribe("PlayerDead", [this]()
		{
			SetEvent(PlayerEvent::Dead);
		});

	EventManager::Subscribe("PlayerHideDead", [this]()
		{
			SetEvent(PlayerEvent::Dead);
		});

	EventManager::Subscribe("Death production", [this]()
		{
			SetEvent(PlayerEvent::DeathProduction);
		});


	GetGameObject()->layer= GameObject::LayerType::Player;

	//SEをロード
	//m_pHeartBeatSound= Audio::Instance().LoadAudioSource(".\\Data\\Audio\\SE\\heartbeat.wav");

	//AudioManager::Instance().AddAudioFromFile(".\\Data\\Audio\\SE\\heartbeat.wav");

	m_pEnemy = GameObjectManager::Instance().Find("Enemy");

	//m_pHeartBeatSound->Play(false);
	//m_pHeartBeatSound->Play(false);
	m_Time = 0.0f;
}

//PlayerComponent::~PlayerComponent()
//{
//	delete m_pHeartBeatSound;
//	m_pHeartBeatSound = nullptr;
//}

void PlayerComponent::Update(float et)
{
	//static int index = 0;
	//m_pHeartBeatSound[index]->Play(true);



	if (m_PlayerEvent == PlayerEvent::None)
	{
		//心音
		if (m_pEnemy.lock())
		{
			m_Diration = EnemyLength();

			if (m_Diration > 1.0f) {
				// 敵が遠い時は停止
				AudioManager::Instance().StopSE(SE_HEARTBEAT);
				m_Time = 0.0f;
			}
			else {

				if (m_Time > 3.5f * m_Diration) {
					// 一定間隔で再生
					AudioManager::Instance().StopSE(SE_HEARTBEAT);
					AudioManager::Instance().PlaySE(SE_HEARTBEAT);
					AudioManager::Instance().SetVolumeSE(SE_HEARTBEAT, 1 - m_Diration);
					m_Time = 0.0f;
				}
				// 敵が近い時
				m_Time += et;
			}
		}


		m_pMoveFlag = false;

		//カメラの視野角用のイージング
		float value = m_pEasingComponent->GetValue(2);
		m_pCameraComponent.lock()->SetFov(value);

		//高さ調整のイージング
		if (m_pEasingComponent->GetValue(1))
		{
			GetGameObject()->transform->localPosition.y = m_pEasingComponent->GetValue(1);
			GetGameObject()->transform->MarkDirty();
		}

		//移動速度のイージング
		m_pMovementComponent->SetMoveSpeed(m_pEasingComponent->GetValue(0));

		//回転処理
		Vector2 subMousePos = MouseInput::Instance().GetSubMousePos();
		Vector3 angle = { subMousePos.y,subMousePos.x,0 };
		m_pMovementComponent->AddAngle(angle * 2.0f);
	}

	//イベントが発生したときマウス固定を解除
	else
	{
		MouseInput::Instance().SetDebugFlag(true);
	}

	//クリア演出の時は呼ばない
	if (m_PlayerEvent != PlayerEvent::Clear && m_PlayerEvent != PlayerEvent::DeathProduction)
	{
		m_pStateMachine->Update(et);
	}
}

void PlayerComponent::LateUpdate(float et)
{
	m_pStateMachine->LateUpdate(et);

	//レイキャスト
	//レイヤーマスク
	//GameObject::LayerType maskLayer = GameObject::LayerType::Item;
	uint32_t layermask = ((uint32_t)GameObject::LayerType::Item | (uint32_t)GameObject::LayerType::Wall | (uint32_t)GameObject::LayerType::SneakObject);


	std::weak_ptr<CameraComponent> cameraComponent = GameObjectManager::Instance().GetMainCamera()->GetComponent<CameraComponent>();

	m_pUIObject = GameObjectManager::Instance().Find("ItemGetUI");

	//プレイヤーの位置
	Vector3 position = cameraComponent.lock()->GetEye();

	//プレイヤーの前方向ベクトル
	Vector3 front = GetGameObject()->transform->GetFront();
	front.Normalize();

	//レイの長さ
	float t = 3.0f;

	//Vector3 end = position + front * t;

	//スクリーン座標をワールド座標に変換する
	Vector3 end; 
	Vector2 screen = { 960,540 };
	
	cameraComponent.lock()->ScreenToWorld(screen,end);

	Vector3 vec = end - position;
	vec.Normalize();
	vec *= t;
	end = position + vec;


	GameObject* hitGameObject;

	//この二つは多分使わない
	Vector3 hitPosition;
	Vector3 hitNormal;

	if (m_pUIObject.lock())
	{
		m_pUIObject.lock()->GetComponent<SpriteRenderer>()->enabled = false;
	}

	hitGameObjectName = "";
	m_RayHit = false;
	hitGameObject = GameObjectManager::Instance().GetRayCastManager()->RayCast(position, end, layermask,hitPosition,hitNormal);
	//hitGameObject = GameObjectManager::Instance().GetRayCastManager()->RayCast(position, end, (uint32_t)maskLayer, hitPosition, hitNormal);
	if (hitGameObject != nullptr)
	{
		hitGameObjectName = hitGameObject->GetName();

		if (hitGameObject->GetComponent<ItemComponent>() != nullptr)
		{
			if (m_pUIObject.lock())
			{
				m_pUIObject.lock()->GetComponent<SpriteRenderer>()->enabled = true;
			}


			if (MouseInput::Instance().LeftDown())
			{
				//アイテムコンポーネントwお取得する
				std::weak_ptr<ItemComponent> itemComponent = hitGameObject->GetComponent<ItemComponent>();

				//取得したアイテムのイベントを発生させる
				itemComponent.lock()->ItemGetEvent();

				GameObjectManager::Instance().GetRayCastManager()->Destroy(hitGameObject);

				//ここでアイテムを消す処理を作る
				GameObjectManager::Instance().Destroy(hitGameObject->GetName());

				GameObjectManager::Instance().GetRayCastManager()->Test();

				m_hasItem++;
			}
		}

		m_RayHit = true;
	}
}

void PlayerComponent::Gui()
{
	ImGui::Checkbox("RayHit", &m_RayHit);

	ImGui::InputFloat3("HidePosition", &m_HidePosition.x);

	ImGui::InputFloat("Time", &m_Time);

	ImGui::DragFloat("Diration", &m_Diration, 0.01f, 0.0f, 5.0f);

	ImGui::Text(hitGameObjectName.c_str());
}

float PlayerComponent::EnemyLength()
{
	Vector3 enemyPos = m_pEnemy.lock()->transform->GetWorldPosition();
	Vector3 playerPos = GetGameObject()->transform ->GetWorldPosition();

	Vector3 len = enemyPos - playerPos;
	float length = len.Length();
	float divide = length /30.0f;
	return divide;
}

int PlayerComponent::GetAreaIndex()
{
	//return GameObjectManager::Instance().GetSearchAreaManager()->GetNearPoint(GetGameObject()->transform->GetWorldPosition());
	return 1;
}




void PlayerComponent::SetPlayerState(PlayerStateType state)
{
	m_PlayerState = state;
}

void PlayerComponent::IsMove()
{
	//移動処理
	if (KeyInput::KeyPressed(KeyInput::Key::K_W))
	{
		m_pMoveFlag = true;
	}

	if (KeyInput::KeyPressed(KeyInput::Key::K_A))
	{
		m_pMoveFlag = true;
	}

	if (KeyInput::KeyPressed(KeyInput::Key::K_S))
	{
		m_pMoveFlag = true;
	}

	if (KeyInput::KeyPressed(KeyInput::Key::K_D))
	{
		m_pMoveFlag = true;
	}
}

Vector3 PlayerComponent::Moving()
{

	//現在の前方向,右方向のベクトルを取得する
	Vector3 front = GetGameObject()->transform->GetFront();
	Vector3 right = GetGameObject()->transform->GetRight();
	front.y = 0;
	right.y = 0;

	Vector3 direction = {0,0,0};
	//移動処理
	if (KeyInput::KeyPressed(KeyInput::Key::K_W))
	{
		direction += front;
	}

	if (KeyInput::KeyPressed(KeyInput::Key::K_A))
	{
		direction -= right;
	}

	if (KeyInput::KeyPressed(KeyInput::Key::K_S))
	{
		direction -= front;
	}

	if (KeyInput::KeyPressed(KeyInput::Key::K_D))
	{
		direction += right;
	}

	return direction;
}

void PlayerComponent::Collider()
{
}

void PlayerComponent::IsTrigger(bool flag)
{
	m_pCapsuleColliderComponent->isTrigger = flag;
}

bool PlayerComponent::IsSneaking() const
{
	// ステートマシンの現在のステート名を確認する等の処理
	return m_pStateMachine->GetStateName() == "PlayerSneak";
}


