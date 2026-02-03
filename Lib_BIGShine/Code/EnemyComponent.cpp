#include "EnemyComponent.h"
#include"GameObjectManager.h"

#include"IScene.h"
#include"KeyInput.h"

#include <fstream>

#include"BehaviorData.h"
#include"BehaviorTree.h"
#include"NodeBase.h"

#include"JudgmentDerived.h"
#include"ActionDerived.h"
#include"AnimationController.h"

#include"GraphicsDeviceManager.h"
#include"CapsuleCollider.h"
#include"PlayerComponent.h"

#include"SphereCollider.h"

enum class EnemyStateType
{
	Idle,
	Walk,
	Run,
	Attack,
};

void EnemyComponent::Start()
{
	// すでにあれば取得、なければ追加
	m_pMovementComponent = GetGameObject()->GetOrAddComponent<MovementComponent>();
	m_pAgentComponent = GetGameObject()->GetOrAddComponent<AgentComponent>(false);
	m_pAnimationComponent = GetGameObject()->GetOrAddComponent<AnimationController>();
	m_pCapsuleCollider = GetGameObject()->GetOrAddComponent<CapsuleCollider>();
	
	m_pMovementComponent->SetTurnSpeed(200);
	//エネミーの重さ
	m_pCapsuleCollider->m_Mass = 1.0f;

	m_pPlayerGameObject = GameObjectManager::Instance().Find("MainCamera").get();

	//ビヘイビアツリー
	m_pBehaviorData = new BehaviorData();
	m_pBehaviorTree = new BehaviorTree(GetGameObject().get());
	
	m_pBehaviorTree->AddNode("","Root",0,BehaviorTree::SelectRule::Priority,nullptr,nullptr);
	
	//サーチの中間ノード
	m_pBehaviorTree->AddNode("Root", "Search", 3, BehaviorTree::SelectRule::Priority, nullptr, nullptr);
	

	//確認する中間ノード
	m_pBehaviorTree->AddNode("Search", "Check", 0, BehaviorTree::SelectRule::Sequence, new CheckJudgment(GetGameObject().get()), nullptr);

	//スニークオブジェクトまで移動
	m_pBehaviorTree->AddNode("Check", "CheckPointMove", 1, BehaviorTree::SelectRule::None, nullptr, new CheckPointMoveAction(GetGameObject().get()));

	//しゃがむ
	m_pBehaviorTree->AddNode("Check", "Peek", 1, BehaviorTree::SelectRule::None, nullptr, new PeekAction(GetGameObject().get()));

	//キルモーション
	m_pBehaviorTree->AddNode("Check", "Peek", 1, BehaviorTree::SelectRule::None, nullptr, new KillMotionAction(GetGameObject().get()));


	//サーチ状態の末端ノード
	m_pBehaviorTree->AddNode("Search", "Caution", 1, BehaviorTree::SelectRule::None, new CautionJudgment(GetGameObject().get()), new CautionAction(GetGameObject().get()));
	m_pBehaviorTree->AddNode("Search", "Wander", 2, BehaviorTree::SelectRule::None, new WanderJudgment(GetGameObject().get()), new WanderAction(GetGameObject().get()));
	m_pBehaviorTree->AddNode("Search", "Idle", 3, BehaviorTree::SelectRule::None, nullptr, new IdleAction(GetGameObject().get()));

	//チェイスの中間ノード
	m_pBehaviorTree->AddNode("Root", "Chase", 2, BehaviorTree::SelectRule::Priority, new ChaseJudgment(GetGameObject().get()), nullptr);

	//チェイス状態の末端ノード
	m_pBehaviorTree->AddNode("Chase", "Attack", 1, BehaviorTree::SelectRule::None, new AttackJudgment(GetGameObject().get()), new AttackAction(GetGameObject().get()));
	m_pBehaviorTree->AddNode("Chase", "Run", 2, BehaviorTree::SelectRule::None, nullptr, new ChaseAction(GetGameObject().get()));


	m_pAttachimentGameObject = GameObjectManager::Instance().Find("Attachiment");
}

void EnemyComponent::Update(float et)
{
	//static int count = 0;
	//static Vector3 player = { 0,0,0 };
	//if (m_TestFlag)
	//{
	//	if (count == 0)
	//	{
	//		player = GetTargetPosition();
	//		count = 1;
	//	}
	//	Vector3 enemy = GetGameObject()->transform->GetWorldPosition();
	//	Vector3 vector = player - enemy;

	//	Vector3 front = GetGameObject()->transform->GetFront();
	//	vector.y = 0;
	//	front.y = 0;

	//	vector.Normalize();
	//	front.Normalize();

	//	float dot = vector.Dot(front);

	//	if (dot > 0.99)
	//	{
	//		m_TestFlag = false;
	//		player = { 0,0,0 };
	//		count = 0;
	//	}
	//	m_pMovementComponent->AddDirection(vector);
	//	m_pMovementComponent->SetMoveSpeed(0);

	//}

	//現在実行されているノードがなければ
	if (m_pActiveNode == nullptr)
	{
		//次に実行するノードを推論する
		m_pActiveNode = m_pBehaviorTree->ActiveNodeInference(m_pBehaviorData);
	}

	//現在実行するノードがあれば
	if (m_pActiveNode != nullptr)
	{
		m_pActiveNode = m_pBehaviorTree->Run(m_pActiveNode,m_pBehaviorData,et);
	}
	 
	//ナビメッシュをもとにエネミーを動かすとき
	if (m_UseNavMeshFlag)
	{
		//ここでマネージャーを更新する必要がある
		GameObjectManager::Instance().GetNaviMeshManager()->Update(et);

		//進行ベクトルを取得する
		Vector3 direction = m_pAgentComponent->GetDirection();

		float speed = direction.Length();
		m_pMovementComponent->SetMoveSpeed(speed);

		//進行ベクトルを加算させる
		m_pMovementComponent->AddDirection(direction);
	}

	Vector4 color = { 0,1,0,1 };
	GraphicsDeviceManager::Instance().GetShapeRendererSystem().DrawSphere(GetGameObject()->transform->GetWorldPosition(), m_VisibilityRadius, color);
	color = { 0,0,1,1 };
	GraphicsDeviceManager::Instance().GetShapeRendererSystem().DrawSphere(GetGameObject()->transform->GetWorldPosition(), m_AttackRadius, color);

	color = { 0,0,0,1 };
	GraphicsDeviceManager::Instance().GetShapeRendererSystem().DrawSphere(GetGameObject()->transform->GetWorldPosition(), m_CautionRadius, color);

	//-----------------------------------------------------------//
	//// --- ターゲットの自動再接続ロジック ---
	//if (!m_pPlayerGameObject && !m_targetObjectName.empty())
	//{
	//	// 名前を頼りにマネージャーから検索
	//	auto obj = GameObjectManager::Instance().Find(m_targetObjectName);
	//	if (obj)
	//	{
	//		m_pPlayerGameObject = obj.get();
	//	}
	//}


	//m_Input = false;
	////テスト用
	//if (KeyInput::KeyPressedIn(KeyInput::Key::K_B))
	//{
	//	m_pAgentComponent->SetTargetPosition(m_pPlayerGameObject->transform->GetWorldPosition(),0.0f);
	//	m_Input = true;
	//}

	//GetGameObject()->transform->localPosition = m_pAgentComponent->GetCurrentPosition();
}

EnemyComponent::~EnemyComponent()
{
	delete m_pBehaviorTree;
	delete m_pBehaviorData;

	m_pBehaviorTree = nullptr;
	m_pBehaviorData = nullptr;
}


void EnemyComponent::Gui()
{
	ImGui::Checkbox("K_Alt",&m_Input);
	bool visiblity = Visibility();
	ImGui::Checkbox("Visiblity", &visiblity);

	Vector3 playerPos = m_pPlayerGameObject->transform->GetWorldPosition();
	Vector3 myPos = GetGameObject()->transform->GetWorldPosition();
	Vector3 length = playerPos - myPos;
	float len = length.Length();
	ImGui::InputFloat("PlayerDistance",&len);

	if (m_pActiveNode)
	{
		std::string actionName = m_pActiveNode->GetName();
		ImGui::Text(actionName.c_str());
	}

	ImGui::DragFloat("Fov",&m_Fov,0.01f,-1.0,1.0f);
	ImGui::DragFloat("SearchRadius", &m_VisibilityRadius, 0.1f, 1.0f, 30.0f);
	ImGui::DragFloat("AttackRadius", &m_AttackRadius, 0.1f, 0.0f, 2.0f);

	ImGui::Checkbox("IsPlayerSee", &m_PlayerSeeFlag);

	ImGui::Text( SneakObjectName.c_str());

	ImGui::Checkbox("TestFlag", &m_TestFlag);

}


void EnemyComponent::Serialize(nlohmann::json& j) const
{
	// コンポーネントへのポインタは保存せず、
	// エネミー固有の「設定値」だけを保存する
	if (m_pPlayerGameObject) {
		j["TargetObjectName"] = m_pPlayerGameObject->GetName();
	}
}

bool EnemyComponent::Visibility()
{
	Vector3 playerPos = m_pPlayerGameObject->transform->GetWorldPosition();
	Vector3 myPos = GetGameObject()->transform->GetWorldPosition();

	//プレイヤーまでのベクトルを求める
	Vector3 length = playerPos - myPos;
	length.y = 0;

	//前方向ベクトルを取得する
	Vector3 front = GetGameObject()->transform->GetFront();
	front.y = 0;

	if (length.Length() < m_VisibilityRadius)
	{
		//ベクトルを正規化する
		front.Normalize();
		length.Normalize();

		//エネミーの前方向のにいるとき(度数)
		if (DirectX::XMConvertToDegrees(acosf(front.Dot(length))) <= m_Fov/2.0f)
		{
			return true;
		}
	}
	return false;

}

bool EnemyComponent::AttackRange()
{
	Vector3 playerPos = m_pPlayerGameObject->transform->GetWorldPosition();
	Vector3 myPos = GetGameObject()->transform->GetWorldPosition();

	//高さを合わせて正確な距離を求める
	playerPos.y = 0;
	myPos.y = 0;


	Vector3 length = playerPos - myPos;

	if (length.Length() < m_AttackRadius)
	{
		return true;
	}
	return false;

}

void EnemyComponent::PlayerChase()
{
	//static int start = 0;
	//if (start == 0)
	//{
	//	m_OldPlayerPos = m_pPlayerGameObject->transform->GetWorldPosition();
	//}
	//Vector3 playerPos = m_pPlayerGameObject->transform->GetWorldPosition();
	//float radius = m_pAgentComponent->GetTargetRadius();

	//Vector3 length = m_OldPlayerPos - playerPos;

	//if (length.Length() > m_AttackRadius)
	//{
	//	m_pAgentComponent->SetTargetPosition(playerPos);
	//	m_OldPlayerPos = playerPos;
	//}
	//start = 1;



	Vector3 playerPos = m_pPlayerGameObject->transform->GetWorldPosition();
	float radius = m_pAgentComponent->GetTargetRadius();

	Vector3 length = m_OldPlayerPos - playerPos;

	m_pAgentComponent->SetTargetPosition(playerPos);
	m_OldPlayerPos = playerPos;
}


bool EnemyComponent::IsSeePlayer(uint32_t layerMask)
{
	//自分の位置を取得する
	Vector3 start = GetGameObject()->transform->GetWorldPosition();

	//プレイヤーの位置を取得する
	Vector3 end = GetTargetPosition();

	Vector3 hitPosition;
	Vector3 hitNormal;

	//自分とプレイヤーの間に障害物がなければ
	if (GameObjectManager::Instance().GetRayCastManager()->RayCast(start, end, layerMask, hitPosition, hitNormal) == nullptr)
	{
		m_PlayerSeeFlag = true;
	}
	else
	{
		m_PlayerSeeFlag = false;
	}
	return m_PlayerSeeFlag;
}



bool EnemyComponent::HitPlayer()
{
	for (std::weak_ptr<GameObject> gameObject : m_pAttachimentGameObject.lock()->GetComponent<SphereCollider>()->currentTriggers)
	{
		if (gameObject.lock()->GetName() == m_pPlayerGameObject->GetName())
		{
			return true;
		}
	}
	return false;
}

const Vector3& EnemyComponent::GetHidePosition()
{
	return m_pPlayerGameObject->GetComponent<PlayerComponent>()->GetHidePosition();
}

void EnemyComponent::Deserialize(const nlohmann::json& j)
{
	if (j.contains("ModelName")) {
		m_targetObjectName = j["TargetObjectName"];
	}
}

bool EnemyComponent::StateCheck(PlayerStateType  state)
{
	std::weak_ptr<PlayerComponent> PlayerCompoponent = m_pPlayerGameObject->GetComponent<PlayerComponent>();
	if (state == PlayerCompoponent.lock()->GetPlayerState())
	{
		return true;
	}
	return false;
}

//警戒
bool EnemyComponent::Caution()
{
	std::weak_ptr<PlayerComponent> m_pPlayerComponent = m_pPlayerGameObject->GetComponent<PlayerComponent>();
	if (StateCheck(PlayerStateType::Run))
	{
		Vector3 playerPos = m_pPlayerGameObject->transform->GetWorldPosition();
		float radius = m_pAgentComponent->GetTargetRadius();

		Vector3 length = GetGameObject()->transform->GetWorldPosition() - playerPos;

		if (length.Length() < m_CautionRadius)
		{
			return true;
		}
	}
	return false;
}


void  EnemyComponent::LateUpdate(float et)
{
	//if (m_pCapsuleCollider->m_PushOut)
	//{
	//	Vector3 agentPos = m_pAgentComponent->GetCurrentPosition();
	//	GetGameObject()->transform->SetLocalPosition(agentPos);
	//}
}
