#pragma once

#include"JudgmentBase.h"
class AgentComponent;
class EnemyComponent;

class WanderJudgment :public JudgmentBase
{
public:
	WanderJudgment(GameObject* gameObject) :JudgmentBase(gameObject){}
	bool Judgment();

private:
	std::weak_ptr<AgentComponent> m_pAgentComponent;
};

class ChaseJudgment :public JudgmentBase
{
public:
	ChaseJudgment(GameObject*gameObject):JudgmentBase(gameObject){}
	bool Judgment();

private:
	std::weak_ptr<EnemyComponent> m_pEnemyComponent;
};


class AttackJudgment :public JudgmentBase
{
public:
	AttackJudgment(GameObject* gameObject) :JudgmentBase(gameObject) {}
	bool Judgment();

private:
	std::weak_ptr<EnemyComponent> m_pEnemyComponent;
};

class CautionJudgment :public JudgmentBase
{
public:
	CautionJudgment(GameObject* gameObject) :JudgmentBase(gameObject) {}

	bool Judgment();

private:
	std::weak_ptr<EnemyComponent> m_pEnemyComponent;
};



class CheckJudgment :public JudgmentBase
{
public:
	CheckJudgment(GameObject* gameObject) :JudgmentBase(gameObject) {}

	bool Judgment();

private:
	std::weak_ptr<EnemyComponent> m_pEnemyComponent;
};

