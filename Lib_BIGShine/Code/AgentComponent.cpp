

#include"AgentComponent.h"

#include"GameObjectManager.h"

#include"MovementComponent.h"

#include"GraphicsDeviceManager.h"

void AgentComponent::Start()
{
	dtCrowdAgentParams ap;
	memset(&ap, 0, sizeof(ap));

	//キャラクターの半径
	ap.radius = 0.5f;

	//キャラクターの高さ
	ap.height = 1.0f;

	//どれだけ早くトピスピードに達するのか
	ap.maxAcceleration = 8.0f;

	//キャラクターの最大速度
	ap.maxSpeed = 3.5;

	//他キャラを察知する範囲
	ap.collisionQueryRange = ap.radius * 12.0f;

	//経路をショートカットできるのかを見る範囲
	ap.pathOptimizationRange = ap.radius * 30.0f;

	//AIの賢さ
	ap.updateFlags = 0;
	ap.updateFlags |= DT_CROWD_ANTICIPATE_TURNS;
	ap.updateFlags |= DT_CROWD_OPTIMIZE_VIS;
	ap.updateFlags |= DT_CROWD_OPTIMIZE_TOPO;
	ap.updateFlags |= DT_CROWD_OBSTACLE_AVOIDANCE;
	ap.updateFlags |= DT_CROWD_SEPARATION;

	//回避パターンのプリセット
	ap.obstacleAvoidanceType = 3;

	//他のキャラと離れようとする強さ
	ap.separationWeight = 2.0f;

	//float p[3] = { 0,0,0 };


	Vector3 position = GetGameObject()->transform->GetWorldPosition();
	m_AgentIndex = GameObjectManager::Instance().GetNaviMeshManager()->AddAgent(&position.x, &ap);

	//移動コンポーネントを取得する
	m_pMovementComponent = GetGameObject()->GetComponent<MovementComponent>();

	//回転のタイプを指定する
	m_pMovementComponent.lock()->SetRotationType(RotationType::LookAtVelocity);

	//ナビメッシュのマネージャーにエージェントを登録する
	GameObjectManager::Instance().GetNaviMeshManager()->AddAgentComponent(this);
}

void AgentComponent::Update(float deltaTime)
{

	////自分の進行方向ベクトル
	//Vector3 direction;
	//direction.x = GameObjectManager::Instance().GetNaviMeshManager()->GetAgent(m_AgentIndex)->vel[0];
	//direction.y = GameObjectManager::Instance().GetNaviMeshManager()->GetAgent(m_AgentIndex)->vel[1];
	//direction.z = GameObjectManager::Instance().GetNaviMeshManager()->GetAgent(m_AgentIndex)->vel[2];

	////正規化する
	//direction.Normalize();

	//
	//ImGui::Begin("direction");

	//float speed = direction.Length();
	//ImGui::InputFloat3("AgentDirection", &direction.x);
	//ImGui::InputFloat("Speed", &speed);

	//ImGui::End();

	//m_pMovementComponent.lock()->AddDirection(direction);


	/*GetGameObject()->transform->SetLocalPosition(m_CurrentPosition);*/
	//float speed = GameObjectManager::Instance().GetNaviMeshManager()->GetAgent(m_AgentIndex)->desiredSpeed;
	//m_pMovementComponent.lock()->SetMoveSpeed(1);

	//目的地にたどり着いたのかを計算する処理
	Vector3 lengthVec = m_CurrentPosition - m_TargetPosition;
	if (lengthVec.Length() < m_TargetRadius)
	{
		m_ArriveFlag = true;
	}
}

//最大速度を設定する
void AgentComponent::SetMaxSpeed(float maxSpeed)
{
	dtCrowdAgent* agent = GameObjectManager::Instance().GetNaviMeshManager()->GetEditableAgent(m_AgentIndex);
	dtCrowdAgentParams param = agent->params;
	param.maxSpeed = maxSpeed;

	//パラメータ更新
	GameObjectManager::Instance().GetNaviMeshManager()->ParamUpdate(m_AgentIndex,&param);
}

//加速度を設定する
void AgentComponent::SetMaxAcceleration(float acceleration)
{
	dtCrowdAgent* agent = GameObjectManager::Instance().GetNaviMeshManager()->GetEditableAgent(m_AgentIndex);
	dtCrowdAgentParams param = agent->params;
	param.maxAcceleration = acceleration;

	//パラメータ更新
	GameObjectManager::Instance().GetNaviMeshManager()->ParamUpdate(m_AgentIndex, &param);
}

const Vector3& AgentComponent::GetDirection()
{
	//自分の進行方向ベクトル
	Vector3 direction;
	direction.x = GameObjectManager::Instance().GetNaviMeshManager()->GetAgent(m_AgentIndex)->vel[0];
	direction.y = GameObjectManager::Instance().GetNaviMeshManager()->GetAgent(m_AgentIndex)->vel[1];
	direction.z = GameObjectManager::Instance().GetNaviMeshManager()->GetAgent(m_AgentIndex)->vel[2];

	//正規化する
	//direction.Normalize();

	return direction;
}


void AgentComponent::Gui()
{
	//dtStatus status = GameObjectManager::Instance().GetNaviMeshManager()->GetAgent(m_AgentIndex)->state();
	ImGui::DragFloat("TargetRadius",&m_TargetRadius,0.5f,10.0f);

	m_pAgent = GameObjectManager::Instance().GetNaviMeshManager()->GetAgent(m_AgentIndex);
	
	// もっと分かりやすくするなら（文字列変換）
	const char* stateName = "Unknown";
	switch (m_pAgent->state) {
	case DT_CROWDAGENT_STATE_INVALID: stateName = "Invalid"; break;
	case DT_CROWDAGENT_STATE_WALKING: stateName = "Walking"; break;
	case DT_CROWDAGENT_STATE_OFFMESH: stateName = "OffMesh"; break;
	}
	ImGui::Text("State: %s (%d)", stateName, m_pAgent->state);

	ImGui::Text("Target State: %d", m_pAgent->targetState);

	if (m_pAgent->targetState == DT_CROWDAGENT_TARGET_NONE) {
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "STOPPED (No Target)");
	}
	else if (m_pAgent->targetState == DT_CROWDAGENT_TARGET_VALID) {
		ImGui::TextColored(ImVec4(0, 1, 0, 1), "MOVING (Target Set)");
	}


	dtCrowdAgent* agent = GameObjectManager::Instance().GetNaviMeshManager()->GetEditableAgent(m_AgentIndex);
	ImGui::DragFloat("MaxSpeed", &agent->params.maxSpeed);
	ImGui::DragFloat("MaxAcceleration", &agent->params.maxAcceleration);
	//ImGui::DragFloat("Radius", &m_Radius,0.0f,1.0f);
	//ImGui::DragFloat("Height",&m_Height,0.0f,2.0f);
	//ImGui::DragFloat("MaxAcceleration",&m_MaxAcceleration,0.0f,10.0f);
	//ImGui::DragFloat("MaxSpeed",&m_MaxSpeed, 0.1f, 10.0f);
	//ImGui::DragFloat("CollisionQueryRange",&m_CollisionQueryRange,0.0f,m_Radius * 10.0f);
	//ImGui::DragFloat("PathOpimaizationRange",&m_PathOptimizationRange,10.0f,m_Radius * 12.0f);
	//ImGui::InputFloat("ObstacleAcoidanceType",&m_ObstacleAvoidanceType);
	//ImGui::DragFloat("SaparationWeigth",&m_SaparationWeight,0.0f,5.0f);

	//if (ImGui::Button("AgentReset"))
	//{
	//	dtCrowdAgentParams ap;
	//	memset(&ap, 0, sizeof(ap));

	//	//キャラクターの半径
	//	ap.radius = m_Radius;

	//	//キャラクターの高さ
	//	ap.height = m_Height;

	//	//どれだけ早くトピスピードに達するのか
	//	ap.maxAcceleration = m_MaxAcceleration;

	//	//キャラクターの最大速度
	//	ap.maxSpeed = m_MaxSpeed;

	//	//他キャラを察知する範囲
	//	ap.collisionQueryRange = m_CollisionQueryRange;

	//	//経路をショートカットできるのかを見る範囲
	//	ap.pathOptimizationRange = m_PathOptimizationRange;

	//	//AIの賢さ
	//	ap.updateFlags = 0;
	//	ap.updateFlags |= DT_CROWD_ANTICIPATE_TURNS;
	//	ap.updateFlags |= DT_CROWD_OPTIMIZE_VIS;
	//	ap.updateFlags |= DT_CROWD_OPTIMIZE_TOPO;
	//	ap.updateFlags |= DT_CROWD_OBSTACLE_AVOIDANCE;
	//	ap.updateFlags |= DT_CROWD_SEPARATION;

	//	//回避パターンのプリセット
	//	ap.obstacleAvoidanceType = m_ObstacleAvoidanceType;

	//	//他のキャラと離れようとする強さ
	//	ap.separationWeight = m_SaparationWeight;

	//	float p[3] = { 0,0,0 };
	//}


	int state = GameObjectManager::Instance().GetNaviMeshManager()->GetAgent(m_AgentIndex)->state;


	Vector4 color = { 1,1,1,1 };
	GraphicsDeviceManager::Instance().GetShapeRendererSystem().DrawSphere(m_TargetPosition, m_TargetRadius, color);

	color = { 1,0,0,1 };
	GraphicsDeviceManager::Instance().GetShapeRendererSystem().DrawSphere(m_TestPosition, m_TargetRadius, color);

	ImGui::InputInt("AgentState", &state);
	float speed = GameObjectManager::Instance().GetNaviMeshManager()->GetAgent(m_AgentIndex)->desiredSpeed;


	ImGui::InputFloat("Speed", &speed);
	ImGui::Checkbox("Arrive", &m_ArriveFlag);

	ImGui::InputFloat3("TestPosition", &m_TestPosition.x);
	ImGui::InputFloat3("TargetPosition", &m_TargetPosition.x);
	ImGui::InputFloat3("CurrentPosition", &m_CurrentPosition.x);
}
//目標地点をランダムで決める
bool AgentComponent::SetRandomTargetPosition(const Vector3& position, float radius)
{
	m_TestPosition = position;
	NaviMeshManager* naviMeshManager = GameObjectManager::Instance().GetNaviMeshManager();
	m_TargetPosition = naviMeshManager->SetRandomPosition(m_AgentIndex, m_TestPosition, radius);

	//目的地に到着したか否か
	m_ArriveFlag = false;

	//もしもターゲット範囲内なら処理ならビヘイビアのステートを変更させない
	Vector3 length = m_TargetPosition - m_TestPosition;
	float l = length.Length();
	if (length.Length() < m_TargetRadius)
	{
		GameObjectManager::Instance().GetNaviMeshManager()->Reset(m_AgentIndex);
		return false;
	}


	Vector4 color = { 1,1,1,1 };
}

//目標地点をセットする
void AgentComponent::SetTargetPosition(const Vector3& position)
{
	Vector3 pos = position;

	//目的地に到着したか否か
	m_ArriveFlag = false;


	NaviMeshManager* naviMeshManager = GameObjectManager::Instance().GetNaviMeshManager();
	m_TargetPosition = naviMeshManager->SetTargetPosition(m_AgentIndex, pos);
}

//目標のスピードを再設定する
void AgentComponent::RequestMoveVelocity(float speed)
{
	NaviMeshManager* naviMeshManager = GameObjectManager::Instance().GetNaviMeshManager();
	naviMeshManager->RequestMoveVelocity(m_AgentIndex, speed);
}

//目標地点をリセットする
void AgentComponent::Reset()
{
	NaviMeshManager* naviMeshManager = GameObjectManager::Instance().GetNaviMeshManager();
	naviMeshManager->Reset(m_AgentIndex);

}

bool AgentComponent::TargetStateCheck(MoveRequestState state)
{
	m_pAgent = GameObjectManager::Instance().GetNaviMeshManager()->GetAgent(m_AgentIndex);

	if (m_pAgent->targetState == state)
	{
		return true;
	}
	return false;
}

//bool AgentComponent::SetTargetPosition(const Vector3& position,float radius)
//{
//
//	//m_TestPosition = position;
//	//NaviMeshManager* naviMeshManager = GameObjectManager::Instance().GetNaviMeshManager();
//	//m_TargetPosition = naviMeshManager->SetTargetPosition(m_AgentIndex, m_TestPosition, radius);
//
//	//m_ArriveFlag = false;
//
//	////もしもターゲット範囲内なら処理ならビヘイビアのステートを変更させない
//	//Vector3 length = m_TargetPosition - m_TestPosition;
//	//float l = length.Length();
//	//if (length.Length() < m_TargetRadius)
//	//{
//	//	GameObjectManager::Instance().GetNaviMeshManager()->Reset(m_AgentIndex);
//	//	return false;
//	//}
//
//
//	//Vector4 color = { 1,1,1,1 };
//}

