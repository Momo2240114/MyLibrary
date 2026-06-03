

#include"JudgmentDerived.h"
#include"GameObject.h"


#include"AgentComponent.h"
#include"EnemyComponent.h"

bool WanderJudgment::Judgment()
{
	//目的地点まで到着していなかったとき
	m_pAgentComponent = m_pOwner->GetComponent<AgentComponent>();

	//目的地についていなければ
	if (!m_pAgentComponent.lock()->GetIsArrive() && m_pAgentComponent.lock()->TargetStateCheck(DT_CROWDAGENT_TARGET_VALID))
	{
		return true;
	}
	return false;
}


bool ChaseJudgment::Judgment()
{
	//目的地点まで到着していなかったとき
	m_pEnemyComponent = m_pOwner->GetComponent<EnemyComponent>();

	//プレイヤーを視認できてかつプレイヤーがかくれていなければ
	if (m_pEnemyComponent.lock()->Visibility() &&!m_pEnemyComponent.lock()->StateCheck(PlayerStateType::Hide))
	{
		return true;
	}
	return false;
}


bool AttackJudgment::Judgment()
{
	//目的地点まで到着していなかったとき
	m_pEnemyComponent = m_pOwner->GetComponent<EnemyComponent>();

	//攻撃可能状態にあれば
	//if (m_pEnemyComponent.lock()->GetAttackFlag())
	//{
	//	return true;
	//}

	if (m_pEnemyComponent.lock()->AttackRange() && !m_pEnemyComponent.lock()->StateCheck(PlayerStateType::Hide))
	{
		return true;
	}
	return false;
}

bool CautionJudgment::Judgment()
{
	m_pEnemyComponent = m_pOwner->GetComponent<EnemyComponent>();

	if (m_pEnemyComponent.lock()->Caution())
	{
		return true;
	}
	return false;
}


bool CheckJudgment::Judgment()
{
	m_pEnemyComponent = m_pOwner->GetComponent<EnemyComponent>();

	//壁と隠れるオブジェクト
	uint32_t layerMask = (uint32_t)GameObject::LayerType::Wall;

	if (m_pEnemyComponent.lock()->GetPlayerSeeFlag())
	{
		return true;
	}
	return false;
}

